#define F_CPU 16000000
#include <avr/io.h>
#include <stdlib.h>
#include <util/delay.h>
void servo_init();
void uart_init();
void set_servo();
void turn_servo(int angle);
void Uart_Putstr(char *str);
int Get_angle(void);
char UART0_receive(void);
void Uart_Putnum(int num);
int main(void)
{	servo_init();
	uart_init();
	set_servo();
	
	Uart_Putstr("start_servo!\r\n");
	while(1)
	{
		int angle = Get_angle();       // 각도 숫자로 바로 받음

		// 각도 확인 출력
		Uart_Putstr("turn: ");
		Uart_Putnum(angle);            // 숫자 출력 함수 (아래 추가)
		Uart_Putstr("\r\n");

		// 서보 돌리기
		turn_servo(angle);
	}
}

void servo_init(){
	DDRB = 0x80;    // PB7 출력

	TCCR1A = 0x0A;  // COM1C1=1 (OC1C 비반전 출력), WGM11=1
	TCCR1B = 0x1B;  // WGM13:12=11 → Fast PWM, TOP=ICR1, 분주 64
	ICR1  = 4999;   // TOP → 20ms(50Hz)
	/*OCR1C = 375;*/    // 0도 (1.5ms)
}
void uart_init(){
	DDRE =0x02;
	UBRR0L =16; UBRR0H=0;
	UCSR0A=0x20;
	UCSR0B=0x18;
	UCSR0C=0x06;
}
void set_servo(){
	OCR1C = 500;   //  90도 (2.0ms)
	_delay_ms(1000);
}
void turn_servo(int angle){
	// 각도 범위 제한 (안전장치)
	if (angle < 0)   angle = 0;
	if (angle > 180) angle = 180;

	// 펄스 폭 계산
	// 0.5us 단위이므로: 1ms = 2000틱, 2ms = 4000틱
	// 0° → 2000,  180° → 4000
	// OCR1A = 2000 + (angle / 180) * 2000
	OCR1C = 250 + ((long)angle * 250) / 180;
}
void Uart_Putstr(char *str){
	while(*str){
		while(!(UCSR0A&(1<<UDRE0)));
		UDR0 = *str++;
	}
}
char UART0_receive(void)
{
	while (!(UCSR0A & (1 << RXC0)));  // 수신 완료(RXC1) 대기
	return UDR0;                       // 받은 데이터 반환
}
// 각도를 숫자로 바로 받아서 int로 반환
int Get_angle(void){
	int num = 0;
	int got_digit = 0;   // 숫자를 하나라도 받았는지 표시

	while (1) {
		char c = UART0_receive();

		if (c >= '0' && c <= '9') {   // 숫자면 조립
			num = num * 10 + (c - '0');
			got_digit = 1;
		}
		else {                         // 숫자가 아닌 문자(엔터/공백 등)가 오면
			if (got_digit)             // 숫자를 이미 받았으면 → 입력 끝
			return num;
			// 아직 숫자 없으면 그냥 무시하고 계속 대기
		}
	}
}
// 정수를 UART로 출력
void Uart_Putnum(int num){
	char buf[6];
	itoa(num, buf, 10);   // stdlib.h의 itoa 사용 (10진수)
	Uart_Putstr(buf);
}	