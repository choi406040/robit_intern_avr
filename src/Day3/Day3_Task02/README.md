# ATmega128 과제 및 프로젝트 템플릿

> **광운대학교 (로봇학부)**  
> **작성자:** (최동현)
> **제출일:** (26-08-02)

---

## 1. 개요 (Overview)
본 과제는 ATmega128 마이크로컨트롤러를 활용한다. uart를 이용하여 시리얼 모니터에서 0~9를 입력받는다. 입력받은 값에 따라 led의 움직임이 달라진다. uart를 제어하고 센서 데이터를 수신/처리하는 시스템을 구현하는 것을 목표로 함.

### 핵심 목표
* ATmega128 레지스터 설정을 통한 주변장치 제어
* 센서 및 외부 모듈과의 통신 (USART 등) 및 데이터 처리

---

## 2. 개발 환경 (Environment)

| 항목 | 내용 |
| :--- | :--- |
| **MCU** | ATmega128A (16MHz External Crystal) |
| **IDE / Compiler** | Microchip Studio 7.0 / Microchip AVR GCC |
| **Flasher Tool** | USBISP / STK500 |
| **언어** | C Language |
| **주요 부품** | ATmega128 개발보드, uart |

---

## 3. 하드웨어 구성 및 핀 맵 (Hardware Structure)

### Pin Configuration

```text
[ATmega128]                 [Target Component]
 PORTA (PA0 ~ PA7)   ----->   8-Bit LED
 PE0 (RXD0) / PE1    ----->   UART Serial Communication
```

### 주요 회로 특징
* **전원:** 5V DC 안정화 전원 공급
* **주의사항:** ISP 다운로드 시 SPI 핀 타겟 전원 및 리셋 회로 간섭 주의

---

## 4. 프로젝트 구조 (Directory Structure)
> 구현부(.c), 선언부(.h)만 구조에 표기함.
```text
├── Day3_Task02/
│   ├── main.c # 메인 제어 루프 및 시스템 초기화
├── include/
│   ├── avr/io.h
│   ├── avr/interrupt.h
│   ├── util/delay.h
├── docs/
│   └── schematic.pdf # 회로도 파일
└── README.md
```

---

## 5. 핵심 코드 및 레지스터 설정 (Key Implementation)

### uart를 이용한 문자입력 및 문자열 출력
```c
#include <avr/io.h>
#include <avr/interrupt.h>

//문자 가지고 오기
unsigned char Uart_Getch(void){
	while(!(UCSR0A&(1<<RXC0)));
	return UDR0;
}
//문자열 통신하기
void Uart_Putch(char *PutData){
	while(*PutData!='\0'){
		while (!(UCSR0A&(1<<UDRE0)));
		UDR0=*PutData;
		PutData++;
	}
	
}
```

---

## 6. 동작 설명 및 결과 (Results)

### 동작 시나리오
1. 시스템 전원 인가 시 ATmega128 주변장치(UART,LED) 초기화함
2. 숫자 입력 
3. 입력 받은 숫자를 통해 led를 제어함
4. 제어 후 몇번 led가 켜졌는지 uart로 통신(8,9 예외)

### 동작 사진 / 영상

| 동작 모습 |
| :---:
| ![Hardware Setup](https://naver.me/FEg7jUd5) | 

---

## 7. AI 툴 활용 명시 (AI Tools Declaration)
본 과제 작성 및 구현 과정에서 활용한 AI 도구(Generative AI)의 사용 현황 및 목적은 다음과 같음.

| 도구명 (Tool) | 활용 영역 | 세부 사용 목적 및 내용 |
| :--- | :--- | :--- |
|  Claude** | 코드 디버깅  | - 빌드 에러 및 문법 오류 원인 분석<br> |


### AI 활용 및 검증 원칙
1. **코드 검증:** AI가 생성한 레지스터 설정 및 함수 코드는 데이터시트(ATmega128 Datasheet)및 교육자료 및 구글링과 비교 검증한 후 실제 시리얼 모니터링을 거쳐 직접 수정 및 테스트하였습니다.
2. **학습 주도성:** 코드의 핵심 제어 로직 설계는 직접 작성하였으며, AI는 보조 도구(디버깅, 문서화)로만 활용하였습니다.