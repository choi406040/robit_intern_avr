/*
 * Day4_Task03.c
 *
 * Created: 2026-08-01 오후 9:50:27
 * Author : dhcho
 */ 
#define F_CPU 16000000
#include <stdio.h>
#include <math.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

int Read_adc();
void Uart_Putstr(char *str);
int main(void)
{	DDRE =0x02;
	UBRR0L =16; UBRR0H=0;
	UCSR0A=0x20;
	UCSR0B=0x18;
	UCSR0C=0x06;
	
	DDRF =0x00;
	ADMUX=0x41;
	ADCSRA=0x87;
	sei();
    /* Replace with your application code */
    while (1)
    {
	    int Adc_value = Read_adc();
		if (Adc_value>651 || Adc_value<104){
			Uart_Putstr("out of range\r\n");
			continue;
		}
		float distance=37700 * pow(Adc_value, -1.189);
	    char buf[16];
	    sprintf(buf, "%d\r\n", (int)distance);   // 예: "512\r\n"
	    Uart_Putstr(buf);
    }
}
int Read_adc(){
	unsigned int adc_data=0;
	unsigned char channel =0x01;
	
	ADMUX=0x41|channel;
	ADCSRA |= (1<<ADSC);       // = 0x40
	while(ADCSRA & 0x40);      // ADSC가 0이 될 때까지(변환 완료) 대기
	adc_data = ADC;
	_delay_ms(100);
	return adc_data;
}
void Uart_Putstr(char *str){
	while(*str){
		while(!(UCSR0A&(1<<UDRE0)));
		UDR0 = *str++;
	}
}

