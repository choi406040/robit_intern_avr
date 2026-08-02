# ATmega128 과제 및 프로젝트 템플릿

> **광운대학교 (로봇학부)**  
> **작성자:** (최동현)
> **제출일:** (26-08-02)

---

## 1. 개요 (Overview)
본 과제는 ATmega128 마이크로컨트롤러를 활용하여 가변저항(adc), lcd, uart을 이용하여 다이나믹셀(dynamixel)를 제어하는 것을 목표로 함.

### 핵심 목표
* ATmega128 레지스터 설정을 통한 주변장치 제어
* 센서 및 외부 모듈과의 통신 (uart/ I2C /adc등) 및 데이터 처리
* 타이머/카운터를 활용한 PWM 출력 및 인터럽트 제어

---

## 2. 개발 환경 (Environment)

| 항목 | 내용 |
| :--- | :--- |
| **MCU** | ATmega128A (16MHz External Crystal) |
| **IDE / Compiler** | Microchip Studio 7.0 / Microchip AVR GCC |
| **Flasher Tool** | USBISP / STK500 |
| **언어** | C Language |
| **주요 부품** | ATmega128 개발보드, dynamixel, ADC(가변저항) , uart, max 485|

---

## 3. 하드웨어 구성 및 핀 맵 (Hardware Structure)

### Pin Configuration

```text
[ATmega128]                 [Target Component]
 PORTF (PF0)         ----->   가변저항
 PORTD (PD2,3)       ----->   uart
 PE0 (RXD0) / PE1    ----->   uary(max 485)
 PORTD (PD0,1)       ----->   i2c
```

### 주요 회로 특징
* **전원:** 5V DC 안정화 전원 공급
* **주의사항:** ISP 다운로드 시 SPI 핀 타겟 전원 및 리셋 회로 간섭 주의

---

## 4. 프로젝트 구조 (Directory Structure)
> 구현부(.c), 선언부(.h)만 구조에 표기함.
```text
├── Day3_Task03/
│   ├── main.c  # 메인 제어 루프 및 시스템 초기화
│   ├── i2c.c   #i2c 통신 
│   ├── clcd.c  #lcd 제어
├── include/
│   ├── avr/io.h
│   ├── stdint.h
│   ├── util/delay.h
├── docs/
│   └── schematic.pdf # 회로도 파일
└── README.md
```

---

## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)

### 타이머/카운터 및 PWM 초기화 예시 (`timer.c`)
```c

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
```

---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오
1. 시스템 전원 인가 시 ATmega128 주변장치(dynamixel, UART, lcd, ADC) 초기화함
2. 목표 속도를 입력, 가변저항을 이용해 dynamixel 위치 제어

### 동작 사진 / 영상

| 동작 모습 | 
| :---: |
| (https://naver.me/GHL7bXT7) |

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)
본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool) | 활용 영역 | 세부 사용 목적 및 내용 |
| :--- | :--- | :--- |
| Claude** | 코드 디버깅 & 리팩토링 | - 빌드 에러 및 문법 오류 원인 분석<br>- 레지스터 설정 주석 작성 및 가독성 개선 | - 프로토콜 2.0 설명서 번역 및 예시 자료 해석, 코드 구현|

cf. 포켓의 이해는 어느 정도 하였지만 그것을 어떻게 활용하고 어떤 기능들이 있는지는 정확하게 이해하지 못하여서 스스로 코드 구현까지 가지 못하였다. 이에 ai의 도움을 받아서 과제를 수행하였다. 

