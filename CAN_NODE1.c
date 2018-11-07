#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>         // sprintfÎ¨??†Ïñ∏?òÏñ¥ ?àÏùå 

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
// CAN Ï¥àÍ∏∞??   
// 1. CAN ?úÏñ¥Í∏?Î¶¨ÏÖã 
// 2. Î≥¥Î†à?¥Ìä∏ ?§Ï†ï 
// 3. MOb Î™®Îìú/?ÅÌÉú Ï¥àÍ∏∞??
// 4. MOb IDT, Mask Ï¥àÍ∏∞??
// 5. CAN General ?∏ÌÑ∞?ΩÌä∏ ?¥Î¶¨??CAN ?∏ÌÑ∞?ΩÌä∏ ?¨Ïö©?òÏ? ?äÏùå) 
// 6. CAN MOb ?∏ÌÑ∞?ΩÌä∏ ?¥Î¶¨??CAN MOb ?∏ÌÑ∞?ΩÌä∏ ?¨Ïö©?òÏ? ?äÏùå)
// 7. CAN ?úÏñ¥Í∏??∏Ïóê?¥Î∏î Î™®Îìú ?§Ï†ï 
// 8. CAN ?úÏñ¥Í∏??ôÏûë ?ïÏù∏ ??Ï¥àÍ∏∞???ÑÎ£å                             
//***************************************************************
void can_init (char baudRate)   // CANÏ¥àÍ∏∞??
{
   unsigned char i, j;
   
   CANGCON |= (1<<SWRES);   // CAN ?úÏñ¥Í∏?Î¶¨ÏÖã
                     // CAN General Control Register
                       
   //Î≥¥Î†à?¥Ìä∏ ?§Ï†ï ==============================================
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
         CANBT1= 0x06;   // CANÎ≥¥Î†à?¥Ìä∏ ?§Ï†ï 
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
// CAN Ï¥àÍ∏∞??   
// 1. CAN ?úÏñ¥Í∏?Î¶¨ÏÖã 
// 2. Î≥¥Î†à?¥Ìä∏ ?§Ï†ï 
// 3. MOb Î™®Îìú/?ÅÌÉú Ï¥àÍ∏∞??
// 4. MOb IDT, Mask Ï¥àÍ∏∞??
// 5. CAN General ?∏ÌÑ∞?ΩÌä∏ ?¥Î¶¨??CAN ?∏ÌÑ∞?ΩÌä∏ ?¨Ïö©?òÏ? ?äÏùå) 
// 6. CAN MOb ?∏ÌÑ∞?ΩÌä∏ ?¥Î¶¨??CAN MOb ?∏ÌÑ∞?ΩÌä∏ ?¨Ïö©?òÏ? ?äÏùå)
// 7. CAN ?úÏñ¥Í∏??∏Ïóê?¥Î∏î Î™®Îìú ?§Ï†ï 
// 8. CAN ?úÏñ¥Í∏??ôÏûë ?ïÏù∏ ??Ï¥àÍ∏∞???ÑÎ£å                             
//***************************************************************
void can_init_8Mhz(char baudRate)   // CANÏ¥àÍ∏∞??
{
   unsigned char i, j;
   
   CANGCON |= (1<<SWRES);   // CAN ?úÏñ¥Í∏?Î¶¨ÏÖã
                     // CAN General Control Register
                       
   //Î≥¥Î†à?¥Ìä∏ ?§Ï†ï ==============================================
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
// CAN Î©îÏãúÏß  ?ÑÏÜ° (?¥ÎßÅ Î∞©Ïãù)   
// Ï≤òÎ¶¨ ?úÏÑú 
// 1. MOb ?†ÌÉù 
// 2. MOb ?ÅÌÉú ?¥Î¶¨??
// 3. CANCDMOB ?¥Î¶¨??(Î™®Îìú ?§Ï†ï, ?êÎèô ?ëÎãµ, IDE, DLC)
// 4. ID ?§Ï†ï 
// 5. IDE ?§Ï†ï 
// 6. DLC ?§Ï†ï 
// 7. RTR ?§Ï†ï 
// 8. Î©îÏãúÏß  ?∞Ïù¥??? ??
// 9. Î©îÏãúÏß  ?°Ïã†    
// Î≥ ??
//      obj; MOb Î≤àÌò∏ 
//      msg; Î©îÏãúÏß  Íµ¨Ï°∞Ï≤?
//      rtr; RTR rÍ≤∞Ï†ï(0; ?∞Ïù¥???ÑÎ†à?? 1; Î¶¨Î™®???ÑÎ†à??   
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

   CANIDT4 |= (rtr & 0x04);     // RTRTAG ?§Ï†ï;

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
// CAN Î©îÏãúÏß  ?òÏã† (?¥ÎßÅ Î∞©Ïãù)
// 1. ?òÏã† MOb ?†ÌÉù 
// 2. ?òÏã† ?ÑÎ£å ?ïÏù∏ 
// 3. ?úÏ? ?πÏ? ?ïÏû• Î≤ÑÏ†Ñ ?ïÏù∏ 
// 4. ID ?òÏã† Ï≤òÎ¶¨ 
// 5. IDE ?òÏã† Ï≤òÎ¶¨ 
// 6. DLC ?òÏã† Ï≤òÎ¶¨ 
// 7. Data ?òÏã† Ï≤òÎ¶¨ 
//***************************************************************
char can_rx(unsigned char obj, struct MOb *msg)      
{
   char rece_result = _RECE_FAIL;
   unsigned char i;   
   unsigned long can_id= 0;
   
   CANPAGE = (obj<<4);            // ?òÏã† MOb ?†ÌÉù 

   //usart1_transmit_string("\rRX MOb #");
   //usart1_transmit(obj+0x30);
   //usart1_transmit_string("\r\n");

   // ?òÏã† ?ÑÎ£å???åÍπåÏß  ? Í∏∞Ìï® 
   while(!(CANSTMOB&(1<<RXOK)));
   // ?òÏã†???ÑÎ£å?òÎ©¥ 
   // CANIDT, CANCDMOB, CANMSG???òÏã† Î©îÏãúÏß Í∞  ? ?•Îê® 
   // get CANIDT and CANCDMOB and CANMSg
   //usart1_transmit_string("\rRXOK\n");
   rece_result = _RECE_OK;

   // ?úÏ? ?πÏ? ?ïÏû• ?¨Îß∑??ÎßûÏ∂î??ID, IDE Í≤∞Ï†ï 
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
   msg->id= can_id;         // Íµ¨Ï°∞Ï≤?Î≥ ?òÎ°ú CANID ? ??

   msg->rtr= CANIDT4 & 0x04;   

   msg->dlc= CANCDMOB & 0x0f;   // ?òÏã† Î©îÏãúÏß  Í∏∏Ïù¥ Íµ¨Ï°∞Ï≤?Î≥ ?òÏóê ? ??

   // get data
   for(i=0; i<(CANCDMOB&0xf); i++){
      msg->data[i] = CANMSG;   // Î©îÏãúÏß  ?∞Ïù¥??Î∞∞Ïó¥??? ??
   }

   // rx init 
   CANSTMOB = 0x00;         // ?ÅÌÉú Ï¥àÍ∏∞ ??

   // enable reception mode and ide set
   CANCDMOB |= (1<<CONMOB1);    // ?òÏã† IDE ?†Ï??òÍ≥† ?òÏã† Î™®Îìú ?§Ï†ï

   // reset flag
   CANSTMOB &= ~(1<<RXOK);      // ?òÏã†? Í∏?

   return(rece_result);
}

//****************************************************************/
// CAN ?òÏã† MOb ?§Ï†ï 
// Î≥ ??
//      obj; MOb Î≤àÌò∏ 
//      id; CAN ID
//      ide; Î©îÏãúÏß  ?¨Îß∑(0:2.0A, 1:2.0B)
//      dlc; Î©îÏãúÏß  ?∞Ïù¥??Í∏∏Ïù¥ (ÏµúÎ? 8bytes)
//      idmask; CAN ID ?òÏã† ÎßàÏä§??
//      rtrIdemask; RTR ÎπÑÌä∏?  IDE ÎßàÏä§??         
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
// CAN Interrupt ?òÏã† MOb ?§Ï†ï 
// Î≥ ??
//      obj; MOb Î≤àÌò∏ 
//      id; CAN ID
//      ide; Î©îÏãúÏß  ?¨Îß∑(0:2.0A, 1:2.0B)
//      dlc; Î©îÏãúÏß  ?∞Ïù¥??Í∏∏Ïù¥ (ÏµúÎ? 8bytes)
//      idmask; CAN ID ?òÏã† ÎßàÏä§??
//      rtrIdemask; RTR ÎπÑÌä∏?  IDE ÎßàÏä§??         
// ?úÏÑú:
//  1. MOb ?†ÌÉù  
//  2. ID ?§Ï†ï 
//  3. ?¨Îß∑ ?§Ï†ï 
//  4. ÎßàÏä§???§Ï†ï 
//  5. DLC ?§Ï†ï 
//  6. ?∏ÌÑ∞?ΩÌä∏ ?∏Ïóê?¥Î∏î (interrupt)
//  7. ?òÏã† Î™®Îìú ?§Ï†ï 
//****************************************************************/
void can_int_rx_set(char obj, unsigned long id, char rplvIde, 
               unsigned char dlc, unsigned long idmask, 
               unsigned char rtrIdemask)
{
   CANPAGE = obj<<4;      // set MOb number

   CANSTMOB = 0x00;      // clear status

   if(rplvIde & 0x02)   
      CANCDMOB |= 0x20;         // RPLV set, ?êÎèô ?ëÎãµ Î™®Îìú ?§Ï†ï 
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


//  ?∏ÌÑ∞?ΩÌä∏ ?∏Ïóê?¥Î∏î(?∏ÌÑ∞?ΩÌä∏ ?§Ï†ï)
   CANGIE |= 0xa0;       // Enable all interrupt and Enable Rx interrupt

   if(obj<8) 
      CANIE2 = (1<<obj);      // ?¥Îãπ MOb???∏ÌÑ∞?ΩÌä∏Î•??∏Ïóê?¥Î∏î ?úÌÇ¥ 
   else        
      CANIE1 = (1<<(obj-8));   // 

   CANCDMOB |= 0x80;         // ?òÏã† ?∏Ïóê?¥Î∏î 
   sei();
}

//////////////////////////////

//=========================================================
// AD Î≥ ??
// PF0: ADC0 Ï°∞Ïù¥?§Ìã± yÏ∂?
// PF1: ADC1 Ï°∞Ïù¥?§Ìã± xÏ∂?
//=========================================================

void initAdc(void){
	ADMUX = 0x40;   // AVcc with external capacitor on AREF pin, ?ïÎ†¨ Í∑∏Î?Î°?0100  0000
               // ADC0Îß??¨Ïö©(channel 0Î≤?  
	DDRF = 0xf0;   // PortF[3..0] ?ÖÎ†•?ºÎ°ú ?§Ï†ï, PortF[7..4] Ï∂úÎ†•?ºÎ°ú ?§Ï†ï 
	DIDR0 = 0x07;   // ?îÏ????ÖÎ†• Î∂àÍ? PortF[3..0]
	ADCSRA= 0xC8;   // ADC ?∏Ïóê?¥Î∏î, ADC Î≥ ???úÏûë, ADC?∏ÌÑ∞?ΩÌä∏ ?∏Ïóê?¥Î∏î 
               // ADC ?¥Îü≠ ?§Ï†ï; XTAL??/2(8MHz)   
}

SIGNAL(ADC_vect){
   ADdata= ADC;   // ADÎ≥ ???∞Ïù¥?∞Î? ADdata ??? ??
               //( Register???àÎäîÍ∞íÏùÑ ?ÑÏó≠Î≥ ??ADdata????≤® ?¨Ïö©?òÍ∏∞ ?ΩÍ≤å ?¥Ïö©?úÎã§. )
   if(adcchannel==0){
        adcchannel=1;
        ADMUX = 0x41;
        cds=ADdata;
    }else{
        adcchannel=0;
        ADMUX = 0x40;
        temp = ADdata/20;
    }
    ADCSRA= 0xC8;     // ADC ?∏Ïóê?¥Î∏î, ADC Î≥ ???úÏûë, ADC?∏ÌÑ∞?ΩÌä∏ ?∏Ïóê?¥Î∏î 
               // ADC ?¥Îü≠ ?§Ï†ï; XTAL??/2(8MHz)
}

void initPort(void){
   	DDRC  = 0xff;
	PORTC = 0xff;
	DDRG  = 0xff;
	DDRF  = 0xfc;
	DDRE  = 0xff;
}

void BIOS(){        // setting when program first load.
    initPort();      // ?ÖÏ∂ú???¨Ìä∏ Ï¥àÍ∏∞??
    LCD_init();     // LCD Ï¥àÍ∏∞??
    initAdc();
    initMotor();
    LCD_cmd(0x01);
   
    sprintf(strBuff1, "Starting");
    LCD_Write(0, 0, strBuff1);
    ms_delay(1000);
    LCD_cmd(0x01);

    can_init(b500k);       // Ï¥àÍ∏∞??
    can_rx_set(2, 0x00, EXT, 8, 0x00000000, 0x00);    // CAN ?òÏã†Í∏?Ï¥àÍ∏∞??
    //        obj  id   ide  dlc  idmask   rtrldemask   

    sei();         // INT ?∏Ïóê?¥Î∏î
}
void Buzzer(unsigned int speed, unsigned int dir)
{
   unsigned int level=5;

   PORTE&=0xFC;

   // DC???? ??? ???? ????
   if(dir==0)      // ?©£? ???? ???
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
