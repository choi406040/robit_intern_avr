/*
 * Day3_Task04.c
 *
 * Created: 2026-08-01 오후 4:46:50
 * Author : dhcho
 */ 
#define F_CPU 16000000
#include <avr/io.h>
#include <util/delay.h>

void Uart_Putch(int str);

int main(void)
{	DDRD |=(1<<3);
	PORTD|=(1<<3);
	
    /* Replace with your application code */
    while (1) 
    {	Uart_Putch('H');
		Uart_Putch('e');
		Uart_Putch('l');
		Uart_Putch('l');
		Uart_Putch('o');
		Uart_Putch(' ');
		Uart_Putch('W');
		Uart_Putch('o');
		Uart_Putch('r');
		Uart_Putch('l');
		Uart_Putch('d');
		Uart_Putch('!');
		Uart_Putch(' ');
		_delay_ms(500);
    }
}

void Uart_Putch(int str){
	PORTD=0b00000000;
	_delay_us(104);
	for (int i=1; i<=128; i*=2){   // ← 1부터 시작
		if (str & i){              // ← AND로 판정
			PORTD = 0b00001000;    // ← PD3 High
		}
		else{
			PORTD = 0b00000000;    // PD3 Low
		}
		_delay_us(104);
	}
	PORTD=0b00001000;
	_delay_us(104);
}

