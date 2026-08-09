/*
 * IR 수발광부 6채널 ADC + MAF 필터 + Min/Max + 정규화
 * + LED 제어 (norm >= 0.80 이면 ON)
 * + I2C LCD 정규화 값 표시
 *
 * MCU  : ATmega128 (16MHz)
 * IR   : PF2 ~ PF7  (ADC2 ~ ADC7, 6채널)
 * UART : PD2(RXD1) / PD3(TXD1), 57600bps 8N1
 * LCD  : PD0(SCL)  / PD1(SDA),  I2C PCF8574T (0x27)
 * LED  : PA1 ~ PA6 (IR 0~5 대응)
 */

#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#include "i2c.h"
#include "clcd.h"

/* ---------- 설정 ---------- */
#define BAUD          57600UL
#define UBRR_VALUE    ((F_CPU / (8UL * BAUD)) - 1)

#define IR_START_CH   2        /* ADC2 = PF2 */
#define IR_CH_COUNT   6        /* PF2 ~ PF7  */
#define MAF_SIZE      8

#define NORM_THRESHOLD 80      /* 0.80 = 80/100 */

/* ---------- 전역 변수 ---------- */
uint16_t maf_buf[IR_CH_COUNT][MAF_SIZE];
uint8_t  maf_idx[IR_CH_COUNT];
uint32_t maf_sum[IR_CH_COUNT];

uint16_t ir_min[IR_CH_COUNT];
uint16_t ir_max[IR_CH_COUNT];

uint16_t norm_x100[IR_CH_COUNT];

/* ---------- UART1 ---------- */
void UART1_init(void)
{
    UBRR1H = (unsigned char)(UBRR_VALUE >> 8);
    UBRR1L = (unsigned char)(UBRR_VALUE);
    UCSR1A = (1 << U2X1);
    UCSR1B = (1 << RXEN1) | (1 << TXEN1);
    UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
}

void UART1_transmit(char data)
{
    while (!(UCSR1A & (1 << UDRE1)));
    UDR1 = data;
}

int UART1_putchar(char c, FILE *stream)
{
    if (c == '\n') UART1_transmit('\r');
    UART1_transmit(c);
    return 0;
}

FILE uart1_stream = FDEV_SETUP_STREAM(UART1_putchar, NULL, _FDEV_SETUP_WRITE);

/* ---------- ADC ---------- */
void ADC_init(void)
{
    DDRF  = 0x00;
    PORTF = 0x00;
    ADMUX  = (1 << REFS0);
    ADCSRA = (1 << ADEN)
           | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t ADC_read(uint8_t channel)
{
    uint8_t low, high;
    ADMUX = (1 << REFS0) | (channel & 0x1F);
    _delay_us(10);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    low  = ADCL;
    high = ADCH;
    return ((uint16_t)high << 8) | low;
}

/* ---------- 이동평균 필터 ---------- */
uint16_t MAF_update(uint8_t ch, uint16_t new_val)
{
    maf_sum[ch] -= maf_buf[ch][maf_idx[ch]];
    maf_buf[ch][maf_idx[ch]] = new_val;
    maf_sum[ch] += new_val;
    maf_idx[ch] = (maf_idx[ch] + 1) % MAF_SIZE;
    return (uint16_t)(maf_sum[ch] / MAF_SIZE);
}

/* ---------- LCD 출력 헬퍼 ---------- */
static void lcd_write_norm(uint16_t val_x100)
{
    i2c_lcd_data('0' + (val_x100 / 100));
    i2c_lcd_data('.');
    i2c_lcd_data('0' + ((val_x100 / 10) % 10));
    i2c_lcd_data('0' + (val_x100 % 10));
}

/* ---------- 초기화 ---------- */
void IR_init(void)
{
    uint8_t ch, j;
    uint16_t val;

    for (ch = 0; ch < IR_CH_COUNT; ch++)
    {
        val = ADC_read(IR_START_CH + ch);
        for (j = 0; j < MAF_SIZE; j++)
            maf_buf[ch][j] = val;
        maf_sum[ch] = (uint32_t)val * MAF_SIZE;
        maf_idx[ch] = 0;
        ir_min[ch] = val;
        ir_max[ch] = val;
        norm_x100[ch] = 0;
    }
}

/* ---------- MAIN ---------- */
int main(void)
{
    uint16_t raw, filtered;
    uint8_t ch;
    uint16_t range;
    uint8_t led_state;

    /* JTAG 비활성화 - PF4~PF7을 ADC로 쓰려면 필수 */
    MCUCSR |= (1 << JTD);
    MCUCSR |= (1 << JTD);

    UART1_init();
    ADC_init();
    IR_init();
    stdout = &uart1_stream;

    /* LED: PA1~PA6 출력 설정 */
    DDRA  |= 0x7E;    /* 0b01111110 = PA1~PA6 출력 */
    PORTA &= ~0x7E;   /* PA1~PA6 OFF */

    /* I2C LCD 초기화 */
    i2c_lcd_init();

    while (1)
    {
        led_state = 0x00;

        /* ---- 센서 읽기 + 필터 + 정규화 ---- */
        for (ch = 0; ch < IR_CH_COUNT; ch++)
        {
            raw = ADC_read(IR_START_CH + ch);
            filtered = MAF_update(ch, raw);

            if (raw < ir_min[ch]) ir_min[ch] = raw;
            if (raw > ir_max[ch]) ir_max[ch] = raw;

            range = ir_max[ch] - ir_min[ch];
            if (range > 0)
                norm_x100[ch] = (uint16_t)(((uint32_t)(filtered - ir_min[ch]) * 100) / range);
            else
                norm_x100[ch] = 0;

            /* norm >= 0.80 이면 대응 LED ON (PA1~PA6) */
            if (norm_x100[ch] >= NORM_THRESHOLD)
                led_state |= (1 << (ch + 1));   /* ch0->PA1, ch1->PA2, ... ch5->PA6 */
        }

        /* LED 갱신 (PA1~PA6만, 나머지 비트 보존) */
        PORTA = (PORTA & ~0x7E) | led_state;

        /* ---- LCD: 정규화 값 표시 ---- */
        /* 1행: IR0~IR2 (PF2~PF4) */
        i2c_lcd_goto_XY(0, 0);
        for (ch = 0; ch < 3; ch++)
        {
            lcd_write_norm(norm_x100[ch]);
            if (ch < 2) i2c_lcd_data(' ');
        }
        i2c_lcd_data(' ');   /* 잔상 지움 */

        /* 2행: IR3~IR5 (PF5~PF7) */
        i2c_lcd_goto_XY(1, 0);
        for (ch = 3; ch < 6; ch++)
        {
            lcd_write_norm(norm_x100[ch]);
            if (ch < 5) i2c_lcd_data(' ');
        }
        i2c_lcd_data(' ');

        /* ---- UART 디버그 출력 ---- */
        printf("         original / filter(MAF) / min / max / norm\n");
        for (ch = 0; ch < IR_CH_COUNT; ch++)
        {
            printf("IR %d : %4u    %4u    %4u   %4u  %u.%02u\n",
                   ch,
                   ADC_read(IR_START_CH + ch),
                   (uint16_t)(maf_sum[ch] / MAF_SIZE),
                   ir_min[ch],
                   ir_max[ch],
                   norm_x100[ch] / 100,
                   norm_x100[ch] % 100);
        }
        printf("\n");

        _delay_ms(200);
    }

    return 0;
}