#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
//함수
void Uart_Init(void);
void Button_Init(void);
void Motor_Init(void);
void TimeCounter_Init();
void ADC_Init(void);
int Get_ADC(unsigned char ADC_num);//get ADC value
int Normal_AD(int AD,int AD_Max, int AD_min);//정규화
int GetWhiteNum(void);
void Decide_Flagmode(void);
void Count_Flag_1(void);
void Count_Flag_2(void);
void MODE0(void);
void MODE1(void);
void MODE_Fast(void);
void Stopmode(void);
void MODE_Uturn(void);
//변수
int Mode; //모드설정
int AD[8]; //AD값
int AD_min[8]; //최솟값
int AD_Max[8]={0,}; //최대값(초기화)
int Decidevalue_BW=0; //AD_BW를 결정하기위한 기준값(0초기화)
int AD_Weight[8]={-15,-10,-5,-1,1,5,10,15}; //가중치
int AD_Weight_E[8]={-15,-10,-5,0,0,5,10,15};
int F_Weight[8]={-8,-4,-2,-1,1,2,4,8};
int AD_BW[8];//검정 하양
int LED;
int Pre_AD_BW[8]={0,};//이전 BW값
int flag=0;//flag
int Flagmode=0;
int Time=0;
int s=0;
volatile int Stopmode_C=0;
volatile int S_time=0;
//최대최소 받기모드 설정
ISR(INT0_vect)//오른쪽 버튼(/왼쪽/모터드라이버, 128, 전원부 순으로 배치/오른쪽/)
{
	Mode=0;
}
//주행모드 설정
ISR(INT1_vect)//왼쪽 버튼
{
	Mode=1;
}
//실행
ISR(TIMER0_OVF_vect)//제어주기 약 10ms
{
	TCNT0 = 99;
	if(Mode==0)MODE0();//ADC최대 최솟값 결정+기준값 결정
	else if((Mode==1)&&(Stopmode_C==0)){
		Time++;
		MODE1();//정규화+주행
	}
	else if((Stopmode_C==1)&&(S_time<=300)){S_time++;}
	else{Stopmode_C=0;Mode=1;S_time=0;flag++;}
	
}
int main(void)
{
	//LED
	DDRA=0xff;
	PORTA=0xff;

	//UART+Button
	DDRD=0b00001000;
	
	//ADC
	DDRF=0x00;
	ADC_Init();
	//AD_min 초기화
	for(int i=0;i<8;i++){
		AD_min[i]=1023;
	}
	//button
	Button_Init();	
	//Motor
	DDRB =0xff;
	DDRE=0x0f;
	PORTE= 0x0A;//모터방향
	Motor_Init();
	//제어주기
	TimeCounter_Init();
	
	sei();
	while (1);

}
void Button_Init(void)
{
	EICRA=(1<<ISC01)|(1<<ISC11);
	EIMSK=(1<<INT1)|(1<<INT0);
}
void Motor_Init(void)
{
	TCCR1A = (1<<COM1A1)|(0<<COM1A0)|(1<<COM1B1)|(0<<COM1B0)|(1<<WGM11);//A,B Channel Clear(Compare Match) & Set(Overflow), 14Mode : Fast PWN
	TCCR1B = (1<<WGM13)|(1<<WGM12)|(0<<CS02)|(0<<CS01)|(1<<CS00);//14 Mode : Fast PWN, Prescalar 1
	ICR1 =799;//최대값
}
void TimeCounter_Init(void)
{
	TCCR0 = (0<<WGM01) | (0<<WGM00) | (0<<COM01) | (0<<COM00) | (1<<CS02) | (1<<CS01) | (1<<CS00);
	TIMSK = (1<<TOIE0);
	TCNT0 = 99;//99.75인데 소수점 버림
}
void ADC_Init(void)
{
	ADMUX =0b01000000;
	ADCSRA=0b10000111;
}
int Get_ADC(unsigned char ADC_num)
{
	ADMUX =(ADC_num|0b01000000);
	ADCSRA |=(1<<ADSC);
	while(!(ADCSRA&(1<<ADIF)));
	return ADC;
}
int Normal_AD(int AD,int AD_Max, int AD_min)
{
	double res;
	res=((double)(AD-AD_min)/(AD_Max-AD_min))*1023;
	res=(int)res;
	return res;
}
int GetWhiteNum(void)
{
	int n=0;
	for(int i=0;i<8;i++){
		if(AD_BW[i]==1)n++;
	}
	return n;
}
void Count_Flag_1(void)
{

	if(AD_BW[7]==0)s=1;
}
void Count_Flag_2(void)
{
	if(s==1&&AD_BW[7]==1){
		flag++;
		s=0;	
	}

}
void Decide_Flagmode(void)
{
	if(Time==8700)Flagmode=1;
}
void MODE0(void)
{
	//최대 최소 결정
	for(int i=0;i<8;i++){
		AD[i]=Get_ADC(i);
		if(AD_Max[i]<AD[i])AD_Max[i]=AD[i];
		if(AD_min[i]>AD[i])AD_min[i]=AD[i];
	}
	//기준값 결정
	for(int i=0;i<8;i++){
		Decidevalue_BW+=Normal_AD((double)(AD_Max[i]+AD_min[i])/2, AD_Max[i],AD_min[i]);
	}
	Decidevalue_BW=(int)Decidevalue_BW/8;
}
void MODE1(void)
{
	PORTE= 0x0A;//모터방향
	//ADC값 받기
	for(int i=0;i<8;i++){
		AD[i]=Get_ADC(i);
	}
	//정규화
	for(int i=0;i<8;i++){
		AD[i]=Normal_AD(AD[i],AD_Max[i],AD_min[i]);
	}
	//2진화, LED
	PORTA=0xff;
	LED=0xff;
	for(int i=0;i<8;i++){
		if(AD[i]<Decidevalue_BW-440)AD_BW[i]=0;//검정일땐 0
		else AD_BW[i]=1; //하양일땐 1
		LED = ~(~LED|(AD_BW[i]<<i));//LED
	}
	Decide_Flagmode();
	Count_Flag_1();
	Count_Flag_2();
	if(Flagmode==0)flag=0;
	if(flag==1)
	{
		MODE_Fast();
	}else if(flag==4||flag==11){
		Stopmode();
	}else if(flag==6){
		MODE_Uturn();
	}else{
		if(GetWhiteNum()<=2){
			PORTA=LED;
		}else{
			for(int i=0;i<8;i++){
				if(AD[i]<Decidevalue_BW-430)AD_BW[i]=1;//검정일땐 1 //430
				else AD_BW[i]=0; //하양일땐 0
				PORTA = ~(~PORTA|(AD_BW[i]<<i));//LED
			}
		}
		
		//-------------------------
		//모터
		int Weight=0;//가중치
		if(PORTA==0xff){ //1.모두 흰색이 들어올때 2. 모두 검정이 들어올 때
			for(int i=0;i<8;i++){
				Weight+=AD_Weight[i]*Pre_AD_BW[i];
			}
			if(Weight>0){
				PORTE=0b00001001;
				OCR1A=600;//반대방향
				OCR1B=600;
			}
			else if(Weight<0){
				PORTE=0b00000110;
				OCR1A=600;
				OCR1B=600;//
			}
		}else {
			for(int i=0;i<8;i++){
				Weight+=AD_Weight[i]*AD_BW[i];
			}
			OCR1A=582-Weight*7;
			OCR1B=582+Weight*7;
			for(int i=0;i<8;i++){
				Pre_AD_BW[i]=AD_BW[i];
			}
		}
	}

}
void MODE_Fast(void)
{
	for(int i=0;i<8;i++){
		
		if(AD[i]<Decidevalue_BW-430)AD_BW[i]=1;//검정일땐 1 //430
		else AD_BW[i]=0; //하양일땐 0
		PORTA = ~(~PORTA|(AD_BW[i]<<i));//LED
	}
	int Weight=0;
	for(int i=0;i<8;i++){
		Weight+=F_Weight[i]*AD_BW[i];
	}
	OCR1A=690-Weight*7;
	OCR1B=675+Weight*7;
}
void Stopmode(void)
{
	Stopmode_C=1;
	OCR1A=0;
	OCR1B=0;
}

void MODE_Uturn(void)
{
	PORTE=0b00000110;
	OCR1A=650;
	OCR1B=650;//
}