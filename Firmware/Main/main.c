/*********************************************************************************
 * Logomatic V2 Firmware
 * Sparkfun Electronics 2008
 * ******************************************************************************/

/*******************************************************
 * 		     Header Files
 ******************************************************/
#include <stdio.h>
#include <string.h>
#include "LPC21xx.h"
#include "string_printf.h"
//#include "system.h"

//UART0 Debugging
#include "serial.h"
#include "rprintf.h"

//SPI1 for accelerometer
#include "SPI1.h"
#include "ADXL362.h"
#include "xl362.h"

//Needed for main function calls
#include "main_msc.h"
#include "fat16.h"
#include "armVIC.h"
#include "itoa.h"
#include "rootdir.h"
#include "sd_raw.h"
#include "type.h"

 //test


/*******************************************************
 * 		     Global Variables
 ******************************************************/

#define ON	1
#define OFF	0
#define ADC_BUFFER_LENGTH 	128 //buffer size in BYTES, so number of readings is HALF of this
#define OVERSAMPLING_AMOUNT  16  //number of ADC readings per recorded reading (256 desired), MUST be even
#define OVERSAMPLE_SHIFT 2    //4^(OVERSAMPLE_SHIFT) = OVERSAMPLE_AMOUNT; OVERSAMPLE_SHIFT = num bits added
#define ADC_SAMPLES_PER_TRIG  1  // Try to keep it 1, ok
#define ADC_CHAN  1
#define ADC_FREQ  160

//Variables for the write to the SD card
 //RX_array 1 and 2 are the arrays you'll write data to
unsigned char RX_array1[512];
unsigned char RX_array2[512];
//log_array1 and 2 flag when the array is full; array is reset when it is read into the SD card
unsigned char log_array1 = 0;
unsigned char log_array2 = 0;
//RX_in holds the index that you're in on RX_array1 or 2
//0-511 is in log_array1; 512-1023 is in log_array2
short RX_in = 0;


//RX_array 1 and 2 are the arrays you'll write data to
unsigned char ADC_array1[ADC_BUFFER_LENGTH];
unsigned char ADC_array2[ADC_BUFFER_LENGTH];
//log_array1 and 2 flag when the array is full; array is reset when it is read into the SD card
unsigned char adc_array1_full = 0;
unsigned char adc_array2_full = 0;
//RX_in holds the index that you're in on RX_array1 or 2
//0-511 is in log_array1; 512-1023 is in log_array2
short ADC_index = 0;

//get_frame is for UART logging; a boolean that within the UART ISR will 
//be set high if the "trig" character (default $) is read.
char get_frame = 0;
//part of original code, haven't looked into it too much
signed int stringSize;
struct fat16_file_struct* handle;
struct fat16_file_struct * fd;
char stringBuf[256];


//int adc_trigger_index = 0;
int adc_oversampling_index = 0; //how many samples you've collected towards oversampling 
int adc_oversampling_result = 0;  //hold the ADC value while we're oversampling
unsigned short adc_ovs = 0;		//adc_oversampling_result is bit shifted to a number under 16 bits; this holds it for the write to the buffer


// Default Settings
//static char mode = 2; // 0 = auto uart, 1 = trigger uart, 2 = adc
//static char asc = 'N'; //ASCII.  N sets it to binary
//from original code
static int baud = 115200;  //setting 4 in defaults file
//static int freq = 100;   //ADC frequency setting; will overwrite this
//static char trig = '$';
//not entirely sure what this is for- changes size of arrays?
static short frame = 100;

//see here for circuit: http://www.freescale.com/files/microcontrollers/doc/app_note/AN4059.pdf?amp;tid=AMdlDR


/*******************************************************
 * 		 Function Declarations
 ******************************************************/

void Initialize(void);

void feed(void);
void initialize_adc(void);
static void IRQ_Routine(void) __attribute__ ((interrupt("IRQ")));
static void UART0ISR(void); //__attribute__ ((interrupt("IRQ")));
//static void UART0ISR_2(void); //__attribute__ ((interrupt("IRQ")));
static void ADC_TIMER_ISR(void); //__attribute__ ((interrupt("IRQ")));


void writeShortToSDBuffer(unsigned short data);
void writeCharToSDBuffer(unsigned char data);
void writeShortToADCBuffer(unsigned short data);
void writeCharToADCBuffer(unsigned char data);
unsigned int writeFromADCToSDBuffer(void);
void readAccDataFromFifo(void);

void setup_uart0(int newbaud, char want_ints);

void mode_0(void);
void mode_1(void);

void writeToSDCard(void);
void checkForButtonPress(void);

void test(void);
void stat(int statnum, int onoff);

void AD_conversion(int regbank);

void FIQ_Routine(void) __attribute__ ((interrupt("FIQ")));
void SWI_Routine(void) __attribute__ ((interrupt("SWI")));
void UNDEF_Routine(void) __attribute__ ((interrupt("UNDEF")));

void fat_initialize(void);

void delay_ms(int count);


/*******************************************************
 * 		     	MAIN
 ******************************************************/

int main (void)  
{
	int i;
	char name[32];
	int count = 0;
	
	//seems to be a processor call; sets up interrupt modes in the processor
	//may not need since I'm only using IRQ, not FIQ (higher priority)
	enableFIQ();
	
	//Pin config, SPI0 setup for SD card (clock, SPI settings)
	Initialize();
	
	//sets up SD card, sets SPI0 to 1MHz
	fat_initialize();		

	//115200 baud, no interrupts enabled
	setup_uart0(baud, 0);

	// Flash Status Lights
	for(i = 0; i < 5; i++)
	{
		stat(0,ON);
		delay_ms(50);
		stat(0,OFF);
		stat(1,ON);
		delay_ms(50);
		stat(1,OFF);
	}

	//Initialized the SPI1 interface for peripherals
	SPI1_Init();

	//creates and names the new log file
	//Errors if there are already 250 files
	count++;
	string_printf(name,"LOG%02d.bin",count);
	while(root_file_exists(name))
	{
		count++;
		if(count == 250) 
		{
			rprintf("Too Many Logs!\n\r");
			while(1)
			{
				stat(0,ON);
				stat(1,ON);
				delay_ms(1000);
				stat(0,OFF);
				stat(1,OFF);
				delay_ms(1000);
			}

		}
		string_printf(name,"LOG%02d.bin",count);
	}
	
	handle = root_open_new(name);
	rprintf("log created\n\r");
		
	sd_raw_sync();	//write buffer to SD card

	ADXL362_Init();
	rprintf("ADXL Initialized\n\r");

	initialize_adc();  

	//Clear all arrays
	for (int i = 0; i < 512; i++) {
		RX_array1[i] = 0;
		RX_array2[i] = 0;
	}

	for (int i = 0; i < ADC_BUFFER_LENGTH; i++) {
		ADC_array1[i] = 0;
		ADC_array2[i] = 0;
	}

	//meaty big function loop!  
	while(1){

		//If an ADC buffer is full
		if(writeFromADCToSDBuffer()){
			//rprintf("Wrote ADC into SD buffer, reading ACC data\n\r");	
			readAccDataFromFifo();
		}
		writeToSDCard(); //writes if one of the SD buffers is full
		checkForButtonPress();
	}
    return 0;
}


/*******************************************************
 * 		     Initialize
 ******************************************************/

#define PLOCK 0x400

void Initialize(void)
{
	rprintf_devopen(putc_serial0);
	
	PINSEL0 = 0xCC351505;
	//Ha!  Sets P0.12 to 00 by the second hex # being C

	PINSEL1 = 0x15441801;
	//Sets P0.20 to GPIO by bits 8:9 = 00
	IODIR0 |= 0x00000884;
	IOSET0 = 0x00000080;

	S0SPCR = 0x08;  // SPI clk to be pclk/8
	S0SPCR = 0x30;  // master, msb, first clk edge, active high, no ints

}

void feed(void)
{
	PLLFEED=0xAA;
	PLLFEED=0x55;
}

void readAccDataFromFifo(void) {
	//ADXL362 Data Delimiters
	writeCharToSDBuffer(0xFF);
	writeCharToSDBuffer(0xFF);
	int numSamples = readNumSamplesFifo();
	
	select_ADXL362();
	SPI1_Write(XL362_FIFO_READ);
	//2 bytes per sample
	for (int i = 0; i < (numSamples); i++) {
		unsigned char dataOutLSB = SPI1_Read();
		unsigned char dataOutMSB = SPI1_Read();
		writeCharToSDBuffer(dataOutMSB);
		writeCharToSDBuffer(dataOutLSB);
		//rprintf("%x %x\n\r", dataOutMSB, dataOutLSB);	
		//FOR TESTING ONLY
		//Writes out the accelerometer data during each FIFO read
		//Write the direction:
		/*unsigned char axis = dataOutMSB & 0xC0;
		if (axis == 0x00) {
			rprintf("X: ");
		} else if (axis == 0x40) {
			rprintf("Y: ");
		} else if (axis == 0x80) {
			rprintf("Z: ");
		}
		//write the sign
		unsigned char sign = dataOutMSB & 0x30;
		unsigned short data = 0;
		unsigned char MSB = dataOutMSB & 0x0F;
		//positive
		if (sign == 0x00) {
			data = (unsigned short)(((unsigned short)MSB<<8) | dataOutLSB);
			rprintf("+ ");
		} else {
			data = (unsigned short)(((unsigned short)MSB<<8) | dataOutLSB);
			data = ((~data) & 0x0FFF) + 1;
			rprintf("- ");
		}
		//float dataf = (float)data/(float)4096;
		//rprintf("%f ", dataf);
		rprintf("%d\n\r", data);
		*/
		//rprintf("Accel: %x  %x\n\r", dataOutMSB, dataOutLSB);
		//rprintf("FIFO OUT: %d\n\r", dataOut);  //uncomment to see accel data as it comes from the FIFO
	}

	deselect_ADXL362();
	//End ADXL362 Delimiters
	writeCharToSDBuffer(0xFF);
	writeCharToSDBuffer(0x00);
	//rprintf("%d samples in the FIFO\n\r", numSamples);
	//rprintf("FIFO read complete!\n\r");
	//rprintf("ADC Index: %d\n\r", ADC_index);
}

void initialize_adc(void)
{
	rprintf("Initializing ADC\n\r");	
	enableIRQ();
	// Timer0  interrupt is an IRQ interrupt
	VICIntSelect &= ~0x00000010;
	// Enable Timer0 interrupt
	VICIntEnable |= 0x00000010;
	// Use slot 2 for Timer0 interrupt
	VICVectCntl2 = 0x24;
	// Set the address of ISR for slot 2
	VICVectAddr2 = (unsigned int)ADC_TIMER_ISR;
	rprintf("Interrupts configured\n\r");

	T0TCR = 0x02;	// Reset counter and prescaler
	T0MCR = 0x0003;	// On match reset the counter and generate interrupt
	//the match register value for the timer
	T0MR0 = 58982400 / ADC_FREQ;
	//need to check these timer settings
	//rprintf("Match value:%d", (58982400 / ADC_FREQ));
	T0PR = 0x00000000;  //"specifies the maximum value for the prescale counter"; not using prescale
	//enables the timer, do this last
	T0TCR = 0x00000001; // enables the timer
	stringSize = 512; //???
}

static void UART0ISR(void)
{
	char temp;
	writeShortToSDBuffer(U0RBR);
	temp = U0IIR; // Have to read this to clear the int
	VICVectAddr = 0;	
}

		
void writeShortToSDBuffer(unsigned short data)
{
	unsigned char mschar = 0;
	unsigned char lschar = 0;
	//bitshift instead
	unsigned short tempshort = (data & 0xFF00) >> 8;
	mschar = (unsigned char)tempshort;	
	lschar  = (unsigned char)(data & 0xFF);
	//rprintf("mschar: %d\n\r", mschar);  //uncomment to test this function
	//rprintf("lschar: %d\n\r", lschar);
	writeCharToSDBuffer(mschar);
	writeCharToSDBuffer(lschar);
}


void writeCharToSDBuffer(unsigned char data)
{
	if(RX_in < 512)
	{
		RX_array1[RX_in] = data;
		RX_in++;
		if(RX_in == 512) log_array1 = 1;
	}
	else if(RX_in >= 512)
	{
		RX_array2[RX_in-512] = data;
		RX_in++;
		if(RX_in == 1024)
		{
			log_array2 = 1;
			RX_in = 0;
		}
	}
	return;
}


void writeShortToADCBuffer(unsigned short data)
{
	unsigned char mschar = 0;
	unsigned char lschar = 0;
	//bitshift instead
	unsigned short tempshort = (data & 0xFF00) >> 8;
	mschar = (unsigned char)tempshort;	
	lschar  = (unsigned char)(data & 0xFF);
	writeCharToADCBuffer(mschar);
	writeCharToADCBuffer(lschar);
	return;
}


void writeCharToADCBuffer(unsigned char data)
{
	if(ADC_index < ADC_BUFFER_LENGTH)
	{
		ADC_array1[ADC_index] = data;
		ADC_index++;
		if(ADC_index == ADC_BUFFER_LENGTH) {
			adc_array1_full = 1;
			//rprintf("ADCARRAY1 FULL \n\r");
		}

	}
	else if(ADC_index >= ADC_BUFFER_LENGTH)
	{
		ADC_array2[ADC_index-ADC_BUFFER_LENGTH] = data;
		ADC_index++;
		if(ADC_index == 2*ADC_BUFFER_LENGTH)
		{
			adc_array2_full = 1;
			ADC_index = 0;
			//rprintf("ADCARRAY2 FULL \n\r");
		}
	}
	//rprintf("ADC_index: %d\n\r", ADC_index);
	return;
}

unsigned int writeFromADCToSDBuffer(void)
{
	//rprintf("Writing from adc to SD buffer\n\r");
	if(adc_array1_full == 1)
	{		
		//rprintf("adcarray1full\n\r");
		int j;
		for(j=0; j<ADC_BUFFER_LENGTH; j++){
			writeCharToSDBuffer(ADC_array1[j]);
		}
		adc_array1_full = 0;
		return TRUE;
	}
	if(adc_array2_full == 1)
	{		
		//rprintf("adcarray2full\n\r");
		int k;
		for(k=0; k<ADC_BUFFER_LENGTH; k++){
			writeCharToSDBuffer(ADC_array1[k]);
		}
		adc_array2_full = 0;
		return TRUE;
	}
	return FALSE;
}


//Divide into ISR for timer which sets up ADC read,
//and ADC ISR which reads the ADC value out (make sure to do this in the ISR!)
static void ADC_TIMER_ISR(void)
{

	T0IR = 1; // reset TMR0 interrupt
	//start an ADC reading
	//Make sure to do the read in the ISR!!!
	int adc_reg = 0;
	int adc_reading = 0;
	// Get AD1.2
	if(ADC_CHAN == 1)
	{
		AD1CR = 0x00020FF04; // AD1.2
		AD1CR |= 0x01000000; // start conversion
		//waits for the conversion to complete
		while((adc_reg & 0x80000000) == 0)
		{
			//register containing the ADC reading
			adc_reg = AD1DR;
		}
		//masks the adc reading out of the register
		adc_reg &= 0x0000FFC0;
		//shifts the reading over	
		adc_reading = adc_reg >> 6;
		//rprintf("ADC reading: %x\n\r", adc_reading);
		//rprintf("ADC reading: %d\n\r", adc_reading);
		AD1CR = 0x00000000;

		adc_oversampling_result += adc_reading;
		adc_oversampling_index++;

		if(adc_oversampling_index == OVERSAMPLING_AMOUNT/2) {
			deassertADXLConversionTrigger();
		}
		

		if(adc_oversampling_index == OVERSAMPLING_AMOUNT) {
			adc_oversampling_result = adc_oversampling_result >> OVERSAMPLE_SHIFT;
			adc_ovs = (unsigned short)adc_oversampling_result;
			writeShortToADCBuffer(adc_ovs);
			//adc_trigger_index++;
			assertADXLConversionTrigger();
			//rprintf("Hit ovs mark!\n\r");
			//rprintf("OVS Value: %d\n\r", adc_oversampling_result); 
			/*if(adc_trigger_index == (ADC_SAMPLES_PER_TRIG - 1)) {
				//rprintf("Triggering acc\n\r");
				assertADXLConversionTrigger();
				adc_trigger_index = 0;
			}*/
			adc_oversampling_result = 0;
			adc_oversampling_index = 0;
		}
	}
	//"Updates the priority hardware" (from user manual)
	VICVectAddr= 0;
}


void FIQ_Routine(void)
{
	char a;
	int j;
	stat(0,ON);
	for(j = 0; j < 5000000; j++);
	stat(0,OFF);
	a = U0RBR;
	a = U0IIR;  // have to read this to clear the interrupt
}

void SWI_Routine(void)
{
	while(1);
}

void UNDEF_Routine(void)
{
	stat(0,ON);
}

void setup_uart0(int newbaud, char want_ints)
{
	baud = newbaud;
	U0LCR = 0x83;   // 8 bits, no parity, 1 stop bit, DLAB = 1
	
	if(baud == 1200)
	{
		U0DLM = 0x0C;
		U0DLL = 0x00;
	}
	else if(baud == 2400)
	{
		U0DLM = 0x06;
		U0DLL = 0x00;
	}
	else if(baud == 4800)
	{
		U0DLM = 0x03;
		U0DLL = 0x00;
	}
	else if(baud == 9600)
	{
		U0DLM = 0x01;
		U0DLL = 0x80;
	}
	else if(baud == 19200)
	{
		U0DLM = 0x00;
		U0DLL = 0xC0;
	}
	else if(baud == 38400)
	{
		U0DLM = 0x00;
		U0DLL = 0x60;
	}
	else if(baud == 57600)
	{
		U0DLM = 0x00;
		U0DLL = 0x40;
	}
	else if(baud == 115200)
	{
		U0DLM = 0x00;
		U0DLL = 0x20;
	}

	U0FCR = 0x01;
	U0LCR = 0x03;   

	/*if(want_ints == 1)
	{
		enableIRQ();
		VICIntSelect &= ~0x00000040;
		VICIntEnable |= 0x00000040;
		VICVectCntl1 = 0x26;
		VICVectAddr1 = (unsigned int)UART0ISR;
		U0IER = 0x01;
	}*/
	/*else if(want_ints == 2)
	{
		enableIRQ();
		VICIntSelect &= ~0x00000040;
		VICIntEnable |= 0x00000040;
		VICVectCntl2 = 0x26;
		VICVectAddr2 = (unsigned int)UART0ISR_2;
		U0IER = 0X01;
	}
	if(want_ints == 0)
	//else if(want_ints == 0)
	{
		VICIntEnClr = 0x00000040;
		U0IER = 0x00;
	}*/
}

void stat(int statnum, int onoff)
{
	if(statnum) // Stat 1
	{
		if(onoff){ IOCLR0 = 0x00000800; } // On
		else { IOSET0 = 0x00000800; } // Off
	}
	else // Stat 0 
	{
		if(onoff){ IOCLR0 = 0x00000004; } // On
		else { IOSET0 = 0x00000004; } // Off
	}
}


void writeToSDCard(void)
{
	int j;
	if(log_array1 == 1)
	{
		//rprintf("Log array 1 being written\n\r");
		stat(0,ON);			
		if(fat16_write_file(handle,(unsigned char *)RX_array1, stringSize) < 0)
		{
			//blink lights forever in the case of an error
			while(1)
			{
				stat(0,ON);
				for(j = 0; j < 500000; j++)
				stat(0,OFF);
				stat(1,ON);
				for(j = 0; j < 500000; j++)
				stat(1,OFF);
			}
		}	
		//		
		sd_raw_sync();
		stat(0,OFF);
		log_array1 = 0;
		//rprintf("SD1\n\r");
	}

	if(log_array2 == 1)
	{
		stat(1,ON);
		//rprintf("Log array 2 being written\n\r");
		if(fat16_write_file(handle,(unsigned char *)RX_array2, stringSize) < 0)
		{
			while(1)
			{
				stat(0,ON);
				for(j = 0; j < 500000; j++)
				stat(0,OFF);
				stat(1,ON);
				for(j = 0; j < 500000; j++)
				stat(1,OFF);
			}
		}
		
		sd_raw_sync();
		stat(1,OFF);
		log_array2 = 0;
		//rprintf("SD2\n\r");
	}
}


void checkForButtonPress(void) {
	int j = 0;
	if((IOPIN0 & 0x00000008) == 0) // if button pushed, log file & quit
	{
			rprintf("Button press detected!\n\r");
			VICIntEnClr = 0xFFFFFFFF;

			if(RX_in < 512)
			{
				fat16_write_file(handle, (unsigned char *)RX_array1, RX_in);
				sd_raw_sync();
			}
			else if(RX_in >= 512)
			{
				fat16_write_file(handle, (unsigned char *)RX_array2, RX_in - 512);
				sd_raw_sync();
			}
			while(1)
			{
				stat(0,ON);
				for(j = 0; j < 500000; j++);
				stat(0,OFF);
				stat(1,ON);
				for(j = 0; j < 500000; j++);
				stat(1,OFF);
			}
	}
}

void test(void)
{

	rprintf("\n\rLogomatic V2 Test Code:\n\r");
	rprintf("ADC Test will begin in 5 seconds, hit stop button to terminate the test.\r\n\n");

	delay_ms(5000);

	while((IOPIN0 & 0x00000008) == 0x00000008)
	{
		// Get AD1.3
		AD1CR = 0x0020FF08;
		AD_conversion(1);

		// Get AD0.3
		AD0CR = 0x0020FF08;
		AD_conversion(0);
		
		// Get AD0.2
		AD0CR = 0x0020FF04;
		AD_conversion(0);

		// Get AD0.1
		AD0CR = 0x0020FF02;
		AD_conversion(0);

		// Get AD1.2
		AD1CR = 0x0020FF04;
		AD_conversion(1);
		
		// Get AD0.4
		AD0CR = 0x0020FF10;
		AD_conversion(0);

		// Get AD1.7
		AD1CR = 0x0020FF80;
		AD_conversion(1);

		// Get AD1.6
		AD1CR = 0x0020FF40;
		AD_conversion(1);

		delay_ms(1000);
		rprintf("\n\r");
	}

	rprintf("\n\rTest complete, locking up...\n\r");
	while(1);
		
}

void AD_conversion(int regbank)
{
	int temp = 0, temp2;

	if(!regbank) // bank 0
	{
		AD0CR |= 0x01000000; // start conversion
		while((temp & 0x80000000) == 0)
		{
			temp = AD0DR;
		}
		temp &= 0x0000FFC0;
		temp2 = temp / 0x00000040;

		AD0CR = 0x00000000;
	}
	else	    // bank 1
	{
		AD1CR |= 0x01000000; // start conversion
		while((temp & 0x80000000) == 0)
		{
			temp = AD1DR;
		}
		temp &= 0x0000FFC0;
		temp2 = temp / 0x00000040;

		AD1CR = 0x00000000;
	}

	rprintf("%d", temp2);
	rprintf("   ");
	
}

void fat_initialize(void)
{
	if(!sd_raw_init())
	{
		rprintf("SD Init Error\n\r");
		while(1);
	}

	if(openroot())
	{ 
		rprintf("SD OpenRoot Error\n\r");
	}
}

void delay_ms(int count)
{
	int i;
	count *= 10000;
	for(i = 0; i < count; i++)
		asm volatile ("nop");
}
