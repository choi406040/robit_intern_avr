# ATmega128 과제 및 프로젝트 템플릿

> **광운대학교 (로봇학부)**  
> **작성자:** (최동현)
> **제출일:** (26-08-02)

---

## 1. 개요 (Overview)
본 과제는 ATmega128 마이크로컨트롤러를 활용하여 uart register를 사용하지 않고 uart룰 구현하는 것을 목표로 함.

### 핵심 목표
* ATmega128 레지스터 설정을 통한 uart 구현
* uart register 사용 금지
* baudrate는 9600 고정 

---

## 2. 개발 환경 (Environment)

| 항목 | 내용 |
| :--- | :--- |
| **MCU** | ATmega128A (16MHz External Crystal) |
| **IDE / Compiler** | Microchip Studio 7.0 / Microchip AVR GCC |
| **Flasher Tool** | USBISP / STK500 |
| **언어** | C Language |
| **주요 부품** | ATmega128 개발보드, DC/STEP 모터, ADC 센서 모듈 |

---

## 3. 하드웨어 구성 및 핀 맵 (Hardware Structure)

### Pin Configuration

```text
[ATmega128]                 [Target Component]
 PORTD (PD3)          ----->   uart
```

### 주요 회로 특징
* **전원:** 5V DC 안정화 전원 공급
* **주의사항:** ISP 다운로드 시 SPI 핀 타겟 전원 및 리셋 회로 간섭 주의

---

## 4. 프로젝트 구조 (Directory Structure)
> 구현부(.c), 선언부(.h)만 구조에 표기함.
```text
├── Day3_Task04/
│   ├── main.c # 메인 제어 루프 및 시스템 초기화
├── include/
│   ├── avr/io.h
│   ├── util/delay.h
├── docs/
│   └── schematic.pdf # 회로도 파일
└── README.md
```

---

## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)

### 타이머/카운터 및 PWM 초기화 예시 (`timer.c`)
```c
#include <avr/io.h>
#include <util/delay.h>
void Uart_Putch(int str){
	PORTD=0b00000000;               //start 비트
	_delay_us(104);
	for (int i=1; i<=128; i*=2){   // ← 1부터 시작
		if (str & i){              // ← AND로 판정
			PORTD = 0b00001000;    // ← PD3 High
		}
		else{
			PORTD = 0b00000000;    // PD3 Low
		}
		_delay_us(104);             //baudrate 9600을 맞추기 위해 자체적으로 딜레이 시키기
	}
	PORTD=0b00001000;               //end 비트
	_delay_us(104);
}
```

---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오
1. 시스템 전원 인가 시 ATmega128 주변장치(UART) 초기화함
2. PORTD를 이용해서 uart의 데이터 전송 포멧을 직접 구현함
3. starbit(0)-databit(8bit)-endbit(1)

### 동작 사진 / 영상

| 정면 동작 모습 | 
| :---: |
| ![Hardware Setup](https://naver.me/GtYnWlkw) |

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)
본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool) | 활용 영역 | 세부 사용 목적 및 내용 |
| :--- | :--- | :--- |
| Claude** | 코드 디버깅( & 리팩토링) -baudrate 9600설정 | - delay를 104로 설정하면 된다고 하여서 _delay_ms(104)로 설정하고 코드를 실행시켰을 때 작동하지 않았다. _delay_us(104)로 설정하여야 했다.  |


### AI 활용 및 검증 원칙
1. **코드 검증:** AI가 생성한 레지스터 설정 및 함수 코드는 데이터시트(ATmega128 Datasheet)와 비교 검증한 후 실제 시리얼 모니터링을 거쳐 직접 수정 및 테스트하였습니다.
2. **학습 주도성:** 코드의 핵심 제어 로직 설계는 직접 작성하였으며, AI는 보조 도구(디버깅, 문서화)로만 활용하였습니다.