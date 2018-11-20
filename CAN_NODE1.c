/* 
        main program for Node 2
*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>         

#include "myDelay.h"
#include "lcdControl.h"
#include "myCANLib.h"
#define FREQ_CLKIO      16000000   // clock IO 16 MHz
#define PRESCALE      64
#define DC_PERIOD      25          // DC 25 msec

unsigned int ADdata; 
unsigned int cds=0;                //1:Day, 0:Night
unsigned int temp=0;
unsigned int canCase=0;
unsigned int adcchannel;
unsigned int channel;
unsigned int i;

struct MOb msg1={0x01, 0, EXT, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}; 
struct MOb msg2={0x01, 0, EXT, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}; 
char strBuff1[20]={0};
char strBuff2[20]={0};

void Buzzer(unsigned int speed);
void initAdc(void);
void initPort(void);
void BIOS();
void initBuzzer(void);

SIGNAL(ADC_vect){
   ADdata= ADC;  

   if(adcchannel==0){       //ADC0 : CDS
        adcchannel=1;
        ADMUX = 0x41;
        cds=ADdata;
    }else{                  //ADC1 : Temperature
        adcchannel=0;
        ADMUX = 0x40;
        temp = ADdata/20;
    }
    ADCSRA= 0xC8; 
}

int main(void){                                     //PE 1:alarm_led 2:cds_led, 3:buzzer
    int danger, danger_buffer=0, clear_state;
    temp=25;
    int temp_buffer=temp;

    BIOS();
    while(1){
        /*----------------------------------------------------*/    
        if(cds>=700) PORTE |= 0x04;        //PE2 is for front light  0000 0100         first area
        else if(cds<700) PORTE &= 0xfb;        
		/*----------------------------------------------------*/
    	can_rx(2,&msg1);                                                              //second area
          
		if(msg1.data[5]==2) danger=2;
		else if(msg1.data[5]==1) danger=1;
		/*----------------------------------------------------*/
		if(danger==2){                     //danger warning process.                  //third area
			Buzzer(5);
			sprintf(strBuff1,"WARNING!!!",danger-1);
			LCD_Write(0,0,strBuff1);
			PORTE &= 0xFD;
			us_delay(100);
			PORTE |= 0x02;	
		}else if(danger==1){
			sprintf(strBuff1,"          ",danger-1);
			LCD_Write(0,0,strBuff1);
		}
		/*----------------------------------------------------*/
		if(!(temp-temp_buffer>7 || temp-temp_buffer>7)) temp_buffer=(temp+temp_buffer)/2; //fourth area
		else temp_buffer = temp;
		msg2.data[3]=temp_buffer;
		sprintf(strBuff2,"temp:%d cds:%d",temp_buffer,cds);
		LCD_Write(0,1,strBuff2);
		can_tx(3,&msg2,0);                                                        
		}
}

void initAdc(void){
	ADMUX = 0x40;   
	DIDR0 = 0x07;   
	ADCSRA= 0xC8;   
}

void initPort(void){
    DDRC  = 0xff;
	PORTC = 0xff;
	DDRG  = 0xff;
	DDRF  = 0xfc;
	DDRE  = 0xff;
}

void BIOS(){        // setting register before program start
    initPort();
    LCD_init();    
    initAdc();
    initBuzzer();
    LCD_cmd(0x01);
   
    can_init(b500k);
    can_rx_set(2, 0x00, EXT, 8, 0x00000000, 0x00);    
   
    sei();
}

void initBuzzer(void){
   TCCR3A=   0xAB; 
   TCCR3B=   0x13;  
                   
   ICR3 = FREQ_CLKIO/2/PRESCALE/1000*DC_PERIOD;
}

void Buzzer(unsigned int speed){                //PE3 is the buzzer port

    OCR3A=(speed*FREQ_CLKIO/2/PRESCALE/1000*DC_PERIOD)/5;
    ms_delay(500);
    speed = 0 ;
    OCR3A=(speed*FREQ_CLKIO/2/PRESCALE/1000*DC_PERIOD)/5;
}