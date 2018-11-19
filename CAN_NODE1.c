#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>         // sprintf�??�언?�어 ?�음 

#include "myDelay.h"
#include "lcdControl.h"
#include "myCANLib.h"
#define FREQ_CLKIO      16000000   // clock IO 16 MHz
#define PRESCALE      64
#define DC_PERIOD      25         // DC ???? ??? 25 msec

unsigned int ADdata; 
unsigned int cds=0;                         //1:Day, 0:Night
unsigned int temp=0;
unsigned int canCase=0;
unsigned int adcchannel;
unsigned int channel;

unsigned int i;

struct MOb msg1={0x01, 0, EXT, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}; 
struct MOb msg2={0x01, 0, EXT, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}; 

char strBuff1[20]={0};
char strBuff2[20]={0};

//define LCD Port Pin ===========================================
#define LCD_RS_1  (PORTG |= 0x01)
#define LCD_RS_0  (PORTG &= 0xFE)

#define LCD_RW_1  (PORTG |= 0x02)
#define LCD_RW_0  (PORTG &= 0xFD)

#define EN_1       (PORTG |= 0x04)
#define EN_0      (PORTG &= 0xFB)
void initMotor(void)
{
   TCCR3A=   0xAB;   // COM3A[1:0]=10,  ??????? ?????
            //               TOP ?? 
            // WGM3[3:0] :     Phase correct PWM mode 
            //                 TOP???? ICR ????????? ????
   TCCR3B=   0x13;   // 64 ???? 
                
                
   ICR3 = FREQ_CLKIO/2/PRESCALE/1000*DC_PERIOD;   
               // ???(Top)3125, 40Hz(25msec) 
}
//=========================================================
// AD � ??
// PF0: ADC0 조이?�틱 y�?
// PF1: ADC1 조이?�틱 x�?
//=========================================================

void initAdc(void){
	ADMUX = 0x40;   // AVcc with external capacitor on AREF pin, ?�렬 그�?�?0100  0000
               // ADC0�??�용(channel 0�?  
	DDRF = 0xf0;   // PortF[3..0] ?�력?�로 ?�정, PortF[7..4] 출력?�로 ?�정 
	DIDR0 = 0x07;   // ?��????�력 불�? PortF[3..0]
	ADCSRA= 0xC8;   // ADC ?�에?�블, ADC � ???�작, ADC?�터?�트 ?�에?�블 
               // ADC ?�럭 ?�정; XTAL??/2(8MHz)   
}

SIGNAL(ADC_vect){
   ADdata= ADC;   // AD� ???�이?��? ADdata ??? ??
               //( Register???�는값을 ?�역� ??ADdata????�� ?�용?�기 ?�게 ?�용?�다. )
   if(adcchannel==0){
        adcchannel=1;
        ADMUX = 0x41;
        cds=ADdata;
    }else{
        adcchannel=0;
        ADMUX = 0x40;
        temp = ADdata/20;
    }
    ADCSRA= 0xC8;     // ADC ?�에?�블, ADC � ???�작, ADC?�터?�트 ?�에?�블 
               // ADC ?�럭 ?�정; XTAL??/2(8MHz)
}

void initPort(void){
   	DDRC  = 0xff;
	PORTC = 0xff;
	DDRG  = 0xff;
	DDRF  = 0xfc;
	DDRE  = 0xff;
}

void BIOS(){        // setting when program first load.
    initPort();      // ?�출???�트 초기??
    LCD_init();     // LCD 초기??
    initAdc();
    initMotor();
    LCD_cmd(0x01);
   
    sprintf(strBuff1, "Starting");
    LCD_Write(0, 0, strBuff1);
    ms_delay(1000);
    LCD_cmd(0x01);

    can_init(b500k);       // 초기??
    can_rx_set(2, 0x00, EXT, 8, 0x00000000, 0x00);    // CAN ?�신�?초기??
    //        obj  id   ide  dlc  idmask   rtrldemask   

    sei();         // INT ?�에?�블
}
void Buzzer(unsigned int speed, unsigned int dir)
{
   unsigned int level=5;

   PORTE&=0xFE;

   // DC???? ??? ???? ????
   if(dir==0)      // ?��? ???? ???
   {
      // speed?? ???? ??? ????
      OCR3A=(speed*FREQ_CLKIO/2/PRESCALE/1000*DC_PERIOD)/level;
      PORTE|=0x01;   
   }
}
int main(void)
{
    int danger;
	int danger_buffer=0;
	int clear_state;
    temp=25;
    int temp_buffer=temp;
	PORTE = 0xff;
    BIOS(); //initial setting
	//PORTE=0x00;
    while(1){
        /*--------------------------------------------------*/
        if(cds>=700) PORTE |= 0x04;       //PE2 is for front light  0000 0100 
        else if(cds<700) PORTE &= 0xfb;        
		/*----------------------------------------------------*/
    	can_rx(2,&msg1);
          
		if(msg1.data[5]==2) danger=2;
		else if(msg1.data[5]==1) danger=1;
		
		if(danger==2){
			Buzzer(500,0);
			sprintf(strBuff1,"WARNING!!!",danger-1);
			LCD_Write(0,0,strBuff1);
			PORTE &= 0xFD;
			us_delay(100);
			PORTE |= 0x02;	
		}else if(danger==1){
			sprintf(strBuff1,"          ",danger-1);
			LCD_Write(0,0,strBuff1);
		}
		 
		if(!(temp-temp_buffer>7 || temp-temp_buffer>7)) temp_buffer=(temp+temp_buffer)/2;
		else temp_buffer = temp;
		msg2.data[3]=temp_buffer;
		sprintf(strBuff2,"temp:%d cds:%d",temp_buffer,cds);
		LCD_Write(0,1,strBuff2);
		can_tx(3,&msg2,0);
		}
}
