/******************************************************************************
*   SPI1.c - SPI driver for use on the LPC214x Family Microcontrollers
*
*   History
*   12/22/13 - NHD - First draft.
******************************************************************************/
/*Includes*/
#include "LPC214x.h"
#include "SPI1.h"
#include "xl362.h"
#include "ADXL362.h"
#include "type.h"


/*Defines*/
#define EINT_SELECT			2
#define P0_20_SHIFT			20 /*P0.20 is wired to the ADXL chip select*/
#define P0_30_SHIFT			30 /*P0.30 is the interrupt pin; EINT3*/


/*
reference:
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

*/

/*Macros*/
//Processor Interrupt Configuration for FIFO watermark (not used, or tested)
#define configure_pin_ss() 			IODIR0       |= (1<<P0_20_SHIFT) /*Sets the CS pin, P0.20, to output*/
#define configure_pin_irq() 		PINSEL0      |= ((EINT_SELECT) << (P0_30_SHIFT)) /*sets P0.30 to EINT3*/
#define configure_irq_direction()	EXTMODE      |= 0x04; EXTPOLAR |= 0x04  /*sets the interrupt to edge sensitive, rising edge*/
#define configure_int_wakeup()		INTWAKE      |= (1<<3); /*so EINT3 can wake the processor from sleep*/
#define configure_VIC_int()			VICIntEnable |= (1<<17); /*17 is the EINT3 wakeup*/
//clear interrupt: write 1 to EXTINT, clear VicIntEnClr


// pin
#define configure_trigger_pin()		    IODIR0 |= (1<<12); /*P0.12 (P8 on the board) is the trigger pin*/
#define set_trigger()					IOSET0 |= (1<<12);
#define clr_trigger()					IOCLR0 |= (1<<12);
#define configure_cs()					IODIR0 |= (1<<P0_20_SHIFT);
#define select_acc()					IOCLR0 |= (1<<P0_20_SHIFT);
#define deselect_acc()					IOSET0 |= (1<<P0_20_SHIFT);

void assertADXLConversionTrigger(void) {
	set_trigger();
	/*rprintf("PINSEL0:%x\n\r", PINSEL0);
	rprintf("IOPIN0:%x\n\r", IOPIN0);
	rprintf("IOSET0:%x\n\r", IOSET0);
	rprintf("IODIR0:%x\n\r", IODIR0);
	while(1) {}*/
	//rprintf("trigger set\n\r");
}

void deassertADXLConversionTrigger(void){
	clr_trigger();
}

void select_ADXL362(void){
	select_acc();
}

void deselect_ADXL362(void){
	deselect_acc();
}

void ADXL362_Init(void){
	/*configure processor for the XL interrupt*/
	/*not currently being used, going off counters in main loop instead
	configure_pin_ss();
	configure_pin_irq();
	configure_irq_direction();
	configure_int_wakeup();
	configure_VIC_int();
	*/	
	configure_trigger_pin();

	configure_cs();	
	deselect_acc();
	rprintf("Pinsel1: %x\n\r", PINSEL1);

	/*configure accelerometer*/

	//enables the FIFO in stream mode, 384 sample deep watermark (128 in 3 axes)
	ConfigureAcc(XL362_FIFO_CTL, (XL362_FIFO_MODE_STREAM | XL362_FIFO_SAMPLES_AH));
	//set int1 to a watermark interrupt (watermark configured in FILTER_CONTROL; 128samples of each axis)
	ConfigureAcc(XL362_INTMAP1, XL362_INT_FIFO_WATERMARK);

	//set the sampling rate to 100Hz ****so the filter is set to 50Hz*****  Does not actually impact sampling rate
	//since it is externally triggered
	ConfigureAcc(XL362_FILTER_CTL, XL362_RATE_100);
	//set into low noise mode
	ConfigureAcc(XL362_POWER_CTL, XL362_LOW_NOISE2);
	//Configure activity interrups to >0 so if enabled, they don't trigger constantly (still not enabled, though)
	ConfigureAcc(XL362_THRESH_ACTH, 0x04);
	ConfigureAcc(XL362_TIME_ACT, 0xFF);
	//Configure external trigger sampling
	ConfigureAcc(XL362_FILTER_CTL, XL362_EXT_TRIGGER);

	//Do this last: enable the accelerometer into measurement mode
	ConfigureAcc(XL362_POWER_CTL, XL362_MEASURE_3D);

	//Clear out all the existing samples in the FIFO
	int samplesInFifo = readNumSamplesFifo();

	select_ADXL362();
	SPI1_Write(XL362_FIFO_READ);
	//2 bytes per sample
	for (int i = 0; i < 2*samplesInFifo; i++) {
		SPI1_Read();
		//rprintf("Accel: %x  %x\n\r", dataOutMSB, dataOutLSB);
		//rprintf("FIFO OUT: %d\n\r", dataOut);  //uncomment to see accel data as it comes from the FIFO
	}
	rprintf("%d samples in the FIFO\n\r", samplesInFifo);
	deselect_ADXL362();

}


//will only set low register values high; may need to add functionality later to set values to 0
unsigned char ConfigureAcc(unsigned char reg, unsigned char value) {
	unsigned char current_reg_val = ReadAcc(reg);
	unsigned char new_val = current_reg_val |= value;
	WriteAcc(reg, new_val);
	rprintf("AccConfig: ");
	rprintf("Reg %x, Val: %x\n\r", reg, ReadAcc(reg));
	return new_val;
}

unsigned char ReadAcc(unsigned char reg) {
	select_acc();
	SPI1_Write(XL362_REG_READ);
	SPI1_Write(reg);
	unsigned char read_val = SPI1_Read();
	deselect_acc();
	return read_val;
}

void WriteAcc(unsigned char reg, unsigned char value) {
	select_acc();
	SPI1_Write(XL362_REG_WRITE);
	SPI1_Write(reg);
	SPI1_Write(value);
	deselect_acc();
	return;
}

unsigned int ADXLDeviceIDCheck(void) {
	unsigned char id = ReadAcc(XL362_DEVID_AD);
	return (id == 0xAD);
}

int readNumSamplesFifo(void){
	unsigned char fifo_num_l = ReadAcc(XL362_FIFO_ENTRIES_L);
	unsigned char fifo_num_h = ReadAcc(XL362_FIFO_ENTRIES_H);
	fifo_num_h &= 0x03; /*only the last 2 bits of the upper register matter*/
	int fifo_num_samples = (int)(((int)fifo_num_h<<8) | fifo_num_l);
	return fifo_num_samples;
}




