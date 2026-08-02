# ATmega128 Day4_Task03 — PSD 기반 거리 측정 시스템

> **광운대학교 로봇학부**  
> **작성자:** 최동현  
> **제작일:** 2026-08-02

---

## 1. 개요 (Overview)
본 과제는 ATmega128의 **ADC(PORTF)** 로 PSD(적외선 거리) 센서의 아날로그 출력을 읽고,
센서 특성 곡선에 맞춰 **거리(cm) 단위로 환산**한 뒤 **UART0**를 통해 PC 시리얼 터미널로
실시간 출력하는 시스템을 구현하는 것을 목표로 함.

### 핵심 목표
* PORTF(ADC1) 채널을 이용한 PSD 센서 아날로그 값 수집
* 비선형적인 센서 출력 특성을 거듭제곱 근사식으로 거리(cm)로 환산
* UART0 폴링 방식 송신을 통한 PC 시리얼 터미널 출력
* 측정 주기(100ms) 설정 및 유효 범위를 벗어난 비정상 데이터 예외처리

---

## 2. 개발 환경 (Environment)

| 항목 | 내용 |
| :--- | :--- |
| **MCU** | ATmega128A (16MHz External Crystal) |
| **IDE / Compiler** | Microchip Studio 7.0 / AVR GCC |
| **Flasher Tool** | USBISP / STK500 |
| **언어** | C Language |
| **주요 부품** | ATmega128 개발보드, PSD 적외선 거리 센서, USB-Serial 변환 모듈 |
| **터미널 설정** | 57600 bps / 8-N-1 |

> **컴파일 옵션 주의:** `pow()` 함수를 사용하므로 링커 옵션에 **`-lm`(libm)** 이 포함되어야 함.
> Microchip Studio 기준 `Project Properties → Toolchain → AVR/GNU Linker → Libraries`에 `m` 추가.


---

## 3. 하드웨어 구성 및 핀 맵 (Hardware Structure)

### Pin Configuration

```text
[ATmega128]                 [Target Component]
 PF1 (ADC1)          <-----   PSD 센서 아날로그 출력 (Vo)
 PE1 (TXD0)          ----->   USB-Serial RXD (PC 시리얼 터미널)
 PE0 (RXD0)          <-----   USB-Serial TXD (수신 허용, 미사용)
 VCC / GND           ----->   PSD 센서 전원 (5V)
```

### 주요 회로 특징
* **ADC 기준전압:** AVCC 기준 (`ADMUX`의 REFS0 = 1)
* **PSD 센서 노이즈:** 출력단에 바이패스 커패시터(약 10µF)를 두면 값 튐이 줄어듦
* **주의사항:** PSD 센서는 순간 전류 소모가 커 전원이 약할 경우 ADC 값이 불안정해짐

---
## 4. 프로젝트 구조 (Directory Structure)
```text
├── Day4_Task03/
│   ├── main.c   
├── include/
│   ├── stdio.h
│   ├── avr/io.h
│   ├── avr/interrupt.h
│   ├── util/delay.h
│   ├── math.h
└── README.md
```
---
## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)

### 5.1 UART0 초기화
```c
DDRE   = 0x02;   // PE1(TXD0) 출력 설정
UBRR0L = 16;     // 보율 설정
UBRR0H = 0;
UCSR0A = 0x20;
UCSR0B = 0x18;   // RXEN0 | TXEN0 (송수신 허용)
UCSR0C = 0x06;   // 8-bit 데이터, 패리티 없음, 스톱비트 1
```
**보율 계산:** `Baud = F_CPU / (16 × (UBRR + 1)) = 16,000,000 / (16 × 17) ≈ 58,823 bps`
→ 표준 **57600 bps**와의 오차 약 2.1%로 통신 가능 범위 내에 있음.

### 5.2 ADC 초기화 및 변환 (`Read_adc()`)
```c
ADMUX  = 0x41;   // AVCC 기준전압, ADC1(PF1) 채널 선택
ADCSRA = 0x87;   // ADC Enable, 분주비 128 → 16MHz/128 = 125kHz

int Read_adc(){
    unsigned int adc_data = 0;
    unsigned char channel = 0x01;

    ADMUX = 0x41 | channel;    // ADC1 채널 지정
    ADCSRA |= (1 << ADSC);     // 변환 시작
    while(ADCSRA & 0x40);      // ADSC가 0이 될 때까지(변환 완료) 대기
    adc_data = ADC;
    _delay_ms(100);            // 측정 주기 100ms (약 10Hz)
    return adc_data;
}
```
> ADC 클럭을 125kHz로 맞춘 이유는 10비트 전체 분해능을 얻기 위한 권장 범위
> (50kHz ~ 200kHz)를 만족시키기 위함임.

### 5.3 거리 환산식
PSD 센서의 출력 전압은 거리에 반비례하는 **비선형 특성**을 가지므로,
데이터시트의 출력 곡선을 거듭제곱 함수로 근사하여 환산함.

```c
float distance = 37700 * pow(Adc_value, -1.189);
```

| 항목 | 내용 |
| :--- | :--- |
| 근사 모델 | `distance[cm] = 37700 × ADC^(-1.189)` |
| 특성 | ADC 값이 클수록(가까울수록) 거리 감소, 로그-로그 스케일에서 직선 형태 |

### 5.4 비정상 데이터 예외처리
```c
if (Adc_value > 651 || Adc_value < 104){
    Uart_Putstr("out of range\r\n");
    continue;
}
```
* **ADC > 651 :** 센서 최소 측정 거리보다 가까워 출력이 다시 감소하는 구간(측정 불가)
* **ADC < 104 :** 센서 최대 측정 거리를 초과했거나 반사체가 없는 구간
* 유효 구간(약 **17cm ~ 150cm**)을 벗어나면 거리값 대신 `out of range` 메시지를 전송

### 5.5 UART 문자열 송신 (`Uart_Putstr()`)
```c
void Uart_Putstr(char *str){
    while(*str){
        while(!(UCSR0A & (1 << UDRE0)));  // 송신 버퍼가 빌 때까지 대기
        UDR0 = *str++;
    }
}
```
포인터를 1바이트씩 증가시키며 널 문자(`\0`)를 만날 때까지 폴링 방식으로 송신함.

---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오
1. 전원 인가 → UART0 및 ADC1 채널 초기화, `sei()`로 전역 인터럽트 허용
2. `Read_adc()`로 PSD 센서의 아날로그 값을 10비트(0~1023)로 변환
3. 유효 범위(104 ~ 651) 검사 → 벗어나면 `out of range` 출력 후 다음 측정 주기로 이동
4. 유효한 경우 거듭제곱 근사식으로 거리(cm) 환산 → `sprintf()`로 문자열 변환
5. `Uart_Putstr()`로 PC 시리얼 터미널에 100ms 주기로 거리값 출력

### 출력 예시
```text
28
27
31
out of range
45
```

### 동작 사진 / 영상

| 시연 영상 |
| :---: | 
| (https://naver.me/IFET2AtZ) | 

### 한계 및 개선 방향
* **노이즈 필터 미적용:** 단일 샘플만 사용하므로 값이 튐. 이동평균 또는 중앙값 필터 적용 시 안정성 향상
* **`_delay_ms()` 사용:** 지연 동안 MCU가 다른 작업을 하지 못하므로, Timer 인터럽트 기반 주기 측정으로 개선 가능
* **정수 절삭:** `(int)distance`로 소수점 이하를 버려 최대 1cm 오차 발생. 소수 첫째 자리까지 출력하려면 10배 스케일링 후 정수 연산으로 분리 출력 권장
* **`pow()` 연산 비용:** 부동소수점 라이브러리를 사용하므로 코드 크기와 연산 시간이 큼. 룩업 테이블(LUT) + 선형 보간으로 대체 가능

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)

| 도구명 (Tool) | 활용 영역 | 세부 사용 목적 및 내용 |
| :--- | :--- | :--- |
| **Claude** | 코드 디버깅 & 문서화 | - `UBRR0` 값에 따른 실제 보율 및 오차율 검증<br>- ADC 분주비 설정과 변환 정확도의 관계 확인<br>- README 문서 작성 및 레지스터 설정 주석 정리 |

### AI 활용 및 검증 원칙
1. **코드 검증:** AI가 제시한 레지스터 설정은 ATmega128 데이터시트 및 PSD 센서 데이터시트와
   대조 검증한 후, 실제 보드에서 줄자로 실측한 거리와 비교하며 직접 수정·테스트하였습니다.
2. **학습 주도성:** 거리 환산 로직 및 예외처리 조건 설계는 직접 작성하였으며,
   AI는 보조 도구(디버깅, 문서화)로만 활용하였습니다.