#define F_CPU 16000000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

int main(void)
{
	DDRB = 0x6F;			// PB0~PB3(IN1~IN4), PB5(OC1A), PB6(OC1B) 출력

	TCCR1A = 0xA2;			// COM1A1/COM1B1 = 비반전, WGM11 = 1
	TCCR1B = 0x1B;			// WGM13:12 = 11 (Mode 14), 분주비 64
	TCCR1C = 0x00;			// No Setting

	ICR1 = 249;				// TOP = 249 → 16MHz/(64*250) = 1kHz
	TCNT1 = 0x0000;			// TCNT1 초기화

	while(1)
	{
		PORTB = (PORTB & 0xF0) | 0x05;	// 모터 정방향
		OCR1A = ICR1 * 0.80;			// PB5 듀티비 80% 출력
		OCR1B = ICR1 * 0.80;			// PB6 듀티비 80% 출력
		_delay_ms(1000);

		PORTB = (PORTB & 0xF0) | 0x0A;	// 모터 역방향
		OCR1A = ICR1 * 0.80;			// PB5 듀티비 80% 출력
		OCR1B = ICR1 * 0.80;			// PB6 듀티비 80% 출력
		_delay_ms(1000);
	}
}