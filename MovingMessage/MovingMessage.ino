/*******************************************************************
 *  Main application sketch for Scrolling Message Display          *
 *  written by BSP Embed (BSP-Embed on Github)                     *
 *  No Libraries are used, as i need full control of the device    * 
 *  Demo Available on YouTube BSP Embed                            *
 *******************************************************************/
#include <EEPROM.h>           /* For Storing Message */
#include "font5_7.h"          /*Font Table, Support ASCII characters */

/*****************************/
/*      CONSTANTS            */
/*****************************/
#define F_CPU             16000000UL
#define USART_BAUDRATE    9600
#define BAUD_PRESCALE     ((F_CPU / (USART_BAUDRATE * 16UL)) - 1)
#define MAGIC_NO          68      /* For EEPROM Detection */
#define MAGIC_ADD         0       
#define MSG_ADD           1
#define MAX_MSG           100
#define TIMER1_RELOAD     63936   /* Rate of Multiplexing */
#define TIMER2_RELOAD     10       /* Scrolling Speed */
#define SCROLL_SPEED      3       /* scroll speed */

#define DATA_P            PB0
#define CLK_P             PB1

#define EnUARTInt()       UCSR0B |= _BV(RXCIE0); UCSR0A |= _BV(RXC0)  
#define DisUARTInt()      UCSR0B &= ~_BV(RXCIE0); UCSR0A &= ~_BV(RXC0)

#define DisDisp()         do {                        \
                            TCCR1B &= ~_BV(CS10);      \   
                            TCCR2B &=  ~(_BV(CS22) | _BV(CS21) | _BV(CS20)); \
                          }while (0)

#define EnDisp()          do {                        \
                            TCCR1B |= _BV(CS10);      \   
                            TCCR2B |= _BV(CS22) | _BV(CS21) | _BV(CS20); \
                          }while (0)  

/*****************************/
/*      Gloabl Vraibales     */
/*****************************/                               
char sbuf[MAX_MSG]= "Happy New Year 2017";
boolean             sflag = 0;
boolean             INflag = 0;
uint8_t             chcnt;
uint8_t             count;
uint8_t             chptr;
volatile uint8_t    msglen;
volatile uint16_t   offset;
uint16_t            ch_base;

/***************************/
/*      Program Begins     */
/***************************/                      
void setup() {
  cli();           /* disable all interrupts */
  UartInit();
  ReadEEPROM();
  DotDispInit();
  TmrInit();   
  sei();            /* enable all interrupts */
}
void loop() {
  if (sflag) {
    sflag = 0;
    WriteMsg(sbuf, MSG_ADD);
    DotDispInit();
    EnUARTInt();
    sei(); 
  }
}
/******************************************/
/*  Timer1 Interrupt for Multiplexing     */
/*****************************************/ 
ISR(TIMER1_OVF_vect) {
  TCNT1 = TIMER1_RELOAD; 
  if (count < 60) {
    PORTB &= ~_BV(CLK_P); 
    if (chcnt == 5) {
        PORTD = (0xFF << 1); 
        chcnt = 0;
        chptr++; 
        ch_base = (sbuf[chptr] - 0x20) * 5;
    } else {
      PORTD = ~(Font5x7[ch_base+chcnt] << 1);
      chcnt++;
    } 
    count++;
    PORTB |= _BV(CLK_P); 
    PORTB &= ~_BV(DATA_P);
  } else {
     chcnt = offset % 6; 
     chptr = offset / 6;
     ch_base = (sbuf[chptr] - 0x20) * 5;
     count = 0;   
     PORTB |= _BV(DATA_P);   
  }
}
/******************************************/
/*  Timer2 Interrupt for Scrolling       */
/*****************************************/
ISR(TIMER2_OVF_vect) {
  static uint8_t i;
  TCNT2 = TIMER2_RELOAD;
  if (++i >= SCROLL_SPEED) {
    i = 0;
    if(++offset == ((msglen * 6) - 60))
      offset = 0;
  }
}
/******************************************/
/*      UART Receive Interrupt            */
/* Receive character & Store in Buffer    */
/*****************************************/
ISR (USART_RX_vect) {
  static uint8_t i, j;
  char ch = UDR0;
  if (INflag || (ch == '*')) {
    if (!INflag) { 
      INflag = 1;
      DisDisp();
    }
    if ((sbuf[i++] = ch) == '#') {
      cli();
      DisUARTInt();
      for (j = 1; j < i-1; j++)
        sbuf[j-1] = sbuf[j]; 
      sbuf[j-1] = '\0'; 
      i = INflag = 0;
      sflag = 1;
   }  
 } 
}
/*********************************************/
/* Timer1&2 Initialize for Multiplex & scroll*/
/*********************************************/
void TmrInit(void) {
  TCCR1A = 0;
  TCCR1B = 0;

  TCNT1   = TIMER1_RELOAD;     
  TCCR1B |= (1 << CS10);      /* PRESCALAR BY 1 */
  TIMSK1 |= (1 << TOIE1);     /* ENABLE OVERFLOW INTERRUPT */
 
  TCNT2    = TIMER2_RELOAD;
  TIMSK2  |= _BV(TOIE2);      /* ENABLE OVERFLOW INTERRUPT */
  TCCR2B  |=  _BV(CS22) | _BV(CS21) | _BV(CS20); /* PRESCALAR BY 256 */
}
/******************************************/
/*      DOT Matrix Initialize            */
/*****************************************/
static void DotDispInit(void) {
  DDRB |= 0x03;
  PORTD = DDRD = 0xFF;
  PORTB &= ~_BV(DATA_P);  
  chcnt = offset = count = chptr = 0;
  FormatMsg(sbuf);
  msglen = strlen(sbuf);
  ch_base = (sbuf[chptr] - 0x20) * 5;
  EnDisp();                       /* Start Timer */
}

/******************************************/
/*UART with receive interrupt initialize */
/*****************************************/
void UartInit(void) {
  UCSR0B |= _BV(RXEN0);                 /* Turn on Rx */
  UCSR0C |= _BV(UCSZ00) | _BV(UCSZ01); /* 8N1 */
  UBRR0L = BAUD_PRESCALE;
  UBRR0H = (BAUD_PRESCALE >> 8);
  EnUARTInt();
}
/**************************************/
/* Read Frequency, ModeOP, Channel No */
/* If EEPROM is blank, Write Defaults */
/**************************************/
void ReadEEPROM(void){
  if (EEPROM.read(MAGIC_ADD) != MAGIC_NO) {   /* New EEPROM, Store Default Msg */
    EEPROM.write(MAGIC_ADD, MAGIC_NO);
    WriteMsg(sbuf, MSG_ADD);
  } else                                     
    ReadMsg(sbuf, MSG_ADD);
}
/**************************************/
/* Write Message to EEPROM as string * /
/**************************************/
void WriteMsg(char *Msg, uint8_t MsgAdd) {
  byte i, j;
  i = MsgAdd; j = 0;
  while (Msg[j] != '\0') { 
    EEPROM.write(i, Msg[j]);  
    i++, j++;
  }  
  EEPROM.write(i, '\0');
}
/**************************************/
/*   Read Message From EEPROM         */
/**************************************/
void ReadMsg(char *Msg, uint8_t MsgAdd) {
  byte i, j;
  i = MsgAdd; j = 0;
  while ((Msg[j] = EEPROM.read(i)) != '\0') {
    i++; j++;
  }
  Msg[j] = '\0';
}
/*************************************/
/* Attach Head (Leading Spaces)      */
/*************************************/
void FormatMsg(char *Msg) {
  int8_t len, k;
  len = 0;
  while (Msg[len++] != '\0');
  len--;
  for (k = 0; k <= 10; k++)
    Msg[len++] = ' ';
  len--;
  Msg[len] = '\0';
  for (; len >= 0; len--)
    Msg[len+10] = Msg[len];
  for (k = 0; k < 10; k++)
    Msg[k] = ' ';
}

        
