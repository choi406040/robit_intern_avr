/*
 * Day3_Task02.c
 *
 * Created: 2026-07-30 오후 8:09:32
 * Author : dhcho
 */ 

#define  F_CPU 16000000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

unsigned char Uart_Getch(void);
void Uart_Putch(char *PutData);
void led_print(char data);
void left_led(void);
void right_led(void);
int main(void)
{	DDRA=0xFF;
	UBRR0L =16;
	UBRR0H = 0;
	UCSR0A = 0x20;
	UCSR0B = 0x18;
	UCSR0C =0x06;
	
	DDRE =0x02;
	
	SREG =0x80;
	
	PORTA=0xFF;
	
	/* Replace with your application code */
	while (1)
	{
		char recvData =Uart_Getch();
		led_print(recvData);
		
	}
}
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
//led 출력 함수
void led_print(char data){
	switch (data){
		case '0':
		PORTA=0xFE;
		Uart_Putch("0LED on ");
		break;
		case '1':
		PORTA=0b11111101;
		Uart_Putch("1LED on ");
		break;
		case '2':
		PORTA=0b11111011;
		Uart_Putch("2LED on ");
		break;
		case '3':
		PORTA=0b11110111;
		Uart_Putch("3LED on ");
		break;
		case '4':
		PORTA=0b11101111;
		Uart_Putch("4LED on ");
		break;
		case '5':
		PORTA=0b11011111;
		Uart_Putch("5LED on ");
		break;
		case '6':
		PORTA=0b10111111;
		Uart_Putch("6LED on ");
		break;
		case '7':
		PORTA=0b01111111;
		Uart_Putch("7LED on ");
		break;
		case '8':
		Uart_Putch("LEFT ");
		left_led();
		break;
		case '9':
		Uart_Putch("RIGHT ");
		right_led();
		break;
		default:
		Uart_Putch("ERROR NUM ");
		break;
		
	}
}
void left_led(){
	uint8_t temp;
	for (temp=0x01;temp!=0b10000000;temp=temp<<1){
		PORTA=~temp;
		_delay_ms(100);
	}
	PORTA=~temp;
	_delay_ms(100);
}
void right_led(){
	uint8_t temp;
	for (temp=0b10000000;temp!=0b00000001;temp=temp>>1){
		PORTA=~temp;
		_delay_ms(100);
	}
	PORTA=~temp;
	_delay_ms(100);
}