# ATmega128 Day4_Task05 — 서보모터(SG90) PWM 제어

> **광운대학교 로봇학부**  
> **작성자:** 최동현  
> **제작일:** 2026-08-02

---

## 1. 개요 (Overview)
본 과제는 ATmega128의 **Timer/Counter1 Fast PWM** 기능을 이용하여 SG90 서보모터를 제어하고,
**UART 시리얼 통신**으로 PC 터미널에서 입력받은 목표 각도(0 ~ 180도)로 서보를 이동시키는
시스템을 구현하는 것을 목표로 함.

### 핵심 목표
* Timer1 Fast PWM(TOP = ICR1) 설정을 통한 50Hz(20ms 주기) PWM 신호 생성
* UART로 수신한 문자열을 정수 각도로 파싱하여 목표 위치 지정
* 시스템 초기화 시 서보모터 원점 복귀 후 대기
* 유효 범위(0 ~ 180도)를 벗어난 입력값에 대한 예외처리

---

## 2. 개발 환경 (Environment)

| 항목 | 내용 |
| :--- | :--- |
| **MCU** | ATmega128A (16MHz External Crystal) |
| **IDE / Compiler** | Microchip Studio 7.0 / AVR GCC |
| **Flasher Tool** | USBISP / STK500 |
| **언어** | C Language |
| **주요 부품** | ATmega128 개발보드, SG90 서보모터, 3pin 핀헤더(만능기판 납땜), USB-Serial 변환 모듈 |
| **터미널 설정** | 57600 bps / 8-N-1 |
| **팀 구성** | 2인 1조 (서보모터 2인당 1개 배부, 과제 및 소스코드는 각자 구현) |

---

## 3. 하드웨어 구성 및 핀 맵 (Hardware Structure)

### Pin Configuration

```text
[ATmega128]                 [Target Component]
 PB7 (OC1C)          ----->   SG90 서보모터 PWM 신호선 (주황/노란색)
 VCC (5V)            ----->   SG90 전원 (빨간색)
 GND                 ----->   SG90 접지 (갈색)
 PE1 (TXD0)          ----->   USB-Serial RXD (PC 시리얼 터미널)
 PE0 (RXD0)          <-----   USB-Serial TXD (목표 각도 입력)
```

### 주요 회로 특징
* **핀헤더 구성:** 만능기판에 3pin 핀헤더를 납땜하여 서보모터 커넥터를 착탈식으로 연결
* **전원:** SG90은 기동 시 순간 전류가 크므로(수백 mA) MCU 로직 전원과 분리하거나
  전원단에 전해 커패시터(약 470µF)를 두는 것이 안정적임
* **주의사항:** 서보의 GND는 반드시 MCU GND와 공통으로 묶어야 신호 기준점이 일치함

---

## 4. 프로젝트 구조 (Directory Structure)
```text
├── Day4_Task05/
│   ├── main.c   
├── include/
│   ├── avr/io.h
│   ├── stdlib.h
│   ├── util/delay.h
└── README.md
```

---

## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)

### 5.1 Timer1 Fast PWM 초기화 (`servo_init()`)
```c
void servo_init(){
    DDRB = 0x80;    // PB7(OC1C) 출력 설정

    TCCR1A = 0x0A;  // COM1C1 = 1 (비반전 출력), WGM11 = 1
    TCCR1B = 0x1B;  // WGM13:12 = 11 → Fast PWM (TOP = ICR1), CS = 011 (분주비 64)
    ICR1   = 4999;  // TOP 값 → 20ms 주기(50Hz)
}
```

**PWM 주기 계산**

| 항목 | 계산 |
| :--- | :--- |
| 타이머 클럭 | `16MHz / 64 = 250kHz` → 1 tick = **4µs** |
| PWM 주기 | `(ICR1 + 1) × 4µs = 5000 × 4µs = 20ms` → **50Hz** |
| 듀티 분해능 | TOP = 4999이므로 0 ~ 4999 단계 |

> Fast PWM 모드에서 WGM13:10 = `1110`(TOP = ICR1)을 선택한 이유는,
> ICR1로 TOP을 직접 지정해 서보 규격인 정확한 20ms 주기를 만들기 위함임.

### 5.2 각도 → 펄스 폭 변환 (`turn_servo()`)
```c
void turn_servo(int angle){
    if (angle < 0)   angle = 0;      // 유효 범위 제한 (안전장치)
    if (angle > 180) angle = 180;

    OCR1C = 250 + ((long)angle * 250) / 180;
}
```

**펄스 폭 매핑 (1 tick = 4µs 기준)**

| 목표 각도 | OCR1C 값 | 펄스 폭 | 듀티비 |
| :---: | :---: | :---: | :---: |
| 0° | 250 | 1.0ms | 5% |
| 90° | 375 | 1.5ms | 7.5% |
| 180° | 500 | 2.0ms | 10% |

> `(long)` 캐스팅을 적용한 이유는 `180 × 250 = 45,000`이 `int`(16bit, 최대 32,767)
> 범위를 초과해 오버플로우가 발생하기 때문임.

### 5.3 시스템 초기화 시 원점 복귀 (`set_servo()`)
```c
void set_servo(){
    OCR1C = 500;        // 원점 위치로 이동
    _delay_ms(1000);    // 서보가 물리적으로 이동을 완료할 때까지 대기
}
```
전원 투입 또는 리셋 직후 서보를 지정된 기준 원점으로 이동시킨 뒤 1초간 대기하여,
불확정한 초기 위치에서 갑자기 큰 각도로 튀는 것을 방지함.

### 5.4 UART를 통한 각도 입력 (`Get_angle()`)
```c
int Get_angle(void){
    int num = 0;
    int got_digit = 0;              // 숫자를 하나라도 받았는지 표시

    while (1) {
        char c = UART0_receive();

        if (c >= '0' && c <= '9') { // 숫자면 자릿수 조립
            num = num * 10 + (c - '0');
            got_digit = 1;
        }
        else {                      // 엔터/공백 등 구분자가 오면
            if (got_digit) return num;   // 숫자를 받은 상태면 입력 종료
            // 아직 숫자가 없으면 무시하고 계속 대기
        }
    }
}
```
문자열 `"90\r\n"`을 받으면 `'9'`, `'0'`을 순서대로 조립해 `90`을 만들고,
`'\r'`을 만나는 순간 입력을 확정하여 반환함.
숫자 없이 엔터만 눌린 경우는 무시하므로 오동작하지 않음.

### 5.5 UART 송수신 함수
```c
char UART0_receive(void){
    while (!(UCSR0A & (1 << RXC0)));   // 수신 완료(RXC0) 대기
    return UDR0;
}

void Uart_Putnum(int num){
    char buf[6];
    itoa(num, buf, 10);                // <stdlib.h>의 itoa (10진수 변환)
    Uart_Putstr(buf);
}
```
수신은 `RXC0` 플래그, 송신은 `UDRE0` 플래그를 확인하는 **폴링 방식**으로 구현함.

---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오
1. 전원 인가 → `servo_init()`으로 Timer1 Fast PWM(50Hz), `uart_init()`으로 UART0 초기화
2. `set_servo()`로 서보를 원점으로 이동 후 1초 대기 (초기화 완료)
3. 터미널에 `start_servo!` 메시지 출력 후 각도 입력 대기
4. PC 시리얼 터미널에서 목표 각도(예: `90`) 입력 → 엔터
5. 수신한 각도를 `turn: 90` 형태로 되돌려 출력(에코)
6. `turn_servo()`가 각도를 OCR1C 값으로 환산하여 서보를 해당 위치로 이동
7. 다시 다음 입력 대기 (무한 반복)

### 출력 예시
```text
start_servo!
turn: 0
turn: 90
turn: 180
```

### 동작 사진 / 영상

| 시연 영상 |
| :---: | 
| (https://naver.me/FKGMwdip) | 

### 한계 및 개선 방향
* **원점 각도 확인 필요:** `set_servo()`의 `OCR1C = 500`은 5.2절 매핑 기준으로 **180도** 위치임.
  원점을 90도로 두려면 `OCR1C = 375`, 0도로 두려면 `OCR1C = 250`으로 수정해야 함
* **경고 메시지 미출력:** 과제 요구사항 중 "유효 범위 초과 시 경고 메시지 출력"이 아직 미구현
  상태로, 현재는 `turn_servo()`에서 조용히 0/180으로 클램핑만 수행함.
  범위 검사를 `main()` 쪽으로 옮겨 `out of range` 메시지 출력 후 모터 동작을 건너뛰는 방식 권장
* **주석 정정 필요:** `turn_servo()` 내부 주석의 "0.5us 단위, 2000틱/4000틱"은 분주비 8 기준의
  이전 계산으로, 현재 분주비 64(1 tick = 4µs, 250 ~ 500틱)와 맞지 않으므로 갱신 필요
* **입력 자릿수 제한 없음:** `Get_angle()`이 자릿수를 제한하지 않아 매우 긴 숫자 입력 시
  `int` 오버플로우로 음수가 될 수 있음(클램핑으로 물리적 위험은 없으나 입력 검증 추가 권장)
* **폴링 방식 수신:** `UART0_receive()`가 블로킹 동작이므로 입력 대기 중 다른 작업 불가.
  RX 인터럽트(`USART0_RX_vect`) 기반으로 바꾸면 논블로킹 구조로 확장 가능

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)

| 도구명 (Tool) | 활용 영역 | 세부 사용 목적 및 내용 |
| :--- | :--- | :--- |
| **Claude** | 코드 디버깅 & 문서화 | - Timer1 Fast PWM(TOP = ICR1) 모드 비트 조합 및 20ms 주기 계산 검증<br>- 각도-펄스폭 변환식의 `int` 오버플로우 원인 분석 및 캐스팅 적용<br>- README 문서 작성 및 레지스터 설정 주석 정리 |

### AI 활용 및 검증 원칙
1. **코드 검증:** AI가 제시한 레지스터 설정은 ATmega128 데이터시트 및 SG90 서보 규격
   (20ms 주기 / 1~2ms 펄스)과 대조 검증한 후, 실제 보드에서 각도별 동작을 직접 확인하며
   수정·테스트하였습니다.
2. **학습 주도성:** PWM 매핑 로직 및 UART 각도 파싱 로직 설계는 직접 작성하였으며,
   AI는 보조 도구(디버깅, 문서화)로만 활용하였습니다.