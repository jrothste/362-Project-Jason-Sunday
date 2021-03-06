/*******************************************************************
ECE 362 MINI PROJECT CODE
JASON ROTHSTEIN, JORDAN WARNE, DAVID PIMLEY
"SIMON SAYS"

PWM CH0 for Speaker
ATD CH0 for Potentiometer (difficulty)
RTI @ 2.048 ms (sample pushbuttons)
TIM CH7 @ 1 ms (with interrupts) for game timing
Port T pins 1-6 for LED outputs
Port AD pins 1-6 for pushbutton inputs
*******************************************************************/

#include <hidef.h>      /* common defines and macros */
#include "derivative.h"      /* derivative-specific definitions */
#include <mc9s12c32.h>

/* Declare all functions called from/after main here */
void shiftout(char);
void lcdwait(void);
void send_byte(char);
void send_i(char);
void chgline(char);
void print_c(char);
void pmsglcd(char[]);

void win(void);
void lose(void);
void generateOrder(void);
void dispround(void);
void lightup(char);
void waitlevel(void);
void selectDiff(void);

/* Variable Declarations */
char button1 = 0; //button flags
char button2 = 0; //1-6 in left-to-right order
char button3 = 0;
char button4 = 0;
char button5 = 0;
char button6 = 0;
/*char LED1 = 0;
char LED2 = 0;        //may not be necessary
char LED3 = 0;
char LED4 = 0;
char LED5 = 0;
char LED6 = 0;*/
char startbutton = 0;

char round = 0;
char guessround = 0;
char startflg = 1;
char playflg = 0;
char sequence[250];

char prev1;
char prev2;
char prev3;
char prev4;
char prev5;
char prev6;
char prevstart;

char difficulty;

char tenthflg = 0;
char secflg = 0;
char milli = 0;
char tenth = 0;
char tenthadder = 0;

//to be removed //
char hundreds = 0;
char tens = 0;
char ones = 0;
char ind1 = 0;
char ind2 = 0;
char ind3 = 0;


//#define DIF1 0x00
#define DIF2 0x33    //difficulty thresholds (fractions of 0xFF)
#define DIF3 0x66
#define DIF4 0x99
#define DIF5 0xCC

#define BUTTON_1 0   //lookup value for button sequence
#define BUTTON_2 1
#define BUTTON_3 2
#define BUTTON_4 3
#define BUTTON_5 4
#define BUTTON_6 5

#define LED1 PTT_PTT1   //route LEDs and PBs to hardware on Port T and ATD
#define LED2 PTT_PTT2
#define LED3 PTT_PTT3
#define LED4 PTT_PTT4
#define LED5 PTT_PTT5
#define LED6 PTT_PTT6

#define PB1 PTAD_PTAD1
#define PB2 PTAD_PTAD2
#define PB3 PTAD_PTAD3
#define PB4 PTAD_PTAD4
#define PB5 PTAD_PTAD5
#define PB6 PTAD_PTAD6
#define PBSTART PTAD_PTAD7


/* Other Configurations */ 

/* ASCII character definitions */
#define CR 0x0D	// ASCII return character   

/* LCD COMMUNICATION BIT MASKS */
#define RS 0x04		// RS pin mask (PTT[2])
#define RW 0x08		// R/W pin mask (PTT[3])
#define LCDCLK 0x10	// LCD EN/CLK pin mask (PTT[4])

/* LCD INSTRUCTION CHARACTERS */
#define LCDON 0x0F	// LCD initialization command
#define LCDCLR 0x01	// LCD clear display command
#define TWOLINE 0x38	// LCD 2-line enable command
#define CURMOV 0xFE	// LCD cursor move instruction
#define LINE1 0x80	// LCD line 1 cursor position
#define LINE2 0xC0	// LCD line 2 cursor position


/*	 	   		
***********************************************************************
 Initializations
***********************************************************************
*/

void  initializations(void) {

/* Set the PLL speed (bus clock = 24 MHz) */
  CLKSEL = CLKSEL & 0x80; // disengage PLL from system
  PLLCTL = PLLCTL | 0x40; // turn on PLL
  SYNR = 0x02;            // set PLL multiplier
  REFDV = 0;              // set PLL divider
  while (!(CRGFLG & 0x08)){  }
  CLKSEL = CLKSEL | 0x80; // engage PLL

/* Disable watchdog timer (COPCTL register) */
  COPCTL = 0x40   ; // COP off; RTI and COP stopped in BDM-mode

/* Initialize asynchronous serial port (SCI) for 9600 baud, interrupts off initially */
  SCIBDH =  0x00; //set baud rate to 9600
  SCIBDL =  0x9C; //24,000,000 / 16 / 156 = 9600 (approx)  
  SCICR1 =  0x00; //$9C = 156
  SCICR2 =  0x0C; //initialize SCI for program-driven operation
  DDRB   =  0x10; //set PB4 for output mode
  PORTB  =  0x10; //assert DTR pin on COM port
 
/* Add additional port pin initializations here */
   DDRM = 0xFF; //Set Port M pins 0 and 1 to Outputs.

/* Initialize SPI for baud rate of 6 Mbs */
   SPIBR = 0x01; //Set SPT Baud Rate to 6.25 Mbs 
   SPICR1 = 0x50;
   SPICR2 = 0x00;
   SPISR = 0x00;
   
/* Initialize digital I/O port pins */
  DDRT = 0x7E; //Port T pins 1-6 are outputs (for LED's)
  ATDDIEN = 0xFE; //Port AD pins 1-7 are inputs (for Pushbuttons);         
         
/* Initialize TIM Ch 7 (TC7) for periodic interrupts every 1.000 ms
     - enable timer subsystem
     - set channel 7 for output compare
     - set appropriate pre-scale factor and enable counter reset after OC7
     - set up channel 7 to generate 1 ms interrupt rate
     - initially disable TIM Ch 7 interrupts      
*/
  TSCR1 = 0x80; //Set Timer Enable Bit
  TIOS  = 0x80; //Set Channel 7 to Output Compare
  TSCR2 = 0x0C; //Reset Counter and Prescaler = 16.
  TC7   = 1500; //Interrupt every 1500 ticks. 
  TIE   = 0x00; //Initially disable TIM Ch 7 Interrupts  


/* Initialize PWM Channel 0*/
  PWME = 0x00; //Initially dissable PWM channel 0;
  PWMPOL = 0x01; //Set PWM channel 0 to positive polarity.
  PWMCLK = 0x01; //Set clock for PWM channel 0 to Scaled Clock A.
  PWMPRCLK = 0x07; //Clock A = 187.5kHz.
  PWMSCLA = 0x20; //Scaled Clock A = 2900Hz: ///By adjusting period: Frequency Range: 11.5-1460 Hz.///
  PWMPER0 = 2; //1460 Hz
  PWMDTY0 = 1;  //50% duty Cycle.
  MODRR = 0x01; //Set PWM channel 0 output on PTT0.

  
/* Initialize ATD Channel 0*/
  ATDCTL2 = 0x80; //Power Up ATD
  ATDCTL3 = 0x08; //1 conversion per sequence.   (non-FIFO mode)
  ATDCTL4 = 0x85; //2Mhz ATD clock, 8-bit mode.


/*
  Initialize the RTI for an 2.048 ms interrupt rate
*/
  CRGINT = 0x80;
  RTICTL = 0x1F;  


/* 
   Initialize the LCD
     - pull LCDCLK high (idle)
     - pull R/W' low (write state)
     - turn on LCD (LCDON instruction)
     - enable two-line mode (TWOLINE instruction)
     - clear LCD (LCDCLR instruction)
     - wait for 2ms so that the LCD can wake up     
*/ 
  PTM = PTM | LCDCLK; //pull LCD clk high
 	send_i(LCDON);
  send_i(TWOLINE);
  send_i(LCDCLR);
  lcdwait();		 		  			 		  		
	      
}



/*	 		  			 		  		
***********************************************************************
Main                                  
***********************************************************************
*/


void main(void) {
  /* put your own code here */
  initializations();

	EnableInterrupts;

  for(;;) {
  
    while (startflg) {    //startflg indicates phase before game starts
      send_i(LCDCLR);
      chgline(LINE1);
      pmsglcd("Difficulty:");
      selectDiff();            //select difficulty (continuous)
      if (startbutton) {
        startflg = 0;         //exit loop when start button is pressed, and start game
        round = 0;
        generateOrder();   //come up with random sequence to match
        dispround();        //display first round
        playflg = 1;
      }
    }
    
   /* if (tenthflg) {
      tenthflg = 0;
      if (tenthadder == 255) {
        tenthadder = 0;
      }
      
     */ 
    
    if (button1 && playflg) {      //button push only counts when playflg on (like an enable)
      button1 = 0;
      playflg = 0;               //flag off while response actions occur
      lightup(BUTTON_1);     //light & sound response
      if (BUTTON_1 == sequence[guessround]) {
        if (guessround == round) {
          win();
        } else {
          guessround++;        //correct guess, but pattern not done yet
        }
        playflg = 1;
      } else {
        lose();
      }
    }
        
          
    
  } /* loop forever */
}  /* never leave main */

/*
***********************************************************************
 RTI interrupt service routine: RTI_ISR
  Initialized for 2.048 ms interrupt rate
  Samples state of pushbuttons
***********************************************************************
*/
interrupt 7 void RTI_ISR(void)
{
  	// clear RTI interrupt flag
  	CRGFLG = CRGFLG | 0x80; 
  	if (PB1 < prev1) {     //sample each pushbutton (not RESET)
  	  button1 = 1;         //set button flag
  	}
  	prev1 = PB1;
  	
  	if (PB2 < prev2) {
  	  button2= 1;
  	}
  	prev2 = PB2;
  	
  	if (PB3 < prev3) {
  	  button3= 1;
  	}
  	prev3 = PB3;
  	
  	if (PB4 < prev4) {
  	  button4= 1;
  	}
  	prev4 = PB4;
  	
  	if (PB5 < prev5) {
  	  button5= 1;
  	}
  	prev5 = PB5;
  	
  	if (PB6 < prev6) {
  	  button6= 1;
  	}
  	prev6 = PB6;
  	
  	if (PBSTART < prevstart) {
  	  startbutton= 1;
  	}
  	prevstart = PBSTART;
  
}

/*
*********************************************************************** 
  TIM Channel 7 interrupt service routine
  Initialized for 1.00 ms interrupt rate
  Increment (3-digit) BCD variable "react" by one
***********************************************************************
*/

interrupt 15 void TIM_ISR(void)
{
	// clear TIM CH 7 interrupt flag
 	TFLG1 = TFLG1 | 0x80; 
 		
 	milli++;              //coutns each ms
 	if(milli == 100) {
 	  tenth++;            //counts each 0.1 s
 	  milli = 0;
 	  tenthflg = 1;
 	}
 	if (tenth == 10) {
 	  secflg = 1;
 	  tenth = 0;
 	  tenthadder++;       //counts up intervals of 0.1 sec - used such that its only reset in wait function (or when it hits 255)
 	}
 	/*
 	if((PTT & 0x7E) == 0x7E) {
 	  PTT = PTT & ~0x7E;
 	} else {
 	  PTT = PTT | 0x7E;
 	}
 	*/
}

/***************************************************************************** 
User Defined/Helper Functions called in Main 
*****************************************************************************/

void selectDiff() {          //uses potentiometer to select difficulty
  difficulty = ATDDR0H;
  chgline(LINE2);
  pmsglcd("                ");    //clears second line
  chgline(LINE2);
  if (difficulty > 200) {          //print difficulty as it is being selected
    pmsglcd("WTF");
  } else if (difficulty > 150) {
    pmsglcd("Pro");
  } else if (difficulty > 100) {
    pmsglcd("Shmedium");
  } else if (difficulty > 50) {
    pmsglcd("Amateur");
  } else {
    pmsglcd("Chump");
  }
}


void dispround() {
  /* display right round on LCD */
  send_i(LCDCLR);
  chgline(LINE2);
  pmsglcd("Round: ");
  hundreds = ((round + 1) / 100) % 10; //digits of round score
  tens = ((round + 1) / 10) % 10; //round + 1 because it is an index (starts at 0)
  ones = (round + 1) / 10;
  if (hundreds > 0) {  //controls where digits are displayed based on magnitude of round
    print_c(hundreds + 48);
  }
  if (tens > 0) {
    print_c(tens + 48);
  }
  if (ones > 0) {
    print_c(ones + 48);
  }
  chgline(LINE1);
  pmsglcd("Simon Says...");
  /* display round output on the lights */
  for (ind1; ind1 <= round; ind1++) {
    lightup(sequence[ind1]);
    waitlevel();
  }
  
  /* Tell user to start input */
  chgline(LINE1);
  pmsglcd("                ");
  chgline(LINE1);
  pmsglcd("Go!");
}
  
  
  
void generateOrder() {  //generates random game sequence
  for (ind2; ind2 < 250; ind2++) {
    sequence[ind2] = rand() % 6; //random number 0-5
  }
}



void lightup(char button) { //momentary lights & sounds whenever button is pressed (our round sequence output)
  if (button == BUTTON_1) {
    LED1 = 1;
    //play frequency 1
  } else if (button == BUTTON_2) {
    LED2 = 1;
    //play frequency 2
  } else if (button == BUTTON_3) {
    LED3 = 1;
    //play frequency 3
  } else if (button == BUTTON_4) {
    LED4 = 1;
    //play frequency 4
  } else if (button == BUTTON_5) {
    LED5 = 1;
    //play frequency 5
  } else if (button == BUTTON_6) {
    LED6 = 1;
    //play frequency 6
  }
  waitlevel();
  
  LED1 = 0; //reset lights to 0
  LED2 = 0;
  LED3 = 0;
  LED4 = 0;
  LED5 = 0;
  LED6 = 0;
}



void win() {
  chgline(LINE1);  //success message
  pmsglcd("                ");
  chgline(LINE1);
  pmsglcd("Correct!");
  
  LED1 = 1; //all lights flash
  LED2 = 1;
  LED3 = 1;
  LED4 = 1;
  LED5 = 1;
  LED6 = 1;
  //play 2-3 tone jingle
  LED1 = 0;
  LED2 = 0;
  LED3 = 0;
  LED4 = 0;
  LED5 = 0;
  LED6 = 0;
  
  round++;
  guessround = 0;
  dispround();
}



void lose() {
  send_i(LCDCLR);   //failure message
  chgline(LINE1);
  pmsglcd("Game Over!");  
  LED2 = 1;  //CHANGE HERE - only want RED lights to light up when you lose
  LED5 = 1;
  //play error jingle
  LED2 = 0;
  LED5 = 0;
  chgline(LINE2);
  pmsglcd("Final Score: ");
  //same display round code as dispround function
  hundreds = (round / 100) % 10; //digits of round score
  tens = (round / 10) % 10; //round because need last COMPLETED round
  ones = round / 10;
  if (hundreds > 0) {  //controls where digits are displayed based on magnitude of round
    print_c(hundreds + 48);
  }
  if (tens > 0) {
    print_c(tens + 48);
  }
  if (ones > 0) {
    print_c(ones + 48);
  }
}
 
 
 
void waitlevel() {       
  if (difficulty > 200) {          //determine difficulty level given thresholds
    ind3 = 1;
  } else if (difficulty > 150) {
    ind3 = 3;                          //i values are arbitrary - (i.e. i=5 waits 0.2*5= 1 second)
  } else if (difficulty > 100) {
    ind3 = 5;
  } else if (difficulty > 50) {
    ind3 = 7;
  } else {
    ind3 = 9;
  } 
    
  for (ind3; ind3 > 0; ind3--) {       ///difficulty determines how many times this runs
    tenthadder = 0;
    while (tenthadder < 2) {   ///waits 0.2 sec
    }
  }
}
  


/* CODE TAKEN FROM LAB ASSIGNMENTS - LCD MESSAGE CONFIGURATION */
/*
***********************************************************************
  shiftout: Transmits the character x to external shift 
            register using the SPI.  It should shift MSB first.  
            MISO = PM[4]
            SCK  = PM[5]
***********************************************************************
*/
 
void shiftout(char x)

{
 
  // test the SPTEF bit: wait if 0; else, continue
  // write data x to SPI data register
  // wait for 30 cycles for SPI data to shift out 

  int ind;
  while (SPISR_SPTEF == 0) {
  }
  SPIDR = x;
  for (ind = 0; ind < 30; ind++) {
  }
}

/*
***********************************************************************
  lcdwait: Delay for approx 2 ms
***********************************************************************
*/

void lcdwait()
{
  int ind;
  for (ind = 0; ind < 5000; ind++) {
  }
}

/*
*********************************************************************** 
  send_byte: writes character x to the LCD
***********************************************************************
*/

void send_byte(char x)
{
     // shift out character
     // pulse LCD clock line low->high->low
     // wait 2 ms for LCD to process data
  
  shiftout(x);
  PTT_PTT6 = 0;
  PTT_PTT6 = 1;
  PTT_PTT6 = 0;
  lcdwait();
}

/*
***********************************************************************
  send_i: Sends instruction byte x to LCD  
***********************************************************************
*/

void send_i(char x)
{
        // set the register select line low (instruction data)
        // send byte
  PTT_PTT4 = 0;
  send_byte(x);
}

/*
***********************************************************************
  chgline: Move LCD cursor to position x
  NOTE: Cursor positions are encoded in the LINE1/LINE2 variables
***********************************************************************
*/

void chgline(char x)
{
  send_i(CURMOV);
  send_i(x);
}

/*
***********************************************************************
  print_c: Print (single) character x on LCD            
***********************************************************************
*/
 
void print_c(char x)
{
  PTT_PTT4 = 1;
  send_byte(x);
}

/*
***********************************************************************
  pmsglcd: print character string str[] on LCD
***********************************************************************
*/

void pmsglcd(char str[])
{
  int ind;
  ind = 0;
  while (str[ind] != 0) {
    print_c(str[ind]);
    ind++;
  }
}