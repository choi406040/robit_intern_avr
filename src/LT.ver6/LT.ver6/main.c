 
#define F_CPU 16000000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <util/delay.h>
#include "clcd.h"
#include "i2c.h"
#include <math.h>

void led_init();
void motor_init();
void switch_init();
void lcd_init();
void IR_init();
void PSD_init();
void all_init();
int read_adc(int channel);
void calibration();
void line_tracing();
void line_tracing2();
void print_ir();
void print_weight_sum();
void value_move();
void in_5verticle();
void in_6verticle();
void parallelogram();
void print_verticle();
int PSD_distance();
void rotate_ccw(uint16_t duration_ms);		//반시계 방향으로 돌기
void forward_ccw(uint16_t duration_ms);		//앞으로 가기
void backward_ccw(uint16_t duration_ms);	//뒤로 가기
void black_line_tracing();					//검은판 라인트레이싱
void black_line_tracing2();					//검은판 라인트레이싱(verticle=2인식한 후 부터)
void black_line_tracing3();					//장애물 인식후 라인트레이싱
void black_line_tracing4();					//장애물 인식후 라인트레이싱
void print_norm();
void black_calibration();
void obstacle();							//가까운 장애물 인식하는 모드
void obstacle2();							//먼 장애물 인식하는 모드
void rotate_clock(uint16_t duration_ms);	//시계 방향으로 돌기

int base_speed=62;	//249*25/100;
volatile int eightescape=3;					//팔자를 탈출하기 위한 수직선 인식 (eightescape+2)가 되어야 평행사변형구간임.
volatile int mode=0;						//switch case 문에서 사용할 주행모드
volatile int stop_time=500;
ISR(INT4_vect){
	if (mode==0){
		mode=10;							//검은판 켈리브레이션
	}
	else if(mode==10){
		mode=11;							//각 켈리브레이션을 사이
	}
	else if (mode==11){
		mode=1;								//흰판 켈리브레이션
	}
}
ISR(INT5_vect){
	if (mode==1||mode ==10){
		mode=2;		//라인트레이싱
	}
	else{
		mode=3;		//평행사변형 라인트레이싱
	}
}
ISR(INT2_vect){
	if (mode==4){
		mode =5;		//검은판 라인트레이싱
	}
	else if (mode==5){
		mode=6;	//먼 장애물
	}
	else if (mode==6){
		mode =7;	//가까운 장애물
	}
	else{
		mode=4;		//PSD+LT
	}
	
	
	
}
ISR(INT3_vect){
	mode=5;
}

int finished=0;			//종료코드
int max_ir[6]={0,0,0,0,0,0};		
int min_ir[6]={1023,1023,1023,1023,1023,1023};
int max_ir_black[6]={0,0,0,0,0,0};
int min_ir_black[6]={1023,1023,1023,1023,1023,1023};
int current_ir[3][6]={{0,0,0,0,0,0},{0,0,0,0,0,0},{0,0,0,0,0,0}};
int norm_ir[6];
int weight[6]={-8,-4,-2,2,4,8};	//가중치
int weight_sum=0;				//가중치 합
int verticle=0;		//수직선 만난 횟수
int lcd_count=0;				//lcd를 몇번마다 출력할지 판단할 변수
uint8_t branch_latch = 0;
int latch_dir = 0;
uint8_t latch_age    = 0; 
uint8_t prev_all = 0;// 직전프레임
uint8_t escape_done = 0;
//   한쪽에 치우쳐 물게 함. 부호가 따라갈 변을 결정.
//   (+) 왼쪽 변을 로봇 왼쪽에 두고 따라감
//   (-) 오른쪽 변을 로봇 오른쪽에 두고 따라감
int left_parallcount=0;			//평행사변형 구간에서 왼쪽벽에 부딪힌 횟수( 오른쪽벽에 부딪힌 후 부터 카운트 시작)
int right_parallcount=0;		//평행사변형 구간에서 오른족벽에 부딪힌 횟수
uint8_t parall_entered = 0;		//평행사변형 구간에서 벗어나면 1로 변환 (나중에 다시 평행사변형 구간으로 진입하지 못하게 막음
int yes_650=0;					//psd센서의 raw값이 넘어갔는지 판단
int yes_500=0;					//psd세서의 raw값이 넘어갔는지 판단
int RL_edge=0;					//6번 ir이 <50인지 판단하는 변수
int left_edge=0;				//1번 ir이 <50인지 판단하는 변수
int main(void)
{
	all_init();
	sei();
	static uint8_t lcd_count = 0;
	
	//uint8_t stop_count=0;
    /* Replace with your application code */
    while (1) 
    {	
		if (finished==1){
			return 0;
		}
		if (!parall_entered &&verticle>=(eightescape+2)){
			parall_entered=1;
			mode=3;
			print_verticle();
			i2c_lcd_goto_XY(1,8);
			i2c_lcd_write_string("mode3");
		}
		switch (mode){
			case 1:
				calibration();
				/*print_ir();*/
				break;
			case 2:
				line_tracing();
				if (++lcd_count >= 50) {   // 50번에 1번만 LCD 갱신
					lcd_count = 0;
					print_verticle();
				}
				break;
			case 3:
				parallelogram();
				break;
			case 4:{
				int adc_value=read_adc(1);
				if (++lcd_count >= 50) {   // 50번에 1번만 LCD 갱신
					lcd_count = 0;
					
					i2c_lcd_goto_XY(0,0);
					i2c_lcd_write_string("adc: ");
					i2c_lcd_goto_XY(0,3);
					char str[16];
					sprintf(str,"%5d",adc_value);
					i2c_lcd_write_string(str);
				}
				
				//verticle_print (위치를 다르게)
				/*i2c_lcd_goto_XY(1,0);
				i2c_lcd_write_string("verticle: ");
				i2c_lcd_goto_XY(1,9);
				char temp[3];
				sprintf(temp,"%2d",verticle);
				i2c_lcd_write_string(temp);*/
				if (adc_value>=650){
					yes_650=1;
				}
				if (adc_value<=500&&yes_650){
					i2c_lcd_goto_XY(0,12);
					i2c_lcd_write_string("stop");
					if (adc_value<=150){
						i2c_lcd_goto_XY(0,12);
						i2c_lcd_write_string("go!   ");
						yes_650=0;
					}
					
					OCR1A=0;
					OCR1B=0;
				}
				else{
					line_tracing2();
				}
					//이걸 함수안에 넣을지 고민중
				
				break;
				
			}
			case 5:
				if (++lcd_count>=50){
					print_verticle();
					lcd_count=0;
				}
				
				black_line_tracing();
				
				break;
			case 6:
				obstacle();
				break;
			case 7:
				obstacle2();
				break;
			case 8:
				black_line_tracing2();
				break;
			case 9:
				black_line_tracing3();
				break;
			case 10:
				black_calibration();
				break;
			case 11:
				PORTA=0xFF;
				break;
			case 12:
				black_line_tracing4();
				break;
			default:
				break;
		}
				
    }
}
//led 세팅하기
void led_init(){
	//출력으로 설정하기
	DDRA=0xFF;
	//시작은 꺼짐 설정
	PORTA=0xFF;
}
//INIT 
void motor_init(){
	DDRB = 0b01101111;			// PB0~PB3(IN1~IN4), PB5(OC1A), PB6(OC1B) 출력
	TCCR1A = 0xA2;			// COM1A1/COM1B1 = 비반전, WGM11 = 10
	TCCR1B = 0x1B;			// WGM13:12 = 11 (Mode 14), 분주비 64
	TCCR1C = 0x00;			// No Setting
	
	ICR1=249;
	TCNT1=0x0000;
	
	PORTB = (PORTB & 0xF0) | 0x05;	// 모터 정방향
}
void switch_init(){
	DDRE = 0b11001011;
	
	// PD0(SCL), PD1(SDA), PD2(INT2), PD3(INT3) 입력
	DDRD  &= ~((1<<0)|(1<<1)|(1<<2)|(1<<3));
	PORTD |=  (1<<0)|(1<<1)|(1<<2)|(1<<3);     // 풀업 활성화
	PORTE |=  (1<<4)|(1<<5);
	
	EIMSK = 0b00111100;   // INT2,3,4,5 활성화 (기존 0b00110100 → INT3 추가)
	EICRA = 0b10100000;   // INT3, INT2 하강엣지 (기존 0b00100000)
	EICRB = 0b00001010;   // INT5, INT4 하강엣지
}
void lcd_init(){
	i2c_lcd_init();
}
void IR_init(){
	//PF2~7, psd는 0,1번인데 일단 플로팅상태여서 출력으로 설정
	DDRF=0b00000011;
	//기준전압 avcc, adc활성화, 분주비 128
	ADMUX=0x40;
	ADCSRA=0x87 ;
}
void PSD_init(){
	DDRF=0b00000001;
}
void all_init(){
	led_init();
	motor_init();
	switch_init();
	lcd_init();      // ← 여기까지 오면 i2c_lcd_init() 통과
	IR_init();
	PSD_init();
}
int read_adc(int channel){
	ADMUX=0x40|channel;
	ADCSRA|=0x40;
	while((ADCSRA&(1<<ADSC))!=0);
	return ADC;
}
void calibration(){
	value_move();
	for (int i=2;i<8;i++){
		int adc_value=read_adc(i);
		current_ir[0][i-2]=adc_value;
		if (max_ir[i-2]<=adc_value){
			max_ir[i-2]=adc_value;
		}
		if (min_ir[i-2]>=adc_value){
			min_ir[i-2]=adc_value;
		}
	}
	//led 비추기 max-min 값의 중앙값보다 크면 led 출력하기
	for (int i=2;i<8;i++){
		// 수정
		if (current_ir[0][i-2] < (max_ir[i-2] + min_ir[i-2]) / 2) {
			PORTA &= ~(1 << (i-1));   // 켜기 (해당 비트만 0으로)
			} else {
			PORTA |= (1 << (i-1));    // 끄기 (해당 비트만 1로)
		}
	}
}
void line_tracing(){
	value_move();
	for (int i=2;i<8;i++){
		int adc_value=read_adc(i);
		current_ir[0][i-2]=adc_value;
		int range=1;
		if ((max_ir[i-2]-min_ir[i-2])==0){
			range=1;
		}
		else {
			range=(max_ir[i-2]-min_ir[i-2]);
		}
		norm_ir[i-2]=(long)(((current_ir[0][i-2]+current_ir[1][i-2]+current_ir[2][i-2])/3)-min_ir[i-2])*100/range;
	}
	//led 비추기 정규화한 값이 50보다 작으면(검은선이면) 불키기
	for (int i=2;i<8;i++){
		if (norm_ir[i-2] < 50) {
			PORTA &= ~(1 << (i-1));
			} else {
			PORTA |= (1 << (i-1));
		}
	}
	//가중치 합구하기+led 켜진 갯수 구하기
	weight_sum=0;
	int on_line=0;
	for (int i=0;i<6;i++){
		weight_sum += norm_ir[i]*weight[i];
		if (norm_ir[i] < 50) on_line++;
	}
	in_5verticle();		//수직으로 led를 만났는가(falling edge)
	if ((verticle == 1 || verticle == 2) && on_line == 6){
		rotate_ccw(300);
	}
	if (verticle == 3 && on_line == 5){
		rotate_ccw(200);
	}
	//마지막 방향
	static int last_dir = 0;
	if (on_line > 0 && weight_sum != 0){
		last_dir = (weight_sum > 0) ? 1 : -1;
	}
	//가중치를 나눌값->control로 이어짐
	int weight_cal = 800;
	long control;
	if (on_line == 0){
		// 라인 완전히 놓침 → 마지막 방향으로 최대 조향
		control = (long)ICR1 * 25/100 * last_dir;
		} else {
		control = (long)ICR1 * 25/100 * weight_sum / weight_cal;   // P only
	}
	
	
	
	int min_duty = ICR1 * 20 / 100;

	int ocr_a = base_speed + control;   // 오른쪽
	int ocr_b = base_speed - control;   // 왼쪽
	uint8_t dir = 0x00;                 // 방향 비트를 새로 조립 (덮어쓰기 X)

	// 오른쪽 (OCR1A)
	if (ocr_a < min_duty) {
		dir  |= 0x02;                   // 오른쪽만 역방향
		ocr_a = min_duty - ocr_a;
		} else {
		dir  |= 0x01;                   // 오른쪽 정방향
	}
	if (ocr_a > ICR1) ocr_a = ICR1;     // 상한 클램프 (여기가 ICR1이어야 함)

	// 왼쪽 (OCR1B)
	if (ocr_b < min_duty) {
		dir  |= 0x08;                   // 왼쪽만 역방향
		ocr_b = min_duty - ocr_b;
		} else {
		dir  |= 0x04;                   // 왼쪽 정방향
	}
	if (ocr_b > ICR1) ocr_b = ICR1;

	PORTB = (PORTB & 0xF0) | dir;       // 마지막에 딱 한 번만 쓰기
	OCR1A = ocr_a;   // 오른쪽
	OCR1B = ocr_b;   // 왼쪽
}
void line_tracing2(){					//psd+주차하는 구간에서 쓸 라인트레이싱
	value_move();
	for (int i=2;i<8;i++){
		int adc_value=read_adc(i);
		current_ir[0][i-2]=adc_value;
		int range=1;
		if ((max_ir[i-2]-min_ir[i-2])==0){
			range=1;
		}
		else {
			range=(max_ir[i-2]-min_ir[i-2]);
		}
		norm_ir[i-2]=(long)(((current_ir[0][i-2]+current_ir[1][i-2]+current_ir[2][i-2])/3)-min_ir[i-2])*100/range;
	}
	//led 비추기 정규화한 값이 50보다 작으면(검은선이면) 불키기
	for (int i=2;i<8;i++){
		if (norm_ir[i-2] < 50) {
			PORTA &= ~(1 << (i-1));
			} else {
			PORTA |= (1 << (i-1));
		}
	}
	//가중치 합구하기+led 켜진 갯수 구하기
	weight_sum=0;
	int on_line=0;
	for (int i=0;i<6;i++){
		weight_sum += norm_ir[i]*weight[i];
		if (norm_ir[i] < 50) on_line++;
	}
	in_6verticle();		//수직으로 led를 만났는가(falling edge)
	
	
	if (verticle==3&&on_line==6){
		//black_linetracing모드로 ㄱㄱ
		i2c_lcd_goto_XY(1,7);
		char temp[3];
		sprintf(temp,"%2d",verticle);
		i2c_lcd_write_string(temp);
		
		verticle=0;
		mode =5;
		return ;
	}
	//살짝 행동이 커서 초를 줄어야 할수도 이건 나중에 
	if (verticle==2&&on_line==6){
		rotate_ccw(200);
		return ;
	}
	if (verticle==1&&on_line==6){
		i2c_lcd_goto_XY(1,12);
		i2c_lcd_write_string("PARK");
		i2c_lcd_goto_XY(1,7);
		char temp[3];
		sprintf(temp,"%2d",verticle);
		i2c_lcd_write_string(temp);
		OCR1A=0;
		OCR1B=0;
		_delay_ms(3000);
		i2c_lcd_goto_XY(1,12);
		i2c_lcd_write_string("    ");
		//먼저 조금 돌리고 iR가운데 두개가 검은선 인식할때까지 200ms씩 돌리기? (아이디어)
		rotate_ccw(1800);
		return ;
	}
	/*if (verticle==0&&on_line==6&&yes_650){
		rotate_ccw(200);
		return ;
	}*/
	//마지막 방향
	static int last_dir = 0;
	if (on_line > 0 && weight_sum != 0){
		last_dir = (weight_sum > 0) ? 1 : -1;
	}
	//가중치를 나눌값->control로 이어짐
	int weight_cal = 800;
	long control;
	if (on_line == 0){
		// 라인 완전히 놓침 → 마지막 방향으로 최대 조향
		control = (long)ICR1 * 25/100 * last_dir;
		} else {
		control = (long)ICR1 * 25/100 * weight_sum / weight_cal;   // P only
	}
	
	
	
	int min_duty = ICR1 * 20 / 100;

	int ocr_a = base_speed + control;   // 오른쪽
	int ocr_b = base_speed - control;   // 왼쪽
	uint8_t dir = 0x00;                 // 방향 비트를 새로 조립 (덮어쓰기 X)

	// 오른쪽 (OCR1A)
	if (ocr_a < min_duty) {
		dir  |= 0x02;                   // 오른쪽만 역방향
		ocr_a = min_duty - ocr_a;
		} else {
		dir  |= 0x01;                   // 오른쪽 정방향
	}
	if (ocr_a > ICR1) ocr_a = ICR1;     // 상한 클램프 (여기가 ICR1이어야 함)

	// 왼쪽 (OCR1B)
	if (ocr_b < min_duty) {
		dir  |= 0x08;                   // 왼쪽만 역방향
		ocr_b = min_duty - ocr_b;
		} else {
		dir  |= 0x04;                   // 왼쪽 정방향
	}
	if (ocr_b > ICR1) ocr_b = ICR1;

	PORTB = (PORTB & 0xF0) | dir;       // 마지막에 딱 한 번만 쓰기
	OCR1A = ocr_a;   // 오른쪽
	OCR1B = ocr_b;   // 왼쪽
}
void print_ir(){
	for (int i=0;i<3;i++){
		i2c_lcd_goto_XY(0,i*4);
		char temp[5];
		sprintf(temp,"%4d",current_ir[0][i]);
		i2c_lcd_write_string(temp);
	}
	for (int i=0;i<3;i++){
		i2c_lcd_goto_XY(1,i*4);
		char temp[5];
		sprintf(temp,"%4d",current_ir[0][i+3]);
		i2c_lcd_write_string(temp);
	}
}
void print_weight_sum(){
	
	i2c_lcd_goto_XY(0,0);
	i2c_lcd_write_string("                ");
	i2c_lcd_goto_XY(1,0);
	i2c_lcd_write_string("                ");
	i2c_lcd_goto_XY(0,0);
	i2c_lcd_write_string("weight_sum");
	i2c_lcd_goto_XY(1,0);
	char str[8];
	int temp;
	temp=weight_sum;
	sprintf(str,"%8d",temp);
	i2c_lcd_write_string(str);
	}
void value_move(){
	for (int i=1;i>=0;i--){
		for (int j=0;j<6;j++){
			current_ir[i+1][j]=current_ir[i][j];
		}
	}
}
void in_5verticle (){
	int on_line=0;
	for (int i=0;i<6;i++){
		if (norm_ir[i]<=50){
			on_line++;
		}
	}
	uint8_t all_line = 0;
	if (on_line>=5){
		all_line = 1;
	}
	else{
		all_line = 0;
	}
	if (!all_line && prev_all) {   // ★ 1→0 될 때 = falling edge
		verticle++;
	}
	prev_all = all_line;
}
void in_6verticle(){
	uint8_t all_line = 0;
	if (norm_ir[0]<=50&&norm_ir[1]<=50&&norm_ir[2]<=50&&norm_ir[3]<=50&&norm_ir[4]<=50&&norm_ir[5]<=50){
		all_line = 1;
	}
	else{
		all_line = 0;
	}
	if (!all_line && prev_all) {   // ★ 1→0 될 때 = falling edge
		verticle++;
	}
	prev_all = all_line;
}
void parallelogram(){		//평행사변형
	value_move();
	for (int i=2;i<8;i++){
		int adc_value = read_adc(i);
		current_ir[0][i-2] = adc_value;
		int range = (max_ir[i-2]-min_ir[i-2])==0 ? 1 : (max_ir[i-2]-min_ir[i-2]);
		norm_ir[i-2] = (long)((current_ir[0][i-2])-min_ir[i-2])*100/range;
	}
	for (int i=2;i<8;i++){
		if (norm_ir[i-2] < 50) PORTA &= ~(1 << (i-1));
		else                   PORTA |=  (1 << (i-1));
	}
	if (norm_ir[0]<50){
		if (right_parallcount>=1){
			left_parallcount++;
			if (left_parallcount>=2){
				OCR1A=ICR1*30/100;
				OCR1B=0;
				_delay_ms(800);
				mode=4;
				parall_entered = 1;
				verticle=0;
				i2c_lcd_goto_XY(1,0);
				i2c_lcd_write_string("                ");
				return ;
				
			}
		}
		PORTB = (PORTB & 0xF0) | 0x0A;
		OCR1A=ICR1*30/100;
		OCR1B=0;
		_delay_ms(500);
		return ;
	}
	if (norm_ir[5]<50){
		right_parallcount++;
		PORTB = (PORTB & 0xF0) | 0x0A;
		OCR1B=ICR1*30/100;
		OCR1A=0;
		_delay_ms(500);
		
		return ;
	}
	/*weight_sum = 0;
	int on_line = 0;
	for (int i=0;i<6;i++){
		weight_sum += norm_ir[i]*weight[i];
		if (norm_ir[i] < 50) on_line++;
	}*/
	
	

	PORTB = (PORTB & 0xF0) | 0x05;           // 정방향
	OCR1A = ICR1*30/100;
	OCR1B = ICR1*30/100;
}
void print_verticle(){
	i2c_lcd_goto_XY(0,0);
	i2c_lcd_write_string("verticle");
	i2c_lcd_goto_XY(1,0);
	char temp[3];
	sprintf(temp,"%2d",verticle);
	i2c_lcd_write_string(temp);
}
int PSD_distance(){
	int adc_value=read_adc(1);
	return (int)(37700 * pow(adc_value, -1.189));
}
void rotate_ccw(uint16_t duration_ms){
	// 반시계방향: 왼쪽 역회전 + 오른쪽 정회전
	PORTB = (PORTB & 0xF0) | 0x09;
	OCR1A = ICR1*30/100;   // 오른쪽
	OCR1B = ICR1*30/100;   // 왼쪽
	for (uint16_t t=0; t<duration_ms; t+=10){
		_delay_ms(10);
	}

	OCR1A = 0;
	OCR1B = 0;
}
void forward_ccw(uint16_t duration_ms){
	// 반시계방향: 왼쪽 정회전 + 오른쪽 정회전
	PORTB = (PORTB & 0xF0) | 0x05;
	OCR1A = ICR1*30/100;   // 오른쪽
	OCR1B = ICR1*30/100;   // 왼쪽

	for (uint16_t t=0; t<duration_ms; t+=10){
		_delay_ms(10);
	}

	OCR1A = 0;
	OCR1B = 0;
}
void backward_ccw(uint16_t duration_ms){
	// 반시계방향: 왼쪽 정회전 + 오른쪽 정회전
	PORTB = (PORTB & 0xF0) | 0x0A;
	OCR1A = ICR1*30/100;   // 오른쪽
	OCR1B = ICR1*30/100;   // 왼쪽

	for (uint16_t t=0; t<duration_ms; t+=10){
		_delay_ms(10);
	}

	OCR1A = 0;
	OCR1B = 0;
}
void black_line_tracing(){		//검은판 라인트레이싱
	value_move();
	for (int i=2;i<8;i++){
		int adc_value=read_adc(i);
		current_ir[0][i-2]=adc_value;
		int range=1;
		if ((max_ir_black[i-2]-min_ir_black[i-2])==0){
			range=1;
		}
		else {
			range=(max_ir_black[i-2]-min_ir_black[i-2]);
		}
		norm_ir[i-2]=100-(long)(((current_ir[0][i-2]+current_ir[1][i-2]+current_ir[2][i-2])/3)-min_ir_black[i-2])*100/range;
	}
	//led 비추기 정규화한 값이 50보다 작으면(흰선이면) 불키기
	for (int i=2;i<8;i++){
		if (norm_ir[i-2] < 50) {
			PORTA &= ~(1 << (i-1));
			} else {
			PORTA |= (1 << (i-1));
		}
	}
	//가중치 합구하기+led 켜진 갯수 구하기
	weight_sum=0;
	int on_line=0;
	for (int i=0;i<6;i++){
		weight_sum += norm_ir[i]*weight[i];
		if (norm_ir[i] < 50) on_line++;
	}
	in_6verticle();
	if (verticle==1&&on_line==6){
		mode=8;
		return;
	}
	if ((current_ir[0][5]>=((max_ir_black[5]+min_ir_black[5])/2))||(current_ir[0][0]>=((max_ir_black[0]+min_ir_black[0])/2))){
		/*if (RL_edge==3){
			forward_ccw(150);
			rotate_ccw(1300)
		}*/
		forward_ccw(100);
		current_ir[0][5]=(max_ir_black[5]+min_ir_black[5])/2;
		current_ir[0][0]=((max_ir_black[0]+min_ir_black[0])/2);
		RL_edge++;
		return ;
	}
	
	
	//마지막 방향
	static int last_dir = 0;
	if (on_line > 0 && weight_sum != 0){
		last_dir = (weight_sum > 0) ? 1 : -1;
	}
	//가중치를 나눌값->control로 이어짐
	int weight_cal = 800;
	long control;
	if (on_line == 0){
		// 라인 완전히 놓침 → 마지막 방향으로 최대 조향
		control = (long)ICR1 * 30/100 * last_dir;
		} else {
		control = (long)ICR1 * 30/100 * weight_sum / weight_cal;   // P only
	}
	
	
	int min_duty = ICR1 * 20 / 100;

	int ocr_a = base_speed + control;   // 오른쪽
	int ocr_b = base_speed - control;   // 왼쪽
	uint8_t dir = 0x00;                 // 방향 비트를 새로 조립 (덮어쓰기 X)

	// 오른쪽 (OCR1A)
	if (ocr_a < min_duty) {
		dir  |= 0x02;                   // 오른쪽만 역방향
		ocr_a = min_duty - ocr_a;
		} else {
		dir  |= 0x01;                   // 오른쪽 정방향
	}
	if (ocr_a > ICR1) ocr_a = ICR1;     // 상한 클램프 (여기가 ICR1이어야 함)

	// 왼쪽 (OCR1B)
	if (ocr_b < min_duty) {
		dir  |= 0x08;                   // 왼쪽만 역방향
		ocr_b = min_duty - ocr_b;
		} else {
		dir  |= 0x04;                   // 왼쪽 정방향
	}
	if (ocr_b > ICR1) ocr_b = ICR1;

	PORTB = (PORTB & 0xF0) | dir;       // 마지막에 딱 한 번만 쓰기
	OCR1A = ocr_a;   // 오른쪽
	OCR1B = ocr_b;   // 왼쪽
	
}
void black_line_tracing2(){
	value_move();
	for (int i=2;i<8;i++){
		int adc_value=read_adc(i);
		current_ir[0][i-2]=adc_value;
		int range=1;
		if ((max_ir_black[i-2]-min_ir_black[i-2])==0){
			range=1;
		}
		else {
			range=(max_ir_black[i-2]-min_ir_black[i-2]);
		}
		norm_ir[i-2]=100-(long)(((current_ir[0][i-2]+current_ir[1][i-2]+current_ir[2][i-2])/3)-min_ir_black[i-2])*100/range;
	}
	//led 비추기 정규화한 값이 50보다 작으면(흰선이면) 불키기
	for (int i=2;i<8;i++){
		if (norm_ir[i-2] < 50) {
			PORTA &= ~(1 << (i-1));
			} else {
			PORTA |= (1 << (i-1));
		}
	}
	//가중치 합구하기+led 켜진 갯수 구하기
	weight_sum=0;
	int on_line=0;
	for (int i=0;i<6;i++){
		weight_sum += norm_ir[i]*weight[i];
		if (norm_ir[i] < 50) on_line++;
	}
	in_6verticle();
	if (verticle==3&&on_line==6){
		OCR1A=0;
		OCR1B=0;
		_delay_ms(1000);
		if(read_adc(1)>=350){
			mode=6;
			return ;
		}
		else{
			mode=7;
			return;
		}
		
	}
	if (verticle==2&&on_line==6){
		OCR1A=0;
		OCR1B=0;
		_delay_ms(2000);
		backward_ccw(100);
		return ;
				
	}
	
	
	//마지막 방향
	static int last_dir = 0;
	if (on_line > 0 && weight_sum != 0){
		last_dir = (weight_sum > 0) ? 1 : -1;
	}
	//가중치를 나눌값->control로 이어짐
	int weight_cal = 800;
	long control;
	if (on_line == 0){
		// 라인 완전히 놓침 → 마지막 방향으로 최대 조향
		control = (long)ICR1 * 30/100 * last_dir;
		} else {
		control = (long)ICR1 * 30/100 * weight_sum / weight_cal;   // P only
	}
	
	
	int min_duty = ICR1 * 20 / 100;

	int ocr_a = base_speed + control;   // 오른쪽
	int ocr_b = base_speed - control;   // 왼쪽
	uint8_t dir = 0x00;                 // 방향 비트를 새로 조립 (덮어쓰기 X)

	// 오른쪽 (OCR1A)
	if (ocr_a < min_duty) {
		dir  |= 0x02;                   // 오른쪽만 역방향
		ocr_a = min_duty - ocr_a;
		} else {
		dir  |= 0x01;                   // 오른쪽 정방향
	}
	if (ocr_a > ICR1) ocr_a = ICR1;     // 상한 클램프 (여기가 ICR1이어야 함)

	// 왼쪽 (OCR1B)
	if (ocr_b < min_duty) {
		dir  |= 0x08;                   // 왼쪽만 역방향
		ocr_b = min_duty - ocr_b;
		} else {
		dir  |= 0x04;                   // 왼쪽 정방향
	}
	if (ocr_b > ICR1) ocr_b = ICR1;

	PORTB = (PORTB & 0xF0) | dir;       // 마지막에 딱 한 번만 쓰기
	OCR1A = ocr_a;   // 오른쪽
	OCR1B = ocr_b;   // 왼쪽
	
}
void black_line_tracing3(){		//가까운 장애물 기준으로 만든 트레이싱
	value_move();
	for (int i=2;i<8;i++){
		int adc_value=read_adc(i);
		current_ir[0][i-2]=adc_value;
		int range=1;
		if ((max_ir_black[i-2]-min_ir_black[i-2])==0){
			range=1;
		}
		else {
			range=(max_ir_black[i-2]-min_ir_black[i-2]);
		}
		norm_ir[i-2]=100-(long)(((current_ir[0][i-2]+current_ir[1][i-2]+current_ir[2][i-2])/3)-min_ir_black[i-2])*100/range;
	}
	//led 비추기 정규화한 값이 50보다 작으면(흰선이면) 불키기
	for (int i=2;i<8;i++){
		if (norm_ir[i-2] < 50) {
			PORTA &= ~(1 << (i-1));
			} else {
			PORTA |= (1 << (i-1));
		}
	}
	//가중치 합구하기+led 켜진 갯수 구하기
	weight_sum=0;
	int on_line=0;
	for (int i=0;i<6;i++){
		weight_sum += norm_ir[i]*weight[i];
		if (norm_ir[i] < 50) on_line++;
	}
	in_6verticle();
	if (verticle==2&&on_line==6){
		forward_ccw(1500);
		_delay_ms(50000);
		finished=1;
		
	}
	
	
	//마지막 방향
	static int last_dir = 0;
	if (on_line > 0 && weight_sum != 0){
		last_dir = (weight_sum > 0) ? 1 : -1;
	}
	//가중치를 나눌값->control로 이어짐
	int weight_cal = 800;
	long control;
	if (on_line == 0){
		// 라인 완전히 놓침 → 마지막 방향으로 최대 조향
		control = (long)ICR1 * 30/100 * last_dir;
		} else {
		control = (long)ICR1 * 30/100 * weight_sum / weight_cal;   // P only
	}
	
	
	int min_duty = ICR1 * 20 / 100;

	int ocr_a = base_speed + control;   // 오른쪽
	int ocr_b = base_speed - control;   // 왼쪽
	uint8_t dir = 0x00;                 // 방향 비트를 새로 조립 (덮어쓰기 X)

	// 오른쪽 (OCR1A)
	if (ocr_a < min_duty) {
		dir  |= 0x02;                   // 오른쪽만 역방향
		ocr_a = min_duty - ocr_a;
		} else {
		dir  |= 0x01;                   // 오른쪽 정방향
	}
	if (ocr_a > ICR1) ocr_a = ICR1;     // 상한 클램프 (여기가 ICR1이어야 함)

	// 왼쪽 (OCR1B)
	if (ocr_b < min_duty) {
		dir  |= 0x08;                   // 왼쪽만 역방향
		ocr_b = min_duty - ocr_b;
		} else {
		dir  |= 0x04;                   // 왼쪽 정방향
	}
	if (ocr_b > ICR1) ocr_b = ICR1;

	PORTB = (PORTB & 0xF0) | dir;       // 마지막에 딱 한 번만 쓰기
	OCR1A = ocr_a;   // 오른쪽
	OCR1B = ocr_b;   // 왼쪽
	
}
void black_line_tracing4(){		//가까운 장애물 기준으로 만든 트레이싱
	value_move();
	for (int i=2;i<8;i++){
		int adc_value=read_adc(i);
		current_ir[0][i-2]=adc_value;
		int range=1;
		if ((max_ir_black[i-2]-min_ir_black[i-2])==0){
			range=1;
		}
		else {
			range=(max_ir_black[i-2]-min_ir_black[i-2]);
		}
		norm_ir[i-2]=100-(long)(((current_ir[0][i-2]+current_ir[1][i-2]+current_ir[2][i-2])/3)-min_ir_black[i-2])*100/range;
	}
	//led 비추기 정규화한 값이 50보다 작으면(흰선이면) 불키기
	for (int i=2;i<8;i++){
		if (norm_ir[i-2] < 50) {
			PORTA &= ~(1 << (i-1));
			} else {
			PORTA |= (1 << (i-1));
		}
	}
	//가중치 합구하기+led 켜진 갯수 구하기
	weight_sum=0;
	int on_line=0;
	for (int i=0;i<6;i++){
		weight_sum += norm_ir[i]*weight[i];
		if (norm_ir[i] < 50) on_line++;
	}
	in_6verticle();
	if (verticle==1&&on_line==6){
		rotate_clock(300);
		return ;
	}
	if (verticle==3&&on_line==6){
		forward_ccw(1500);
		finished=1;
		_delay_ms(50000);
		
	}
	
	
	//마지막 방향
	static int last_dir = 0;
	if (on_line > 0 && weight_sum != 0){
		last_dir = (weight_sum > 0) ? 1 : -1;
	}
	//가중치를 나눌값->control로 이어짐
	int weight_cal = 800;
	long control;
	if (on_line == 0){
		// 라인 완전히 놓침 → 마지막 방향으로 최대 조향
		control = (long)ICR1 * 30/100 * last_dir;
		} else {
		control = (long)ICR1 * 30/100 * weight_sum / weight_cal;   // P only
	}
	
	
	int min_duty = ICR1 * 20 / 100;

	int ocr_a = base_speed + control;   // 오른쪽
	int ocr_b = base_speed - control;   // 왼쪽
	uint8_t dir = 0x00;                 // 방향 비트를 새로 조립 (덮어쓰기 X)

	// 오른쪽 (OCR1A)
	if (ocr_a < min_duty) {
		dir  |= 0x02;                   // 오른쪽만 역방향
		ocr_a = min_duty - ocr_a;
		} else {
		dir  |= 0x01;                   // 오른쪽 정방향
	}
	if (ocr_a > ICR1) ocr_a = ICR1;     // 상한 클램프 (여기가 ICR1이어야 함)

	// 왼쪽 (OCR1B)
	if (ocr_b < min_duty) {
		dir  |= 0x08;                   // 왼쪽만 역방향
		ocr_b = min_duty - ocr_b;
		} else {
		dir  |= 0x04;                   // 왼쪽 정방향
	}
	if (ocr_b > ICR1) ocr_b = ICR1;

	PORTB = (PORTB & 0xF0) | dir;       // 마지막에 딱 한 번만 쓰기
	OCR1A = ocr_a;   // 오른쪽
	OCR1B = ocr_b;   // 왼쪽
	
}
void print_norm(){
	for (int i=0;i<3;i++){
		i2c_lcd_goto_XY(0,i*4);
		char temp[5];
		sprintf(temp,"%4d",norm_ir[i]);
		i2c_lcd_write_string(temp);
	}
	for (int i=0;i<3;i++){
		i2c_lcd_goto_XY(1,i*4);
		char temp[5];
		sprintf(temp,"%4d",norm_ir[i+3]);
		i2c_lcd_write_string(temp);
	}
}
void black_calibration(){
	value_move();
	for (int i=2;i<8;i++){
		int adc_value=read_adc(i);
		current_ir[0][i-2]=adc_value;
		if (max_ir_black[i-2]<=adc_value){
			max_ir_black[i-2]=adc_value;
		}
		if (min_ir_black[i-2]>=adc_value){
			min_ir_black[i-2]=adc_value;
		}
	}
	//led 비추기 max-min 값의 중앙값보다 크면 led 출력하기
	for (int i=2;i<8;i++){
		// 수정
		if (current_ir[0][i-2] < (max_ir_black[i-2] + min_ir_black[i-2]) / 2) {
			PORTA &= ~(1 << (i-1));   // 켜기 (해당 비트만 0으로)
			} else {
			PORTA |= (1 << (i-1));    // 끄기 (해당 비트만 1로)
		}
	}
}
//처음에 psd값 인식해서 300이하면 먼장애물 모드, 이상이면 가까운 모드로 결정
//가까운 모드
void obstacle(){
	int adc_value=read_adc(1);
	i2c_lcd_goto_XY(0,0);
	i2c_lcd_write_string("adc: ");
	i2c_lcd_goto_XY(0,3);
	char str[16];
	sprintf(str,"%5d",adc_value);
	i2c_lcd_write_string(str);
	
	if (adc_value>=400){
		forward_ccw(100);
	}
	else{
		rotate_ccw(600);
		forward_ccw(1000);
		mode=9;
		verticle=0;
	}
}
//mode7, 먼장애물 모드
void obstacle2(){
	int adc_value=read_adc(1);
	i2c_lcd_goto_XY(0,0);
	i2c_lcd_write_string("adc: ");
	i2c_lcd_goto_XY(0,3);
	char str[16];
	sprintf(str,"%5d",adc_value);
	i2c_lcd_write_string(str);
	if (adc_value>=500){
		yes_500=1;
	}
	if (adc_value<=400&&yes_500){
		rotate_ccw(600);
		forward_ccw(1000);
		mode=12;
		verticle=0;
	}
	else{
		forward_ccw(100);
	}
}
void rotate_clock(uint16_t duration_ms){
	// 시계방향: 왼쪽 정회전 + 오른쪽 역회전
	PORTB = (PORTB & 0xF0) | 0x06;
	OCR1A = ICR1*30/100;   // 오른쪽
	OCR1B = ICR1*30/100;   // 왼쪽
	for (uint16_t t=0; t<duration_ms; t+=10){
		_delay_ms(10);
	}

	OCR1A = 0;
	OCR1B = 0;
}



