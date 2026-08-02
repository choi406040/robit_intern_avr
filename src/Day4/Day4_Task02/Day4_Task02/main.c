/*
 * Day4_Task02.c
 *
 * Created: 2026-07-31 오전 10:43:06
 * Author : dhcho
 */ 
#define F_CPU 16000000
#include  <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "clcd.h"
#include "i2c.h"

int timetable[7]={0,0,0,0,0,0,0}; //year,month, day, hour, min, sec, msec
int timetable_count=0;

unsigned int count1ms =0;
void set (void);
void sort_time_tabel();
void lcd_print();
int main(void)
{	set();
	i2c_lcd_init();

    /* Replace with your application code */
    while (1) 
    {
	    unsigned char chaneel =0x00;
	    ADMUX = 0x40 | chaneel;
	    ADCSRA |=0x40;
	    while((ADCSRA&0x10)==0);
		switch (timetable_count){
			case 0:
				timetable[timetable_count] = (long)ADC * 100 / 1024;
				break;
			case 1:
				timetable[timetable_count] =ADC;		//month 
				timetable[timetable_count]=timetable[timetable_count]*12/1024;
				timetable[timetable_count]++;
				break;
			case 2:
				timetable[timetable_count] =ADC;		//day인데 month에 따라 30,31일 구별 필요
				if (timetable[timetable_count-1]==1 || timetable[timetable_count-1]==3 || timetable[timetable_count-1]==5 || timetable[timetable_count-1]==7 || timetable[timetable_count-1]==8 || timetable[timetable_count-1]==10 || timetable[timetable_count-1]==12 ){
					timetable[timetable_count]*=31;
					timetable[timetable_count]/=1024;
					timetable[timetable_count]++;		//0~31
				}
				else if ( timetable[timetable_count-1]==2){
					timetable[timetable_count]*=28;
					timetable[timetable_count]/=1024;
					timetable[timetable_count]++;
				}
				else{
					timetable[timetable_count]*=30;
					timetable[timetable_count]/=1024;
					timetable[timetable_count]++;
				}
				break;
			case 3:			//hour
				timetable[timetable_count] =ADC;
				timetable[timetable_count]*=24;
				timetable[timetable_count]/=1024;
				break;
			case 4:			//min
				timetable[timetable_count] = (long)ADC * 60 / 1024;
				break;
			case 5:			//sec
				timetable[timetable_count] = (long)ADC * 60 / 1024;
				break;
			case 6:			//msec
				timetable[timetable_count] = (long)ADC * 100 / 1024;
				break;
			default:
				break;
		}
		lcd_print();
    }
}
void set (void){
	//4,5=switch(1,2),012=rs485
	DDRE=0x00;
	PORTE |= (1<<4)|(1<<5);
	//0,1=가변저항(입력)
	DDRF=0x00;
	
	EIMSK=0b00110000;
	EICRB=0x0A;
	
	ADMUX = 0x40;
	ADCSRA = 0x87;
	
	SREG =0x80;
}
void lcd_print(){
	int i;
	for (i=0;i<3;i++){
		i2c_lcd_goto_XY(0,i*2);
		char temp[3];
		sprintf(temp,"%02d",*(timetable+i));
		i2c_lcd_write_string(temp);
	}
	int j=0;
	for (i=3;i<6;i++){
		i2c_lcd_goto_XY(1,j*3);
		char temp[4];
		sprintf(temp,"%02d",*(timetable+i));
		i2c_lcd_write_string(temp);
		i2c_lcd_goto_XY(1,(j*3)+2);
		i2c_lcd_write_string(":");
		j++;
	}
	i2c_lcd_goto_XY(1,(j*3)-1);
	i2c_lcd_write_string(".");
	i2c_lcd_goto_XY(1,j*3);
	char temp[4];
	sprintf(temp,"%d",*(timetable+i));
	i2c_lcd_write_string(temp);
}
//timetable 정렬 및 시키기(60초=1분으로)
void sort_time_tabel(){
	if (timetable[6]>=100){		//ms
		timetable[6]=0;
		timetable[5]++;
	}
	if (timetable[5]>=60){		//sec
		timetable[5]=0;
		timetable[4]++;
	}
	if (timetable[4]>=60){		//min
		timetable[4]=0;
		timetable[3]++;
	}
	if (timetable[3]>=24){		//hour
		timetable[3]=0;
		timetable[2]++;
	}
	//day
	if (timetable[1]==1 || timetable[1]==3 || timetable[1]==5 || timetable[1]==7 || timetable[1]==8 || timetable[1]==10 || timetable[1]==12 ){
		if (timetable[2]>=32){
			timetable[2]=1;
			timetable[1]++;
		}
	}
	else if ( timetable[1]==2){
		if (timetable[2]>=29){
			timetable[2]=1;
			timetable[1]++;
		}
	}
	else{
		if (timetable[2]>=31){
			timetable[2]=1;
			timetable[1]++;
		}
	}
	//month
	if (timetable[1]>=13){
		timetable[1]=1;
		timetable[0]++;
	}
	
	
}
//스위치 1번 누르면...
ISR(INT4_vect){
	timetable_count++;
}
//스위치 2번 누르면 1ms씩 증가 표현
ISR(INT5_vect){
	TCNT0 =6;
	TCCR0 =0x04;
	TIMSK =0x01;
}
ISR(TIMER0_OVF_vect){
	count1ms++;
	if (count1ms==10){
		timetable[6]++;
		sort_time_tabel();
		count1ms=0;
	}
	TCNT0 =6;
}

