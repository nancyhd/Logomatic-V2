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

//Needed for main function calls
#include "main_msc.h"
#include "fat16.h"
#include "armVIC.h"
#include "itoa.h"
#include "rootdir.h"
#include "sd_raw.h"

 //test


/*******************************************************
 * 		     Global Variables
 ******************************************************/

#define ON	1
#define OFF	0

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

int ADC_BUFFER_LENGTH = 128;
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

signed int stringSize;
struct fat16_file_struct* handle;
struct fat16_file_struct * fd;
char stringBuf[256];

//Flags for interrupts (SPI and ADC)
bool ADC_READING_READY = false;
int OVERSAMPLING_AMOUNT = 256;  //number of ADC readings per recorded reading
int ADC_SAMPLES_PER_TRIG = 2;

int adc_trigger_index = 0;
int adc_oversampling_index = 0; //how many samples you've collected towards oversampling 
unsigned short adc_oversampling_result = 0;  //hold the ADC value while we're oversampling
int ADC_CHAN = 1;
int ADC_FREQ = 256000;

// Default Settings
static char mode = 2; // 0 = auto uart, 1 = trigger uart, 2 = adc
static char asc = 'N'; //ASCII.  N sets it to binary
static int baud = 9600;  //setting 4 in defaults file
static int freq = 100;   //ADC frequency setting; will overwrite this
static char trig = '$';
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
static void UART0ISR_2(void); //__attribute__ ((interrupt("IRQ")));
static void ADC_TIMER_ISR(void); //__attribute__ ((interrupt("IRQ")));


void writeShortToSDBuffer(unsigned short data);
void writeCharToSDBuffer(unsigned char data);
void writeShortToADCBuffer(unsigned char data);
void writeCharToADCBuffer(unsigned char data);
bool writeShortFromADCToSDBuffer(void);
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
	enableFIQ();
	
	//Pin config, SPI0 setup for SD card (clock, SPI settings)
	Initialize();
	
	//sets up SD card, sets SPI0 to 1MHz
	fat_initialize();		

	//9600 baud, no interrupts enabled
	setup_uart0(9600, 0);

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
	string_printf(name,"LOG%02d.txt",count);
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
		string_printf(name,"LOG%02d.txt",count);
	}
	
	handle = root_open_new(name);
		
	sd_raw_sync();	//write buffer to SD card

	initialize_adc();  
	
	//meaty big function loop!  
	while(1){
	/*  1. If an ADC buffer is full, read it to the SD buffer, then read out the accel FIFO
		3. If an SD buffer is full, write to the SD card
		4. Check for button press, close out if the button is pressed
		5. Go to sleep (not yet implemented)
		Button press, ADC timer, and ADC ready must all be set to wake processor from sleep
	*/

		//ACCEL trigger: 625Hz max, filter is still set by ODR though the ODR isn't used
		//500Hz accelerometer, 256KHz ADC oversampled to 1KHz.  
		//Every 512 ADC samples (oversample to 2ADC samples), trigger accelerometer
		//So you get 2 ADC samples for each accelerometer sample
		//at 64 ADC samples (128 bytes) (32 accel = 192 bytes) 320 total bytes; read into the buffers
		//have 2 128 byte ADC buffers to switch between for this
		//320 bytes about every 16th of a second => 5000 bytes per second => about 10 write cycles
		//so you have about 100ms per write cycle; 45.2ms required, so this should be OK!

		if(writeFromADCToSDBuffer()){
			readAccDataFromFifo();
		}
		writeToSDCard(); //writes if something is full
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
	
	PINSEL0 = 0xCF351505;
	PINSEL1 = 0x15441801;
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
	int numSamples = readNumSamplesFifo();
	select_ADXL362();
	SPI1_Write(XL362_FIFO_READ);
	//2 bytes per sample
	for (int i = 0; i < 2*numSamples; i++) {
		writeCharToSDBuffer(SPI1_read());
	}
	deselect_ADXL362();
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

	T0TCR = 0x00000002;	// Reset counter and prescaler
	T0MCR = 0x00000003;	// On match reset the counter and generate interrupt
	T0MR0 = 58982400 / ADC_FREQ;

	T0PR = 0x00000000;

	T0TCR = 0x00000001; // enable timer

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
	return 0;
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
}


void writeCharToADCBuffer(unsigned char data)
{
	if(ADC_index < ADC_BUFFER_LENGTH)
	{
		ADC_array1[ADC_index] = data;
		ADC_index++;
		if(ADC_index == ADC_BUFFER_LENGTH) log_array1 = 1;
	}
	else if(ADC_index >= ADC_BUFFER_LENGTH)
	{
		ADC_array2[ADC_index-ADC_BUFFER_LENGTH] = data;
		ADC_index++;
		if(ADC_index == 2*ADC_BUFFER_LENGTH)
		{
			log_array2 = 1;
			ADC_index = 0;
		}
	}
	return 0;
}

bool writeShortFromADCToSDBuffer(void)
{
	int j;
	if(adc_array1_full == 1)
	{		
		for(j=0; j<ADC_BUFFER_LENGTH; j++){
			writeCharToADCBuffer(ADC_array1[j]);
		}
		log_array1_full = 0;
		return true;
	}
	if(adc_array2_full == 1)
	{		
		for(j=0; j<ADC_BUFFER_LENGTH; j++){
			writeCharToADCBuffer(ADC_array1[j]);
		}
		log_array2_full = 0;
		return true;
	}
	return false;
}


//Divide into ISR for timer which sets up ADC read,
//and ADC ISR which reads the ADC value out (make sure to do this in the ISR!)
static void ADC_TIMER_ISR(void)
{

	T0IR = 1; // reset TMR0 interrupt
	//start an ADC reading
	//set to trigger ADC ISR eventually, BUT make sure to do the read in the ISR!!!
	int temp = 0;
	int adc_reading = 0;
	// Get AD1.2
	if(ADC_CHAN == 1)
	{
		AD1CR = 0x00020FF04; // AD1.2
		AD1CR |= 0x01000000; // start conversion
		//waits for the conversion to complete
		while((temp & 0x80000000) == 0)
		{
			//temp is the register containing the ADC reading
			temp = AD1DR;
		}
		//masks the adc reading out of the register
		temp &= 0x0000FFC0;
		//shifts the reading over	
		//adc_reading = temp / 0x00000040;
		//10 bit reading altogether
		adc_reading = temp >> 7;

		AD1CR = 0x00000000;

		adc_oversampling_result += adc_reading;
		adc_oversampling_index++;
		adc_trigger_index++;

		if(adc_oversampling_index == OVERSAMPLING_AMOUNT) {
			writeShortToADCBuffer(adc_oversampling_result);
		}

		if(adc_trigger_index == ADC_SAMPLES_PER_TRIG) {
			triggerADXLConversion();
		}

		//write int to data buffer
		//make sure short is apprioriate in this case
		writeShortToSDBuffer((unsigned short)adc_reading);

	}
	//what does this do?  Jumps to this register value = back to beginning of the loop?
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

	if(want_ints == 1)
	{
		enableIRQ();
		VICIntSelect &= ~0x00000040;
		VICIntEnable |= 0x00000040;
		VICVectCntl1 = 0x26;
		VICVectAddr1 = (unsigned int)UART0ISR;
		U0IER = 0x01;
	}
	else if(want_ints == 2)
	{
		enableIRQ();
		VICIntSelect &= ~0x00000040;
		VICIntEnable |= 0x00000040;
		VICVectCntl2 = 0x26;
		VICVectAddr2 = (unsigned int)UART0ISR_2;
		U0IER = 0X01;
	}
	else if(want_ints == 0)
	{
		VICIntEnClr = 0x00000040;
		U0IER = 0x00;
	}
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


void mode_0(void) // Auto UART mode
{
	rprintf("MODE 0\n\r");
	setup_uart0(baud,1);
	stringSize = 512;
	//mode_action();
	//rprintf("Exit mode 0\n\r");

}

void mode_1(void)
{
	rprintf("MODE 1\n\r");	

	setup_uart0(baud,2);
	stringSize = frame + 2;

	//mode_action();
}

void writeToSDCard(void)
{
	int j;
	
	if(log_array1 == 1)
	{
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
	}

	if(log_array2 == 1)
	{
		stat(1,ON);
		
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
	}

}


void checkForButtonPress(void) {
	if((IOPIN0 & 0x00000008) == 0) // if button pushed, log file & quit
	{
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
