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

//LCD Functions =================================================
void E_Pulse(void)
{
   EN_1;

   us_delay(100);

   EN_0;
}

void LCD_init(void)
{
   ms_delay(40);

   PORTC = 0x38;   // Function Set
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

//Delay =========================================================
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
      asm("PUSH R0");    // 2 cycle +
      asm("POP R0");       // 2 cycle +
      asm("PUSH R0");    // 2 cycle +
      asm("POP R0");       // 2 cycle + =12 cycle for 11.0592MHZ
      asm("PUSH R0");    // 2 cycle +
      asm("POP R0");       // 2 cycle = 16 cycle = 1us for 16MHz
   }
}

void ms_delay(unsigned int ms_time)
{
    unsigned int i;
    
    for(i=0; i<ms_time;i++)
        us_delay(1000);
}

////////myCANLib.c/////////////

//***************************************************************
// CAN 초기??   
// 1. CAN ?�어�?리셋 
// 2. 보레?�트 ?�정 
// 3. MOb 모드/?�태 초기??
// 4. MOb IDT, Mask 초기??
// 5. CAN General ?�터?�트 ?�리??CAN ?�터?�트 ?�용?��? ?�음) 
// 6. CAN MOb ?�터?�트 ?�리??CAN MOb ?�터?�트 ?�용?��? ?�음)
// 7. CAN ?�어�??�에?�블 모드 ?�정 
// 8. CAN ?�어�??�작 ?�인 ??초기???�료                             
//***************************************************************
void can_init (char baudRate)   // CAN초기??
{
   unsigned char i, j;
   
   CANGCON |= (1<<SWRES);   // CAN ?�어�?리셋
                     // CAN General Control Register
                       
   //보레?�트 ?�정 ==============================================
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
         CANBT1= 0x06;   // CAN보레?�트 ?�정 
         CANBT2= 0x0c;   // bit timing: datasheet 264 (check table)
         CANBT3= 0x37;   // 250kbps, 16 MHz CPU Clock(0.250usec)
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
   
   for(i=0; i<15; i++)      // Reset all MObs
   {
      CANPAGE = (i<<4);   // MOBNB3~0
                     // MOb Number Select(0~14)
      CANCDMOB = 0;      // ALL Disable MOb
      CANSTMOB = 0;      // Clear status
      CANIDT1 = 0;      // Clear ID
      CANIDT2 = 0;      // Clear ID
      CANIDT3 = 0;      // Clear ID
      CANIDT4 = 0;      // Clear ID
      CANIDM1 = 0;      // Clear mask
      CANIDM2 = 0;      // Clear mask
      CANIDM3 = 0;      // Clear mask
      CANIDM4 = 0;      // Clear mask

      for(j=0; j<8; j++)
         CANMSG = 0;      // CAN Data Message Register
                     // Clear data
   }
                     
                     // Clear CAN interrupt registers
   CANGIE = 0;            // CAN General Interrupt Enable Register 
                     // None Interrupts
   CANIE1 = 0;            // CAN Enable INT MOb Registers 1
                     // None Interrupts on MObs
   CANIE2 = 0;            // CAN Enable INT MOb Registers 2
                     // None Interrupts on MObs
   CANSIT1 = 0;         // CAN Status INT MOb Registers 1
                     // None Interrupts on MObs
   CANSIT2 = 0;         // CAN Status INT MOb Registers 2
                     // None Interrupts on MObs

   //CANGCON = (1<<TTC );   // TTC mode *******************************************
   
   CANGCON |= (1<<ENASTB);   // CAN General Control Register 
                     // Enable Mode (11 Recessive Bits has Been read)
                     // Start CAN interface

   while (!(CANGSTA & (1<<ENFG))); // CAN General Status Register (Enable Flag)
                           // Wait until module ready
}



//***************************************************************
// CAN 초기??   
// 1. CAN ?�어�?리셋 
// 2. 보레?�트 ?�정 
// 3. MOb 모드/?�태 초기??
// 4. MOb IDT, Mask 초기??
// 5. CAN General ?�터?�트 ?�리??CAN ?�터?�트 ?�용?��? ?�음) 
// 6. CAN MOb ?�터?�트 ?�리??CAN MOb ?�터?�트 ?�용?��? ?�음)
// 7. CAN ?�어�??�에?�블 모드 ?�정 
// 8. CAN ?�어�??�작 ?�인 ??초기???�료                             
//***************************************************************
void can_init_8Mhz(char baudRate)   // CAN초기??
{
   unsigned char i, j;
   
   CANGCON |= (1<<SWRES);   // CAN ?�어�?리셋
                     // CAN General Control Register
                       
   //보레?�트 ?�정 ==============================================
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
         CANBT1= 0x02;   // CAN baud rate set
         CANBT2= 0x0c;   // bit timing: datasheet 264 (check table)
         CANBT3= 0x37;   // 250kbps 8 MHz CPU Clock(0.250usec)
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
   
   for(i=0; i<15; i++)      // Reset all MObs
   {
      CANPAGE = (i<<4);   // MOBNB3~0
                     // MOb Number Select(0~14)
      CANCDMOB = 0;      // ALL Disable MOb
      CANSTMOB = 0;      // Clear status
      CANIDT1 = 0;      // Clear ID
      CANIDT2 = 0;      // Clear ID
      CANIDT3 = 0;      // Clear ID
      CANIDT4 = 0;      // Clear ID
      CANIDM1 = 0;      // Clear mask
      CANIDM2 = 0;      // Clear mask
      CANIDM3 = 0;      // Clear mask
      CANIDM4 = 0;      // Clear mask

      for(j=0; j<8; j++)
         CANMSG = 0;      // CAN Data Message Register
                     // Clear data
   }
                     
                     // Clear CAN interrupt registers
   CANGIE = 0;            // CAN General Interrupt Enable Register 
                     // None Interrupts
   CANIE1 = 0;            // CAN Enable INT MOb Registers 1
                     // None Interrupts on MObs
   CANIE2 = 0;            // CAN Enable INT MOb Registers 2
                     // None Interrupts on MObs
   CANSIT1 = 0;         // CAN Status INT MOb Registers 1
                     // None Interrupts on MObs
   CANSIT2 = 0;         // CAN Status INT MOb Registers 2
                     // None Interrupts on MObs

   CANGCON = (1<<TTC );   // TTC mode *******************************************
   
   CANGCON |= (1<<ENASTB);   // CAN General Control Register 
                     // Enable Mode (11 Recessive Bits has Been read)
                     // Start CAN interface

   while (!(CANGSTA & (1<<ENFG))); // CAN General Status Register (Enable Flag)
                           // Wait until module ready
}


//***************************************************************
// CAN 메시�  ?�송 (?�링 방식)   
// 처리 ?�서 
// 1. MOb ?�택 
// 2. MOb ?�태 ?�리??
// 3. CANCDMOB ?�리??(모드 ?�정, ?�동 ?�답, IDE, DLC)
// 4. ID ?�정 
// 5. IDE ?�정 
// 6. DLC ?�정 
// 7. RTR ?�정 
// 8. 메시�  ?�이??? ??
// 9. 메시�  ?�신    
// � ??
//      obj; MOb 번호 
//      msg; 메시�  구조�?
//      rtr; RTR r결정(0; ?�이???�레?? 1; 리모???�레??   
//***************************************************************
char can_tx (unsigned char obj, struct MOb *msg, char rtr)   // CAN transmission
{
   //usart1_transmit_string("\rCAn loop in\n");

   char send_result = _SEND_FAIL;
   unsigned char i;   
   unsigned long can_id= msg->id;
   
                        // Enable MOb1, auto increment index, start with index = 0
   CANPAGE = (obj<<4);         // CAN Page MOb Register
                        // MOb Number Select

   //usart1_transmit_string("\rPAGE Clear\n");

   CANSTMOB = 0x00;
   CANCDMOB = 0x00;
   
   //usart1_transmit_string("\rMOb Clear\n");

   if(msg->ide== 0x00)   // standard
   {
      CANIDT1= (unsigned char)(can_id>>3);
      CANIDT2= (unsigned char)(can_id<<5);

      CANCDMOB &= ~0x10;      // Set IDE bit 2.0A 11bits
      //usart1_transmit_string("\rstandard\n");
   }
   else   // extended
   {
      CANIDT1= (unsigned char)(can_id>>21);
      CANIDT2= (unsigned char)(can_id>>13);
      CANIDT3= (unsigned char)(can_id>>5);
      CANIDT4= (unsigned char)(can_id<<3);

      CANCDMOB |= 0x10;      // Set IDE bit 2.0B 29bits
   //   usart1_transmit_string("\rExtended\n");
   }

   CANCDMOB |= (msg->dlc<<DLC0);   // set data length

   //usart1_transmit_string("\rDLC Clear\n");   

   CANIDT4 |= (rtr & 0x04);     // RTRTAG ?�정;

   CANIDT4 &= ~0x02;         // RB1TAG=0;
   CANIDT4 &= ~0x01;         // RB0TAG=1;

   //usart1_transmit_string("\rRTR Clear\n");   

   //put data in mailbox
   for(i=0; i<msg->dlc; i++)
      CANMSG = msg->data[i];   // full message 

   //usart1_transmit_string("\rMSG Clear\n");   

   //enable transmission      
   CANCDMOB |= (1<<CONMOB0);

   //usart1_transmit_string("\renable transmissionr\n");   

   while (!(CANSTMOB & (1<<TXOK)));   // check tx ok

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
// CAN 메시�  ?�신 (?�링 방식)
// 1. ?�신 MOb ?�택 
// 2. ?�신 ?�료 ?�인 
// 3. ?��? ?��? ?�장 버전 ?�인 
// 4. ID ?�신 처리 
// 5. IDE ?�신 처리 
// 6. DLC ?�신 처리 
// 7. Data ?�신 처리 
//***************************************************************
char can_rx(unsigned char obj, struct MOb *msg)      
{
   char rece_result = _RECE_FAIL;
   unsigned char i;   
   unsigned long can_id= 0;
   
   CANPAGE = (obj<<4);            // ?�신 MOb ?�택 

   //usart1_transmit_string("\rRX MOb #");
   //usart1_transmit(obj+0x30);
   //usart1_transmit_string("\r\n");

   // ?�신 ?�료???�까�  ? 기함 
   while(!(CANSTMOB&(1<<RXOK)));
   // ?�신???�료?�면 
   // CANIDT, CANCDMOB, CANMSG???�신 메시� �  ? ?�됨 
   // get CANIDT and CANCDMOB and CANMSg
   //usart1_transmit_string("\rRXOK\n");
   rece_result = _RECE_OK;

   // ?��? ?��? ?�장 ?�맷??맞추??ID, IDE 결정 
   if((CANCDMOB & 0x10) == 0x00){         // IDE standard ?
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
   msg->id= can_id;         // 구조�?� ?�로 CANID ? ??

   msg->rtr= CANIDT4 & 0x04;   

   msg->dlc= CANCDMOB & 0x0f;   // ?�신 메시�  길이 구조�?� ?�에 ? ??

   // get data
   for(i=0; i<(CANCDMOB&0xf); i++){
      msg->data[i] = CANMSG;   // 메시�  ?�이??배열??? ??
   }

   // rx init 
   CANSTMOB = 0x00;         // ?�태 초기 ??

   // enable reception mode and ide set
   CANCDMOB |= (1<<CONMOB1);    // ?�신 IDE ?��??�고 ?�신 모드 ?�정

   // reset flag
   CANSTMOB &= ~(1<<RXOK);      // ?�신? �?

   return(rece_result);
}

//****************************************************************/
// CAN ?�신 MOb ?�정 
// � ??
//      obj; MOb 번호 
//      id; CAN ID
//      ide; 메시�  ?�맷(0:2.0A, 1:2.0B)
//      dlc; 메시�  ?�이??길이 (최�? 8bytes)
//      idmask; CAN ID ?�신 마스??
//      rtrIdemask; RTR 비트?  IDE 마스??         
//****************************************************************/
void can_rx_set(char obj, unsigned long id, char ide, unsigned char dlc, 
            unsigned long idmask, unsigned char rtrIdemask)
{
   CANPAGE = obj<<4;      // set MOb number

   CANSTMOB = 0x00;      // clear status

   if(ide== STD)         // standard
   {
      CANIDT1= (unsigned char)(id>>3);
      CANIDT2= (unsigned char)(id<<5);

      CANIDM1= (unsigned char)(idmask>>3);
      CANIDM2= (unsigned char)(idmask<<5);
      CANIDM4=0;

      CANCDMOB &= ~0x10;   // clear IDE =0, standard 11 bits

      //usart1_transmit_string("\rRx Standard Set\n");
   }
   else               // extended
   {
      CANIDT1= (unsigned char)(id>>21);
      CANIDT2= (unsigned char)(id>>13);
      CANIDT3= (unsigned char)(id>>5);
      CANIDT4= (unsigned char)(id<<3);

      CANIDM1= (unsigned char)(idmask>>21);
      CANIDM2= (unsigned char)(idmask>>13);
      CANIDM3= (unsigned char)(idmask>>5);
      CANIDM4= (unsigned char)(idmask<<3);

      CANCDMOB |= 0x10;   // set IDE =1, extended 29 bits

      //usart1_transmit_string("\rRx Extended Set\n");
   }
   CANCDMOB |= (dlc & 0x0f);      // set data length

   CANIDM4 |= (unsigned char)(rtrIdemask & 0x07);
//   CANIDM4 &= ~0x04;      // RTRMSK= 1/0 enable comparison (Data receive)
//   CANIDM4 &= ~0x02;      // recommended
//   CANIDM4 &= ~0x01;      // IDEMSK= 1/0 enable comparison (IDE receive)

   CANCDMOB |= 0x80;      // receive enable 
}

//****************************************************************/
// CAN Interrupt ?�신 MOb ?�정 
// � ??
//      obj; MOb 번호 
//      id; CAN ID
//      ide; 메시�  ?�맷(0:2.0A, 1:2.0B)
//      dlc; 메시�  ?�이??길이 (최�? 8bytes)
//      idmask; CAN ID ?�신 마스??
//      rtrIdemask; RTR 비트?  IDE 마스??         
// ?�서:
//  1. MOb ?�택  
//  2. ID ?�정 
//  3. ?�맷 ?�정 
//  4. 마스???�정 
//  5. DLC ?�정 
//  6. ?�터?�트 ?�에?�블 (interrupt)
//  7. ?�신 모드 ?�정 
//****************************************************************/
void can_int_rx_set(char obj, unsigned long id, char rplvIde, 
               unsigned char dlc, unsigned long idmask, 
               unsigned char rtrIdemask)
{
   CANPAGE = obj<<4;      // set MOb number

   CANSTMOB = 0x00;      // clear status

   if(rplvIde & 0x02)   
      CANCDMOB |= 0x20;         // RPLV set, ?�동 ?�답 모드 ?�정 
   else
      CANCDMOB &= ~0x20;         // RPLV clear

   if(( rplvIde & 0x01) == STD)         // standard
   {
      CANIDT1= (unsigned char)(id>>3);
      CANIDT2= (unsigned char)(id<<5);

      CANIDM1= (unsigned char)(idmask>>3);
      CANIDM2= (unsigned char)(idmask<<5);
      CANIDM4=0;

      CANCDMOB &= ~0x10;   // clear IDE =0, standard 11 bits

      //usart1_transmit_string("\rRx Standard Set\n");
   }
   else               // extended
   {
      CANIDT1= (unsigned char)(id>>21);
      CANIDT2= (unsigned char)(id>>13);
      CANIDT3= (unsigned char)(id>>5);
      CANIDT4= (unsigned char)(id<<3);

      CANIDM1= (unsigned char)(idmask>>21);
      CANIDM2= (unsigned char)(idmask>>13);
      CANIDM3= (unsigned char)(idmask>>5);
      CANIDM4= (unsigned char)(idmask<<3);

      CANCDMOB |= 0x10;   // set IDE =1, extended 29 bits

      //usart1_transmit_string("\rRx Extended Set\n");
   }
   CANCDMOB |= (dlc & 0x0f);      // set data length

   CANIDM4 |= (unsigned char)(rtrIdemask & 0x07);
//   CANIDM4 &= ~0x04;      // RTRMSK= 1/0 enable comparison (Data receive)
//   CANIDM4 &= ~0x02;      // recommended
//   CANIDM4 &= ~0x01;      // IDEMSK= 1/0 enable comparison (IDE receive)


//  ?�터?�트 ?�에?�블(?�터?�트 ?�정)
   CANGIE |= 0xa0;       // Enable all interrupt and Enable Rx interrupt

   if(obj<8) 
      CANIE2 = (1<<obj);      // ?�당 MOb???�터?�트�??�에?�블 ?�킴 
   else        
      CANIE1 = (1<<(obj-8));   // 

   CANCDMOB |= 0x80;         // ?�신 ?�에?�블 
   sei();
}

//////////////////////////////

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

   PORTE&=0xFC;

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
    BIOS(); //initial setting
	PORTE=0x00;
    while(1){
        /*--------------------------------------------------*/
        
        if(cds>=700) PORTE |= 0x04;       //PE2 is for front light  0000 0100 
        else if(cds<700) PORTE &= 0xfb;
        /*----------------------------------------------------*/
		

    	can_rx(2,&msg1);
          
		if(msg1.data[5]==2) danger=2;
		else if(msg1.data[5]==1)danger=1;

		if(danger==2){
			Buzzer(200,0);
			sprintf(strBuff1,"WARNING!!!",danger-1);
			LCD_Write(0,0,strBuff1);
			PORTE&=0xFD;	
		}
		else if(danger==1){
			sprintf(strBuff1,"          ",danger-1);
			LCD_Write(0,0,strBuff1);
			PORTE=0xFF;
		 }
		 
		
		 
		if(!(temp-temp_buffer>5 || temp-temp_buffer>5)) temp_buffer=temp;
		msg2.data[3]=temp;
		can_tx(3,&msg2,0);
		us_delay(300);
		
		}
}
