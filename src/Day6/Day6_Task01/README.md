# ATmega128 IR 센서 필터링 및 정규화 출력

> **광운대학교 로봇학부**  
> **작성자:** 최동현  
> **제출일:** 2026-08-09

---

## 1. 개요 (Overview)
본 과제는 ATmega128 마이크로컨트롤러의 ADC를 활용하여 6채널 IR 수발광 센서 값을 읽고, 이동평균 필터(MAF) 및 Min/Max 기반 정규화를 적용하여 센서 데이터를 처리하는 시스템을 구현하는 것을 목표로 함.

### 핵심 목표
* ADC를 이용한 6채널 IR 센서 아날로그 값 수신 및 이동평균 필터(MAF) 적용
* Min/Max 자동 캘리브레이션 기반 정규화(0.00 ~ 1.00) 연산
* 정규화 값 0.80 이상 시 대응 LED 점등 제어
* I2C LCD(16x2)에 6채널 정규화 값 실시간 표시
* UART를 통한 원본/필터/Min/Max/정규화 값 시리얼 모니터 디버그 출력

---

## 2. 개발 환경 (Environment)

| 항목 | 내용 |
| :--- | :--- |
| **MCU** | ATmega128A (16MHz External Crystal) |
| **IDE / Compiler** | Microchip Studio 7.0 / Microchip AVR GCC |
| **Flasher Tool** | Atmel-ICE (ISP) |
| **언어** | C Language |
| **주요 부품** | ATmega128 개발보드, IR 수발광 센서 모듈 6개, I2C Text LCD(16x2, PCF8574T), LED 6개 |

---

## 3. 하드웨어 구성 및 핀 맵 (Hardware Structure)

### Pin Configuration

```text
[ATmega128]                 [Target Component]
 PF2 ~ PF7 (ADC2~7)  ----->   IR 수발광 센서 6채널 (아날로그 입력)
 PA1 ~ PA6           ----->   LED 6개 (정규화 값 ≥ 0.80 시 점등)
 PD0 (SCL) / PD1 (SDA) --->   I2C Text LCD (PCF8574T, 주소 0x27)
 PD2 (RXD1) / PD3 (TXD1) ->   UART1 시리얼 디버그 출력 (57600bps)
```

### 주요 회로 특징
* **전원:** 5V DC 안정화 전원 공급
* **ADC 기준전압:** AVCC 기준 (ADMUX REFS0), 분주비 128 (125kHz ADC 클럭)
* **JTAG 비활성화:** PF4~PF7을 ADC 입력으로 사용하기 위해 JTD 비트를 연속 2회 세팅하여 JTAG 기능을 런타임에서 비활성화함
* **UART1:** U2X1 모드(2배속) 적용, UBRR = 34 (16MHz 기준 57600bps)
* **주의사항:** AREF 핀에 0.1µF 바이패스 커패시터 권장, IR 발광부 전류제한 저항 확인 필요

---

## 4. 프로젝트 구조 (Directory Structure)
```text
├── main.c
├── include/
│   ├── avr/io.h
│   ├── avr/interrupt.h
│   ├── util/delay.h
│   ├── stdio.h
│   ├── i2c.h
│   └── clcd.h
└── README.md
```

---

## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)

### ADC 초기화 및 채널 읽기
```c
void ADC_init(void)
{
    DDRF  = 0x00;       // PORTF 전체 입력
    PORTF = 0x00;       // 내부 풀업 OFF (아날로그 입력)
    ADMUX  = (1 << REFS0);   // 기준전압 AVCC, 우측 정렬
    ADCSRA = (1 << ADEN)     // ADC Enable
           | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);  // 분주비 128
}
```

### 이동평균 필터 (MAF, 윈도우 크기 8)
```c
uint16_t MAF_update(uint8_t ch, uint16_t new_val)
{
    maf_sum[ch] -= maf_buf[ch][maf_idx[ch]];   // 가장 오래된 값 제거
    maf_buf[ch][maf_idx[ch]] = new_val;         // 새 값 저장
    maf_sum[ch] += new_val;                     // 새 값 누적
    maf_idx[ch] = (maf_idx[ch] + 1) % MAF_SIZE; // 링 버퍼 순환
    return (uint16_t)(maf_sum[ch] / MAF_SIZE);
}
```

### 정규화 및 LED 제어
```c
// 정규화: (filtered - min) / (max - min) * 100
range = ir_max[ch] - ir_min[ch];
if (range > 0)
    norm_x100[ch] = (uint16_t)(((uint32_t)(filtered - ir_min[ch]) * 100) / range);

// norm >= 0.80 이면 대응 LED ON (PA1~PA6)
if (norm_x100[ch] >= NORM_THRESHOLD)
    led_state |= (1 << (ch + 1));   // ch0->PA1, ch1->PA2, ... ch5->PA6
```

---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오
1. 시스템 전원 인가 시 JTAG 비활성화, UART1, ADC, I2C LCD 초기화 수행
2. 6채널 IR 센서(PF2~PF7)의 ADC 값을 200ms 주기로 읽고 이동평균 필터 적용
3. Min/Max 값을 실시간으로 갱신하여 정규화(0.00 ~ 1.00) 연산 수행
4. 정규화 값이 0.80 이상인 채널의 대응 LED(PA1~PA6) 점등
5. I2C LCD 1행에 IR 0~2, 2행에 IR 3~5의 정규화 값 표시
6. UART1을 통해 원본/필터/Min/Max/정규화 값을 시리얼 모니터에 출력

### UART 출력 예시
```text
         original / filter(MAF) / min / max / norm
IR 0 :  243     250     233    921  0.02
IR 1 :  945     943     240    964  0.97
IR 2 :  874     785     221    870  0.95
IR 3 :  668     662     254    896  0.64
IR 4 :  224     226     221    932  0.01
IR 5 :  225     231     230    978  0.01
```

### 동작 사진 / 영상

| 정면 동작 모습 |
| :---: |
| [https://naver.me/5Mv2ZPjS] | 

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)
본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool) | 활용 영역 | 세부 사용 목적 및 내용 |
| :--- | :--- | :--- |
| **Claude** | 코드 작성 & 디버깅 | - ADC + MAF 필터 + 정규화 코드 구조 설계 보조<br>- UART printf 구현 방식 및 avr-libc FDEV_SETUP_STREAM 활용법 참고<br>- JTAG 비활성화(JTD 비트) 및 ISP 프로그래밍 에러(0xc0) 원인 분석 |

### AI 활용 및 검증 원칙
1. **코드 검증:** AI가 생성한 레지스터 설정 및 함수 코드는 데이터시트(ATmega128 Datasheet)와 비교 검증한 후 실제 시리얼 모니터링을 거쳐 직접 수정 및 테스트하였습니다.
2. **학습 주도성:** 코드의 핵심 제어 로직 설계는 직접 작성하였으며, AI는 보조 도구(디버깅, 문서화)로만 활용하였습니다.
