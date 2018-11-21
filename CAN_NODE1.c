/* 
        main program for Node 2
*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>         

#define FREQ_CLKIO      16000000   // clock IO 16 MHz
#define PRESCALE      64
#define DC_PERIOD      25          // DC 25 msec

#define LCD_RS_1  (PORTG |= 0x01)
#define LCD_RS_0  (PORTG &= 0xFE)

#define LCD_RW_1  (PORTG |= 0x02)
#define LCD_RW_0  (PORTG &= 0xFD)

#define EN_1  	  (PORTG |= 0x04)
#define EN_0      (PORTG &= 0xFB)

#define STD		0x00
#define EXT		0x01

#define _SEND_FAIL	0
#define _SEND_OK	1

#define _RECE_FAIL	0
#define _RECE_OK	1

// CAN ???????(baud rate)===============================
#define b1M		1
#define b500k 	2
#define b250k	3
#define b200k	4
#define	b125k	5
#define b100k	6

unsigned int ADdata; 
unsigned int cds=0;                //1:Day, 0:Night
unsigned int temp=0;
unsigned int canCase=0;
unsigned int adcchannel;
unsigned int channel;
unsigned int i;

struct MOb
{
	unsigned long id;
	unsigned char rtr;
	unsigned char ide;
	unsigned char dlc;
	unsigned char data[8];
};

struct MOb msg1={0x01, 0, EXT, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}; 
struct MOb msg2={0x01, 0, EXT, 8, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}; 
char strBuff1[20]={0};
char strBuff2[20]={0};

//MOb ????u ????  ======================================

void can_init_8Mhz(char baudRate);
void can_init(char baudRate);
char can_tx(unsigned char obj, struct MOb *msg, char rtr);	// CAN transmission
char can_rx(unsigned char obj, struct MOb *msg);		
void can_rx_set(char obj, unsigned long id, char ide, unsigned char dlc, 
				unsigned long idmask, unsigned char rtrIdemask);
void can_int_rx_set(char obj, unsigned long id, char ide, unsigned char dlc, 
					unsigned long idmask, unsigned char rtrIdemask);

void delay(unsigned int k);
void us_delay(unsigned int us_time);
void ms_delay(unsigned int ms_time);
void E_Pulse(void);
void LCD_init(void);
void LCD_cmd(unsigned char cmd);
void Write_Char(unsigned char buf);
void LCD_Disp(char x,char y);
void LCD_Write(char x, char y,char *str);
void LCD_Write_char(char x, char y, unsigned char ch);

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
    int danger=1;
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
			sprintf(strBuff1,"WARNING!!!");
			LCD_Write(0,0,strBuff1);
			PORTE &= 0xFD;
			us_delay(100);
			PORTE |= 0x02;	
		}else if(danger==1){
			sprintf(strBuff1,"          ");
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
void E_Pulse(void)
{
	EN_1;

	us_delay(100);

	EN_0;
}

void LCD_init(void)
{
	ms_delay(40);

	PORTC = 0x38;	// Function Set
	E_Pulse();
    us_delay(40);

	PORTC = 0x0c; // DisPlay ON/OFF Control
	us_delay(40);
	E_Pulse();
	
	PORTC = 0x01; // Display Clear
	ms_delay(2);
	E_Pulse();

	PORTC = 0x06; // Entry Mode Set
	E_Pulse();
}

void LCD_cmd(unsigned char cmd)
{
	LCD_RS_0;
	LCD_RW_0;
	PORTC=cmd;
	E_Pulse();
}	

void Write_Char(unsigned char buf)
{
	LCD_RS_1;
	LCD_RW_0;
	PORTC=buf;
	E_Pulse();
}	

void LCD_Disp(char x,char y)
{
	LCD_RS_0;
	LCD_RW_0;

	if(y==0) PORTC = x + 0x80;
	else if(y==1) PORTC = x + 0xc0;
	E_Pulse();
}
 
void LCD_Write(char x, char y,char *str)
{
	LCD_Disp(x,y);
	while(*str)
	Write_Char(*str++);
}

void LCD_Write_char(char x, char y, unsigned char ch)
{
	LCD_Disp(x,y);
	Write_Char(ch);
}
void delay(unsigned int k)
{
    unsigned int i;

	for(i=0;i<k;i++); 
}

void us_delay(unsigned int us_time)
{
	unsigned int i;

	for(i=0; i<us_time; i++) // 4 cycle +
	{
		asm("PUSH R0"); 	// 2 cycle +
		asm("POP R0"); 		// 2 cycle +
		asm("PUSH R0"); 	// 2 cycle +
		asm("POP R0"); 		// 2 cycle + =12 cycle for 11.0592MHZ
		asm("PUSH R0"); 	// 2 cycle +
		asm("POP R0"); 		// 2 cycle = 16 cycle = 1us for 16MHz
	}
}

void ms_delay(unsigned int ms_time)
{
    unsigned int i;
    
    for(i=0; i<ms_time;i++)
        us_delay(1000);
}
void can_init (char baudRate)	// CAN???? 
{
	unsigned char i, j;
	
	CANGCON |= (1<<SWRES);	// CAN ????? ????
							// CAN General Control Register
  							
	//??????? ???? ==============================================
	switch(baudRate){
		case b1M:
			CANBT1= 0x00;
			CANBT2= 0x0c;
			CANBT3= 0x37;
			break;
		case b500k:
			CANBT1= 0x02;
			CANBT2= 0x0c;
			CANBT3= 0x37;			
			break;
		case b250k:
			CANBT1= 0x06;	// CAN??????? ???? 
			CANBT2= 0x0c;	// bit timing: datasheet 264 (check table)
			CANBT3= 0x37;	// 250kbps, 16 MHz CPU Clock(0.250usec)
			break;
		case b200k:
			CANBT1= 0x08;
			CANBT2= 0x0c;
			CANBT3= 0x37;
			break;
		case b125k:
			CANBT1= 0x0E;
			CANBT2= 0x0c;
			CANBT3= 0x37;
			break;

		case b100k:
			CANBT1= 0x12;
			CANBT2= 0x0c;
			CANBT3= 0x37;													
			break;
	}
	
	for(i=0; i<15; i++)		// Reset all MObs
	{
		CANPAGE = (i<<4);	// MOBNB3~0
							// MOb Number Select(0~14)
		CANCDMOB = 0;		// ALL Disable MOb
		CANSTMOB = 0;		// Clear status
		CANIDT1 = 0;		// Clear ID
		CANIDT2 = 0;		// Clear ID
		CANIDT3 = 0;		// Clear ID
		CANIDT4 = 0;		// Clear ID
		CANIDM1 = 0;		// Clear mask
		CANIDM2 = 0;		// Clear mask
		CANIDM3 = 0;		// Clear mask
		CANIDM4 = 0;		// Clear mask

		for(j=0; j<8; j++)
			CANMSG = 0;		// CAN Data Message Register
							// Clear data
	}
							
							// Clear CAN interrupt registers
	CANGIE = 0;				// CAN General Interrupt Enable Register 
							// None Interrupts
	CANIE1 = 0;				// CAN Enable INT MOb Registers 1
							// None Interrupts on MObs
	CANIE2 = 0;				// CAN Enable INT MOb Registers 2
							// None Interrupts on MObs
	CANSIT1 = 0;			// CAN Status INT MOb Registers 1
							// None Interrupts on MObs
	CANSIT2 = 0;			// CAN Status INT MOb Registers 2
							// None Interrupts on MObs

	//CANGCON = (1<<TTC );	// TTC mode *******************************************
	
	CANGCON |= (1<<ENASTB);	// CAN General Control Register 
							// Enable Mode (11 Recessive Bits has Been read)
							// Start CAN interface

	while (!(CANGSTA & (1<<ENFG))); // CAN General Status Register (Enable Flag)
									// Wait until module ready
}



//***************************************************************
// CAN ???? 	
// 1. CAN ????? ???? 
// 2. ??????? ???? 
// 3. MOb ???/???? ???? 
// 4. MOb IDT, Mask ???? 
// 5. CAN General ?????? ?????(CAN ?????? ??????? ????) 
// 6. CAN MOb ?????? ?????(CAN MOb ?????? ??????? ????)
// 7. CAN ????? ?ワ???? ??? ???? 
// 8. CAN ????? ???? ??? ?? ???? ???  									
//***************************************************************
void can_init_8Mhz(char baudRate)	// CAN???? 
{
	unsigned char i, j;
	
	CANGCON |= (1<<SWRES);	// CAN ????? ????
							// CAN General Control Register
  							
	//??????? ???? ==============================================
	switch(baudRate){
		case b1M:
			CANBT1= 0x00;
			CANBT2= 0x04;
			CANBT3= 0x13;
			break;
		case b500k:
			CANBT1= 0x00;
			CANBT2= 0x0c;
			CANBT3= 0x37;			
			break;
		case b250k:
			CANBT1= 0x02;	// CAN baud rate set
			CANBT2= 0x0c;	// bit timing: datasheet 264 (check table)
			CANBT3= 0x37;	// 250kbps 8 MHz CPU Clock(0.250usec)
			break;
		case b200k:
			CANBT1= 0x02;
			CANBT2= 0x0e;
			CANBT3= 0x4b;
			break;
		case b125k:
			CANBT1= 0x06;
			CANBT2= 0x0c;
			CANBT3= 0x37;
			break;
		case b100k:
			CANBT1= 0x08;
			CANBT2= 0x0c;
			CANBT3= 0x37;													
			break;
	}
	
	for(i=0; i<15; i++)		// Reset all MObs
	{
		CANPAGE = (i<<4);	// MOBNB3~0
							// MOb Number Select(0~14)
		CANCDMOB = 0;		// ALL Disable MOb
		CANSTMOB = 0;		// Clear status
		CANIDT1 = 0;		// Clear ID
		CANIDT2 = 0;		// Clear ID
		CANIDT3 = 0;		// Clear ID
		CANIDT4 = 0;		// Clear ID
		CANIDM1 = 0;		// Clear mask
		CANIDM2 = 0;		// Clear mask
		CANIDM3 = 0;		// Clear mask
		CANIDM4 = 0;		// Clear mask

		for(j=0; j<8; j++)
			CANMSG = 0;		// CAN Data Message Register
							// Clear data
	}
							
							// Clear CAN interrupt registers
	CANGIE = 0;				// CAN General Interrupt Enable Register 
							// None Interrupts
	CANIE1 = 0;				// CAN Enable INT MOb Registers 1
							// None Interrupts on MObs
	CANIE2 = 0;				// CAN Enable INT MOb Registers 2
							// None Interrupts on MObs
	CANSIT1 = 0;			// CAN Status INT MOb Registers 1
							// None Interrupts on MObs
	CANSIT2 = 0;			// CAN Status INT MOb Registers 2
							// None Interrupts on MObs

	CANGCON = (1<<TTC );	// TTC mode *******************************************
	
	CANGCON |= (1<<ENASTB);	// CAN General Control Register 
							// Enable Mode (11 Recessive Bits has Been read)
							// Start CAN interface

	while (!(CANGSTA & (1<<ENFG))); // CAN General Status Register (Enable Flag)
									// Wait until module ready
}


//***************************************************************
// CAN ????? ???? (???? ???)	
// o?? ???? 
// 1. MOb ???? 
// 2. MOb ???? ?????
// 3. CANCDMOB ????? (??? ????, ??? ????, IDE, DLC)
// 4. ID ???? 
// 5. IDE ???? 
// 6. DLC ???? 
// 7. RTR ???? 
// 8. ????? ?????? ???? 
// 9. ????? ??? 	
// ???? 
//		obj; MOb ??? 
//		msg; ????? ????u 
//		rtr; RTR r????(0; ?????? ??????, 1; ????? ??????)	
//***************************************************************
char can_tx (unsigned char obj, struct MOb *msg, char rtr)	// CAN transmission
{
	//usart1_transmit_string("\rCAn loop in\n");

	char send_result = _SEND_FAIL;
	unsigned char i;	
	unsigned long can_id= msg->id;
	
								// Enable MOb1, auto increment index, start with index = 0
	CANPAGE = (obj<<4);			// CAN Page MOb Register
								// MOb Number Select

	//usart1_transmit_string("\rPAGE Clear\n");

	CANSTMOB = 0x00;
	CANCDMOB = 0x00;
	
	//usart1_transmit_string("\rMOb Clear\n");

	if(msg->ide== 0x00)	// standard
	{
		CANIDT1= (unsigned char)(can_id>>3);
		CANIDT2= (unsigned char)(can_id<<5);

		CANCDMOB &= ~0x10;		// Set IDE bit 2.0A 11bits
		//usart1_transmit_string("\rstandard\n");
	}
	else	// extended
	{
		CANIDT1= (unsigned char)(can_id>>21);
		CANIDT2= (unsigned char)(can_id>>13);
		CANIDT3= (unsigned char)(can_id>>5);
		CANIDT4= (unsigned char)(can_id<<3);

		CANCDMOB |= 0x10;		// Set IDE bit 2.0B 29bits
	//	usart1_transmit_string("\rExtended\n");
	}

	CANCDMOB |= (msg->dlc<<DLC0);	// set data length

	//usart1_transmit_string("\rDLC Clear\n");	

	CANIDT4 |= (rtr & 0x04);     // RTRTAG ????;

	CANIDT4 &= ~0x02;		   // RB1TAG=0;
	CANIDT4 &= ~0x01;		   // RB0TAG=1;

	//usart1_transmit_string("\rRTR Clear\n");	

	//put data in mailbox
	for(i=0; i<msg->dlc; i++)
		CANMSG = msg->data[i];	// full message 

	//usart1_transmit_string("\rMSG Clear\n");	

	//enable transmission		
	CANCDMOB |= (1<<CONMOB0);

	//usart1_transmit_string("\renable transmissionr\n");	

	while (!(CANSTMOB & (1<<TXOK)));	// check tx ok

	//usart1_transmit_string("\rResult 1\n");	

	send_result= _SEND_OK;

	//usart1_transmit_string("\rResult 2\n");		

	// monitoring with serial com
	//usart1_transmit_string("\rTXOK\n");

	//reset flag
	CANSTMOB &= ~(1<<TXOK);

	return(send_result);
}


//***************************************************************
// CAN ????? ???? (???? ???)
// 1. ???? MOb ???? 
// 2. ???? ??? ??? 
// 3. ??? ??? ??? ???? ??? 
// 4. ID ???? o?? 
// 5. IDE ???? o?? 
// 6. DLC ???? o?? 
// 7. Data ???? o?? 
//***************************************************************
char can_rx(unsigned char obj, struct MOb *msg)		
{
	char rece_result = _RECE_FAIL;
	unsigned char i;	
	unsigned long can_id= 0;
	
	CANPAGE = (obj<<4);				// ???? MOb ???? 

	//usart1_transmit_string("\rRX MOb #");
	//usart1_transmit(obj+0x30);
	//usart1_transmit_string("\r\n");

	// ???? ???? ?????? ????? 
	while(!(CANSTMOB&(1<<RXOK)));
	// ?????? ????? 
	// CANIDT, CANCDMOB, CANMSG?? ???? ??????? ????? 
	// get CANIDT and CANCDMOB and CANMSg
	//usart1_transmit_string("\rRXOK\n");
	rece_result = _RECE_OK;

	// ??? ??? ??? ????? ????? ID, IDE ???? 
	if((CANCDMOB & 0x10) == 0x00){			// IDE standard ?
		msg->ide= STD;
		can_id  = ((unsigned long)CANIDT1)<<8;
		can_id |= ((unsigned long)CANIDT2);
		can_id>>=5;
		//usart1_transmit_string("\rRx Standard\n");
	}
	else{
		msg->ide= EXT;
		can_id  = ((unsigned long)CANIDT1)<<24;
		can_id |= ((unsigned long)CANIDT2)<<16;
		can_id |= ((unsigned long)CANIDT3)<<8;
		can_id |= ((unsigned long)CANIDT4);
		can_id>>=3;
		//usart1_transmit_string("\rRx Extended\n");
	}
	msg->id= can_id;			// ????u ?????? CANID ???? 

	msg->rtr= CANIDT4 & 0x04;   

	msg->dlc= CANCDMOB & 0x0f;	// ???? ????? ???? ????u ?????? ???? 

	// get data
	for(i=0; i<(CANCDMOB&0xf); i++){
		msg->data[i] = CANMSG;	// ????? ?????? ?鵄?? ???? 
	}

	// rx init 
	CANSTMOB = 0x00;			// ???? ??? ? 

	// enable reception mode and ide set
	CANCDMOB |= (1<<CONMOB1); 	// ???? IDE ??????? ???? ??? ????

	// reset flag
	CANSTMOB &= ~(1<<RXOK);		// ?????? 

	return(rece_result);
}

//****************************************************************/
// CAN ???? MOb ???? 
// ????:
//		obj; MOb ??? 
//		id; CAN ID
//		ide; ????? ????(0:2.0A, 1:2.0B)
//		dlc; ????? ?????? ???? (??? 8bytes)
//		idmask; CAN ID ???? ????? 
//		rtrIdemask; RTR ????? IDE ????? 			
//****************************************************************/
void can_rx_set(char obj, unsigned long id, char ide, unsigned char dlc, 
				unsigned long idmask, unsigned char rtrIdemask)
{
	CANPAGE = obj<<4;		// set MOb number

	CANSTMOB = 0x00;		// clear status

	if(ide== STD)			// standard
	{
		CANIDT1= (unsigned char)(id>>3);
		CANIDT2= (unsigned char)(id<<5);

		CANIDM1= (unsigned char)(idmask>>3);
		CANIDM2= (unsigned char)(idmask<<5);
		CANIDM4=0;

		CANCDMOB &= ~0x10;	// clear IDE =0, standard 11 bits

		//usart1_transmit_string("\rRx Standard Set\n");
	}
	else					// extended
	{
		CANIDT1= (unsigned char)(id>>21);
		CANIDT2= (unsigned char)(id>>13);
		CANIDT3= (unsigned char)(id>>5);
		CANIDT4= (unsigned char)(id<<3);

		CANIDM1= (unsigned char)(idmask>>21);
		CANIDM2= (unsigned char)(idmask>>13);
		CANIDM3= (unsigned char)(idmask>>5);
		CANIDM4= (unsigned char)(idmask<<3);

		CANCDMOB |= 0x10;	// set IDE =1, extended 29 bits

		//usart1_transmit_string("\rRx Extended Set\n");
	}
	CANCDMOB |= (dlc & 0x0f);		// set data length

	CANIDM4 |= (unsigned char)(rtrIdemask & 0x07);
//	CANIDM4 &= ~0x04;		// RTRMSK= 1/0 enable comparison (Data receive)
//	CANIDM4 &= ~0x02;		// recommended
//	CANIDM4 &= ~0x01;		// IDEMSK= 1/0 enable comparison (IDE receive)

	CANCDMOB |= 0x80;		// receive enable 
}

//****************************************************************/
// CAN Interrupt ???? MOb ???? 
// ????:
//		obj; MOb ??? 
//		id; CAN ID
//		ide; ????? ????(0:2.0A, 1:2.0B)
//		dlc; ????? ?????? ???? (??? 8bytes)
//		idmask; CAN ID ???? ????? 
//		rtrIdemask; RTR ????? IDE ????? 			
// ????:
//  1. MOb ????  
//  2. ID ???? 
//  3. ???? ???? 
//  4. ????? ???? 
//  5. DLC ???? 
//  6. ?????? ?ワ???? (interrupt)
//  7. ???? ??? ???? 
//****************************************************************/
void can_int_rx_set(char obj, unsigned long id, char rplvIde, 
					unsigned char dlc, unsigned long idmask, 
					unsigned char rtrIdemask)
{
	CANPAGE = obj<<4;		// set MOb number

	CANSTMOB = 0x00;		// clear status

	if(rplvIde & 0x02)	
		CANCDMOB |= 0x20;			// RPLV set, ??? ???? ??? ???? 
	else
		CANCDMOB &= ~0x20;			// RPLV clear

	if(( rplvIde & 0x01) == STD)			// standard
	{
		CANIDT1= (unsigned char)(id>>3);
		CANIDT2= (unsigned char)(id<<5);

		CANIDM1= (unsigned char)(idmask>>3);
		CANIDM2= (unsigned char)(idmask<<5);
		CANIDM4=0;

		CANCDMOB &= ~0x10;	// clear IDE =0, standard 11 bits

		//usart1_transmit_string("\rRx Standard Set\n");
	}
	else					// extended
	{
		CANIDT1= (unsigned char)(id>>21);
		CANIDT2= (unsigned char)(id>>13);
		CANIDT3= (unsigned char)(id>>5);
		CANIDT4= (unsigned char)(id<<3);

		CANIDM1= (unsigned char)(idmask>>21);
		CANIDM2= (unsigned char)(idmask>>13);
		CANIDM3= (unsigned char)(idmask>>5);
		CANIDM4= (unsigned char)(idmask<<3);

		CANCDMOB |= 0x10;	// set IDE =1, extended 29 bits

		//usart1_transmit_string("\rRx Extended Set\n");
	}
	CANCDMOB |= (dlc & 0x0f);		// set data length

	CANIDM4 |= (unsigned char)(rtrIdemask & 0x07);
//	CANIDM4 &= ~0x04;		// RTRMSK= 1/0 enable comparison (Data receive)
//	CANIDM4 &= ~0x02;		// recommended
//	CANIDM4 &= ~0x01;		// IDEMSK= 1/0 enable comparison (IDE receive)


//  ?????? ?ワ????(?????? ????)
	CANGIE |= 0xa0; 		// Enable all interrupt and Enable Rx interrupt

	if(obj<8) 
		CANIE2 = (1<<obj);		// ??? MOb?? ???????? ?ワ???? ??? 
	else        
		CANIE1 = (1<<(obj-8));	// 

	CANCDMOB |= 0x80;			// ???? ?ワ???? 
	sei();
}
