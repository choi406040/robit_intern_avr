# ATmega128 Day4_Task04 (심화) — PSD 원시 데이터 및 이동평균 필터링 데이터 동시 출력

> **광운대학교 로봇학부**  
> **작성자:** 최동현  
> **제작일:** 2026-08-02

---

## 1. 개요 (Overview)
본 과제는 과제 3의 PSD 거리 측정 시스템을 확장하여, 필터를 적용하지 않은 **원시 데이터(raw)** 와
**이동평균 필터(Moving Average Filter)** 를 적용한 데이터를 UART로 **동시에 출력**하고,
두 값의 변화 양상을 비교·분석하는 것을 목표로 함.

### 핵심 목표
* PORTF(ADC1)를 통한 PSD 센서 아날로그 값 주기적 수집
* 최근 3개 샘플을 이용한 이동평균 필터 구현 (시프트 버퍼 방식)
* 원시 데이터 기반 거리와 필터 적용 거리를 UART로 동시 전송
* 필터 미적용/적용 값의 변화 비교 및 필터 특성 분석

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
* **PSD 센서 노이즈:** 센서 내부 스위칭 및 전원 리플로 인해 정지 상태에서도 ADC 값이 수 LSB ~ 수십 LSB 흔들림 → 본 과제에서 필터를 적용하는 근거가 됨
* **주의사항:** PSD 센서는 순간 전류 소모가 커 전원이 약할 경우 ADC 값이 불안정해짐

---
## 4. 프로젝트 구조 (Directory Structure)
```text
├── Day4_Task04/
│   ├── main.c   
├── include/
│   ├── stdio.h
│   ├── avr/io.h
│   ├── avr/interrupt.h
│   ├── util/delay.h
└── README.md
```
---
## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)

### 5.1 UART0 / ADC 초기화
```c
DDRE   = 0x02;   // PE1(TXD0) 출력 설정
UBRR0L = 16;     // 보율 설정 (16MHz / (16 × 17) ≈ 58,823bps → 57600bps 사용)
UBRR0H = 0;
UCSR0A = 0x20;
UCSR0B = 0x18;   // RXEN0 | TXEN0 (송수신 허용)
UCSR0C = 0x06;   // 8-bit 데이터, 패리티 없음, 스톱비트 1

DDRF   = 0x00;   // PF1 입력
ADMUX  = 0x41;   // AVCC 기준전압, ADC1(PF1) 채널 선택
ADCSRA = 0x87;   // ADC Enable, 분주비 128 → 16MHz/128 = 125kHz
```

### 5.2 이동평균 필터 구현
```c
static int initialized = 0;
int adc_n[3] = {0,0,0};      // 최근 3개 샘플 저장 버퍼

int temp = Read_adc();
if (!initialized) {          // 첫 측정: 버퍼 전체를 현재 값으로 채워 초기 왜곡 방지
    adc_n[0] = adc_n[1] = adc_n[2] = temp;
    initialized = 1;
}
else {                       // 이후: 한 칸씩 밀어내고 최신 값 삽입 (FIFO)
    adc_n[0] = adc_n[1];
    adc_n[1] = adc_n[2];
    adc_n[2] = temp;
}
int Adc_value = (adc_n[0] + adc_n[1] + adc_n[2]) / 3;   // 3점 이동평균
```

### 5.3 원시 / 필터 데이터 동시 출력
```c
float distance = 37700 * pow(Adc_value, -1.189);   // 필터 적용 거리

Uart_Putstr("raw: ");
sprintf(buf, "%d", (int)(37700 * pow(adc_n[2], -1.189)));  // 최신 원시값 기반 거리
Uart_Putstr(buf);
Uart_Putstr("   distance_avg: ");
sprintf(buf, "%d\r\n", (int)distance);
Uart_Putstr(buf);
```
`adc_n[2]`(가장 최근 샘플)로 계산한 거리를 raw로, 3점 평균값으로 계산한 거리를
`distance_avg`로 같은 줄에 출력하여 두 값을 직접 비교할 수 있게 함.

### 5.4 비정상 데이터 예외처리
```c
if (Adc_value > 651 || Adc_value < 104){
    Uart_Putstr("out of range\r\n");
    continue;
}
```
* **ADC > 651 :** 센서 최소 측정 거리보다 가까워 출력이 다시 감소하는 구간(측정 불가)
* **ADC < 104 :** 센서 최대 측정 거리를 초과했거나 반사체가 없는 구간
* 판정은 **필터 적용 값** 기준으로 수행하므로, 단발성 노이즈로 인한 오탐(false out of range)이 줄어듦

---

## 6. 필터 관련 보고 (Filter Report)

### 6.1 사용한 필터: 이동평균 필터 (Moving Average Filter, N = 3)

가장 최근 N개의 샘플을 평균 내어 출력하는 **FIR(유한 임펄스 응답) 저역통과 필터**의
가장 단순한 형태이다.

$$y[n] = \frac{1}{N}\sum_{k=0}^{N-1} x[n-k] = \frac{x[n] + x[n-1] + x[n-2]}{3}$$

### 6.2 특징

| 항목 | 내용 |
| :--- | :--- |
| **동작 원리** | 무작위(백색) 노이즈는 부호가 랜덤하므로 평균 시 상쇄되고, 실제 신호는 유지됨 |
| **노이즈 감소량** | 이론상 표준편차가 `1/√N`배로 감소 → N=3이면 약 **58% 수준(-42%)** 으로 감소 |
| **주파수 특성** | 저역통과 특성. 고주파 노이즈를 억제하지만 급격한 신호 변화도 함께 둔화시킴 |
| **지연(Latency)** | 평균적으로 `(N-1)/2 = 1` 샘플 지연 발생 → 샘플링 주기 100ms 기준 **약 100ms 지연** |
| **연산 비용** | 덧셈 2회 + 나눗셈 1회로 매우 가벼워 MCU 환경에 적합 |
| **메모리** | 샘플 N개분의 배열만 필요 (`int adc_n[3]`, 6바이트) |

### 6.3 필터 미적용 값과 적용 값의 변화

**(1) 정지 상태 (반사체 고정)**
```text
raw: 30   distance_avg: 30
raw: 33   distance_avg: 31
raw: 29   distance_avg: 30
raw: 34   distance_avg: 32
raw: 30   distance_avg: 31
```
→ raw는 29~34cm 범위로 약 ±2~3cm 진동하지만, `distance_avg`는 30~32cm로
**변동 폭이 절반 이하로 감소**한다. 단발성으로 크게 튀는 값(스파이크)은
나머지 두 샘플에 의해 희석되어 출력에 미치는 영향이 1/3로 줄어든다.

**(2) 이동 상태 (반사체를 빠르게 이동)**
```text
raw: 30   distance_avg: 32
raw: 45   distance_avg: 36
raw: 60   distance_avg: 45
raw: 60   distance_avg: 55
raw: 60   distance_avg: 60
```
→ 실제 거리가 계단 형태로 변할 때 `distance_avg`가 최종값에 도달하기까지
**N-1 = 2 샘플(약 200ms)의 지연**이 발생한다. 즉 **노이즈 억제와 응답 속도는
트레이드오프 관계**이며, N을 키울수록 값은 부드러워지지만 반응이 느려진다.

> 위 수치는 실측 경향을 정리한 예시이며, 실제 제출 시에는 시리얼 터미널
> 캡처 로그로 대체할 것.

### 6.4 다른 필터와의 비교

| 필터 | 장점 | 단점 |
| :--- | :--- | :--- |
| **이동평균 (본 과제)** | 구현 단순, 백색 노이즈에 효과적 | 배열 필요, 스파이크에 여전히 영향받음, 지연 발생 |
| **중앙값(Median)** | 스파이크(임펄스 노이즈)를 완전히 제거 | 정렬 연산 필요, 연산량 증가 |
| **1차 저역통과(IIR)** | 배열 불필요(이전 출력 1개만 저장), 메모리 최소 | 계수(α) 튜닝 필요, 무한 임펄스 응답 |
| **칼만 필터** | 노이즈 억제와 응답성을 동시에 최적화 | 모델링·연산 부담이 큼 |

PSD 센서의 노이즈는 대부분 전원 리플에 기인한 **연속적인 백색 노이즈** 성격이 강하므로,
연산 비용 대비 효과가 좋은 이동평균 필터를 선택하였다.

---

## 7. 동작 설명 및 결과 (Results)

### 동작 시나리오
1. 전원 인가 → UART0 및 ADC1 채널 초기화, `sei()`로 전역 인터럽트 허용
2. `Read_adc()`로 100ms 주기마다 PSD 센서 값을 10비트(0~1023)로 변환
3. 첫 측정 시 버퍼 3칸을 동일 값으로 초기화(`initialized` 플래그) → 시작 시 0에서 끌려오는 왜곡 방지
4. 이후 샘플은 버퍼를 한 칸씩 시프트하며 삽입, 3개 평균 계산
5. 유효 범위(104 ~ 651) 검사 → 벗어나면 `out of range` 출력 후 다음 주기로 이동
6. 원시값 기반 거리와 평균값 기반 거리를 한 줄에 함께 UART로 출력

### 출력 예시
```text
raw: 30   distance_avg: 31
raw: 33   distance_avg: 31
raw: 29   distance_avg: 30
out of range
raw: 45   distance_avg: 42
```

### 동작 사진 / 영상

| 시연 영상 | 
| :---: | 
| (https://naver.me/F8lQXoXT) | 

### 한계 및 개선 방향
* **출력 항목:** 현재 거리(cm)만 두 가지로 출력하고 있어, 과제 예시의 `RAW: 412 | FILTERED: 405` 형태처럼 **ADC 원시값과 필터 ADC 값도 함께 출력**하면 필터 효과를 더 직접적으로 보여줄 수 있음
* **정수 절삭:** `(int)distance`로 소수점을 버려 최대 1cm 오차 발생. 예시(`15.2cm`)처럼 소수 첫째 자리까지 표현하려면 10배 스케일링 후 정수 분리 출력 권장
* **`_delay_ms()` 사용:** 지연 동안 MCU가 대기 상태이므로 Timer 인터럽트 기반 주기 샘플링으로 개선 가능
* **필터 차수 고정:** N이 3으로 하드코딩되어 있어, `#define N 3` 후 `for`문 합산 방식으로 바꾸면 차수 변경 실험이 쉬워짐
* **스파이크 대응:** 이동평균은 큰 단발 노이즈를 완전히 제거하지 못하므로, 중앙값 필터를 앞단에 두는 조합 구성도 고려 가능

---

## 8. AI 툴 활용 명시 (AI Tools Declaration)

| 도구명 (Tool) | 활용 영역 | 세부 사용 목적 및 내용 |
| :--- | :--- | :--- |
| **Claude** | 코드 디버깅 & 문서화 | - 이동평균 필터의 노이즈 감소량(1/√N) 및 지연 특성 이론 확인<br>- 버퍼 초기화 시점에 따른 시작 구간 왜곡 원인 분석<br>- README 문서 작성 및 레지스터 설정 주석 정리 |

### AI 활용 및 검증 원칙
1. **코드 검증:** AI가 제시한 레지스터 설정 및 필터 이론은 ATmega128 데이터시트, PSD 센서
   데이터시트와 대조 검증한 후, 실제 보드에서 raw/filtered 출력값을 비교하며 직접 수정·테스트하였습니다.
2. **학습 주도성:** 이동평균 필터 구현 및 예외처리 조건 설계는 직접 작성하였으며,
   AI는 보조 도구(디버깅, 문서화)로만 활용하였습니다.