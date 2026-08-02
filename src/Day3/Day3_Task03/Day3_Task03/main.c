/*
 * dynamixel_pot_pc_lcd.c
 *
 * ATmega128 + MAX485 + Dynamixel Protocol 2.0
 *
 *  1) 가변저항(ADC0 / PF0) 값(0~1023) -> 다이나믹셀 목표 위치(Goal Position)
 *  2) PC에서 받은 문자 '0'~'9'        -> 다이나믹셀 목표 속도(Profile Velocity)
 *  3) I2C LCD 1행 : 목표 속도 / 2행 : 목표 위치
 *
 * ---------------------------- 하드웨어 연결 ----------------------------
 *  UART0 : PE0 = RXD0(MAX485 RO), PE1 = TXD0(MAX485 DI)  -> 다이나믹셀 57600bps
 *  방향제어 : PE2 (MAX485 RE + DE 묶음)  HIGH = 송신 / LOW = 수신
 *  UART1 : PD2 = RXD1, PD3 = TXD1                        -> PC 9600bps
 *  I2C   : PD0 = SCL,  PD1 = SDA                         -> PCF8574 LCD(0x27)
 *  ADC0  : PF0                                           -> 가변저항
 *  (스위치 SW0 = PE4 / SW1 = PE5 : 이번 과제에서는 사용 안 함)
 *
 *  Dynamixel ID = 1, Protocol 2.0 (MX-64 2.0 / X시리즈 컨트롤 테이블 기준)
 * -----------------------------------------------------------------------
 */

#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#include "i2c.h"
#include "clcd.h"

/* ======================= 사용자 설정 ======================= */
#define BAUD_DXL   57600UL                                       /* UART0 : 다이나믹셀 */
#define BAUD_PC    9600UL                                        /* UART1 : PC        */
#define UBRR_DXL   (((F_CPU) + 4UL*(BAUD_DXL)) / (8UL*(BAUD_DXL)) - 1)  /* U2X 기준 = 34  */
#define UBRR_PC    (((F_CPU) + 4UL*(BAUD_PC))  / (8UL*(BAUD_PC))  - 1)  /* U2X 기준 = 207 */

#define DXL_ID                 1
#define ADDR_OPERATING_MODE    11    /* 1byte : 3 = 위치 제어 모드 */
#define ADDR_TORQUE_ENABLE     64    /* 1byte */
#define ADDR_PROFILE_VELOCITY  112   /* 4byte : 목표(프로파일) 속도 */
#define ADDR_GOAL_POSITION     116   /* 4byte : 목표 위치 */

#define VEL_MAX   300UL   /* PC 입력 '9' 에 대응하는 속도값 */
#define ADC_DEAD  4       /* 가변저항 잡음 무시 폭 (이만큼 변해야 갱신) */

/* RS485 방향제어 (MAX485 RE/DE 묶음 -> PE2) */
#define DIR_DDR   DDRE
#define DIR_PORT  PORTE
#define DIR_PIN   PE2
#define DIR_TX()  (DIR_PORT |=  (1 << DIR_PIN))
#define DIR_RX()  (DIR_PORT &= ~(1 << DIR_PIN))

/* =================== Dynamixel CRC-16 (poly 0x8005) ===================
 * 지난번 코드의 256개짜리 PROGMEM 테이블과 결과가 완전히 동일한 비트 연산 버전.
 * 플래시 512바이트를 아끼고, 테이블 오타 위험이 없어서 이걸로 바꿨어.
 */
uint16_t update_crc(uint16_t crc_accum, uint8_t *data, uint16_t size)
{
	uint16_t j;
	uint8_t  i;

	for (j = 0; j < size; j++)
	{
		crc_accum ^= ((uint16_t)data[j] << 8);
		for (i = 0; i < 8; i++)
		{
			if (crc_accum & 0x8000)
				crc_accum = (crc_accum << 1) ^ 0x8005;
			else
				crc_accum = (crc_accum << 1);
		}
	}
	return crc_accum;
}

/* ========================= UART0 : 다이나믹셀 ========================= */
void UART0_Init(void)
{
	UBRR0H = (uint8_t)(UBRR_DXL >> 8);
	UBRR0L = (uint8_t)(UBRR_DXL);

	UCSR0A = (1 << U2X0);                    /* 배속 모드 (오차 0.79%) */
	UCSR0B = (1 << TXEN0) | (1 << RXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);  /* 8N1 */
}

void UART0_Tx(uint8_t data)
{
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = data;
}

/* ============================ UART1 : PC ============================ */
void UART1_Init(void)
{
	UBRR1H = (uint8_t)(UBRR_PC >> 8);
	UBRR1L = (uint8_t)(UBRR_PC);

	UCSR1A = (1 << U2X1);
	UCSR1B = (1 << TXEN1) | (1 << RXEN1);
	UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);  /* 8N1 */
}

void UART1_Tx(uint8_t data)
{
	while (!(UCSR1A & (1 << UDRE1)));
	UDR1 = data;
}

/* 수신 문자가 있으면 1, 없으면 0 (블로킹 안 함) */
uint8_t UART1_Available(void)
{
	return (UCSR1A & (1 << RXC1)) ? 1 : 0;
}

uint8_t UART1_Rx(void)
{
	return UDR1;
}

/* ===================== Dynamixel 패킷 송신 (WRITE) ===================== */
void dxl_write(uint8_t id, uint16_t addr, uint8_t *data, uint8_t data_len)
{
	uint8_t  packet[20];
	uint8_t  idx = 0;
	uint16_t length = (uint16_t)data_len + 5;  /* instruction(1) + addr(2) + data + crc(2) */
	uint16_t crc;
	uint8_t  i;

	packet[idx++] = 0xFF;
	packet[idx++] = 0xFF;
	packet[idx++] = 0xFD;
	packet[idx++] = 0x00;                    /* Reserved */
	packet[idx++] = id;
	packet[idx++] = (uint8_t)(length & 0xFF);
	packet[idx++] = (uint8_t)(length >> 8);
	packet[idx++] = 0x03;                    /* Instruction : WRITE */
	packet[idx++] = (uint8_t)(addr & 0xFF);
	packet[idx++] = (uint8_t)(addr >> 8);

	for (i = 0; i < data_len; i++)
		packet[idx++] = data[i];

	crc = update_crc(0, packet, idx);
	packet[idx++] = (uint8_t)(crc & 0xFF);
	packet[idx++] = (uint8_t)(crc >> 8);

	/* ---- RS485 반이중 송신 ---- */
	DIR_TX();
	_delay_us(10);                  /* 방향 전환 안정화 */
	UCSR0A |= (1 << TXC0);          /* TXC 플래그 클리어(1을 써서 지움) */

	for (i = 0; i < idx; i++)
		UART0_Tx(packet[i]);

	while (!(UCSR0A & (1 << TXC0)));  /* 마지막 비트까지 실제로 나갈 때까지 대기 */
	DIR_RX();                         /* 다시 수신 모드 */
}

void dxl_write1(uint8_t id, uint16_t addr, uint8_t value)
{
	dxl_write(id, addr, &value, 1);
}

void dxl_write4(uint8_t id, uint16_t addr, uint32_t value)
{
	uint8_t d[4];
	d[0] = (uint8_t)(value);
	d[1] = (uint8_t)(value >> 8);
	d[2] = (uint8_t)(value >> 16);
	d[3] = (uint8_t)(value >> 24);
	dxl_write(id, addr, d, 4);
}

/* ============================== ADC ============================== */
void ADC_Init(void)
{
	DDRF  &= ~(1 << PF0);   /* PF0 입력 */
	PORTF &= ~(1 << PF0);   /* 내부 풀업 off (아날로그 입력) */

	ADMUX  = (1 << REFS0);  /* 기준전압 = AVCC, 채널 = ADC0 */
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); /* 분주 128 -> 125kHz */
}

uint16_t ADC_Read(void)
{
	ADCSRA |= (1 << ADSC);              /* 변환 시작 */
	while (ADCSRA & (1 << ADSC));       /* 변환 완료까지 대기 */
	return ADC;                         /* ADCL/ADCH 자동 처리 */
}

/* ===================== LCD 숫자 출력 (우측 정렬) =====================
 * clcd.c 에는 정수 출력 함수가 없어서 직접 만들었어.
 * 라이브러리 파일은 손대지 않고 i2c_lcd_data() 만 사용해.
 */
void lcd_write_num(uint16_t value, uint8_t width)
{
	char    buf[6];
	uint8_t i = 0;

	if (value == 0)
		buf[i++] = '0';

	while (value > 0)
	{
		buf[i++] = (char)('0' + (value % 10));
		value /= 10;
	}

	while (i < width)       /* 앞쪽을 공백으로 채워 자리수 고정 (잔상 방지) */
		buf[i++] = ' ';

	while (i > 0)
		i2c_lcd_data((uint8_t)buf[--i]);
}

/* =============================== main =============================== */
int main(void)
{
	uint16_t adc_now;
	uint16_t adc_last = 0xFFFF;   /* 첫 루프에서 무조건 갱신되도록 */
	uint16_t goal_pos = 0;
	uint16_t goal_vel = 0;
	uint8_t  need_lcd = 1;
	uint8_t  rx;

	/* --- 포트 설정 --- */
	DIR_DDR |= (1 << DIR_PIN);    /* PE2 출력 */
	DIR_RX();                     /* 기본 수신 모드 */

	DDRE |= (1 << PE1);           /* TXD0 출력 */
	DDRE &= ~(1 << PE0);          /* RXD0 입력 */

	DDRD |= (1 << PD3);           /* TXD1 출력 */
	DDRD &= ~(1 << PD2);          /* RXD1 입력 */
	/* PD0/PD1 은 건드리지 않음 (i2c_init 이 TWI용으로 설정) */

	UART0_Init();
	UART1_Init();
	ADC_Init();

	i2c_lcd_init();               /* 내부에서 i2c_init() 호출됨 */
	_delay_ms(50);

	i2c_lcd_goto_XY(0, 0);
	i2c_lcd_write_string("Vel:");
	i2c_lcd_goto_XY(1, 0);
	i2c_lcd_write_string("Pos:");

	_delay_ms(500);               /* 다이나믹셀 전원 안정화 */

	/* --- 다이나믹셀 초기 설정 --- */
	dxl_write1(DXL_ID, ADDR_TORQUE_ENABLE, 0);     /* EEPROM 쓰려면 토크 OFF */
	_delay_ms(20);
	dxl_write1(DXL_ID, ADDR_OPERATING_MODE, 3);    /* 3 = 위치 제어 모드 */
	_delay_ms(20);
	dxl_write1(DXL_ID, ADDR_TORQUE_ENABLE, 1);     /* 토크 ON */
	_delay_ms(20);

	dxl_write4(DXL_ID, ADDR_PROFILE_VELOCITY, (uint32_t)goal_vel);

	while (1)
	{
		/* ---------- 2) PC 입력 -> 목표 속도 ---------- */
		if (UART1_Available())
		{
			rx = UART1_Rx();

			if (rx >= '0' && rx <= '9')
			{
				goal_vel = (uint16_t)(((uint32_t)(rx - '0') * VEL_MAX) / 9);
				dxl_write4(DXL_ID, ADDR_PROFILE_VELOCITY, (uint32_t)goal_vel);
				need_lcd = 1;
			}
			UART1_Tx(rx);   /* 에코 : 터미널에 글자가 되돌아오면 통신 정상 */
		}

		/* ---------- 1) 가변저항 -> 목표 위치 ---------- */
		adc_now = ADC_Read();

		if ((adc_now > adc_last && (adc_now - adc_last) >= ADC_DEAD) ||
		    (adc_last > adc_now && (adc_last - adc_now) >= ADC_DEAD) ||
		    adc_last == 0xFFFF)
		{
			adc_last = adc_now;
			goal_pos = adc_now;                                   /* 0~1023 그대로 사용 */
			dxl_write4(DXL_ID, ADDR_GOAL_POSITION, (uint32_t)goal_pos);
			need_lcd = 1;
		}

		/* ---------- 3) LCD 출력 ---------- */
		if (need_lcd)
		{
			need_lcd = 0;

			i2c_lcd_goto_XY(0, 4);
			lcd_write_num(goal_vel, 4);   /* 1행 : 목표 속도 */

			i2c_lcd_goto_XY(1, 4);
			lcd_write_num(goal_pos, 4);   /* 2행 : 목표 위치 */
		}

		_delay_ms(20);
	}
}