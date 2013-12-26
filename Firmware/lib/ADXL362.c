/******************************************************************************
*   SPI1.c - SPI driver for use on the LPC214x Family Microcontrollers
*
*  
*
*   History
*   12/22/13 - NHD - First draft.
******************************************************************************/
/*Includes*/
#include "LPC214x.h"
#include "SPI1.h"
#include "xl362.h"


/*Defines*/
#define EINT_SELECT			2
#define P0_20_SHIFT			20 /*P0.20 is wired to the ADXL chip select*/
#define P0_30_SHIFT			30 /*P0.30 is the interrupt pin; EINT3*/


/*Macros*/
//Processor Interrupt Configuration
#define configure_pin_ss() 			IODIR0 |= (1<<P0_20_SHIFT) /*Sets the CS pin, P0.20, to output*/
#define configure_pin_irq() 		PINSEL1 |= ((EINT_SELECT) << (P0_30_SHIFT)) /*sets P0.30 to EINT3*/
#define configure_irq_direction()	EXTMODE |= 0x04; EXTPOLAR |= 0x04  /*sets the interrupt to edge sensitive, rising edge*/
#define configure_int_wakeup()		INTWAKE |= (1<<3) /*so EINT3 can wake the processor from sleep*/
#define configure_VIC_int()			VICIntEnable |= (1<<17) /*17 is the EINT3 wakeup*/
//clear interrupt: write 1 to EXTINT, clear VicIntEnClr

//Accelerometer configuration
#define configure_fifo_en()			XL362_FIFO_CONTROL |= (XL362_FIFO_MODE_STREAM | XL362_FIFO_SAMPLES_AH)
#define configure_int1_watermark()	XL362_INTMAP1 |= XL362_INT_FIFO_WATERMARK
#define configure_sampling()		XL362_FILTER_CTL |= XL362_RATE_400  /*2G is default*/
#define configure_noise()			XL362_POWER_CTL |= XL362_LOW_NOISE2
#define enable_meas()				XL362_POWER_CTL |= XL362_MEASURE_3D
#define configure_ext_clk()			XL362_POWER_CTL |= XL362_EXT_CLK
#define configure_trig_sampling()	XL362_FILTER_CTL |= XL362_EXT_TRIGGER
#define configure_other_ints()		XL362_THRESH_ACT_H |= 0x04; XL362_TIME_ACT |= 0xFF

//SPI CS macros
#define select_ADXL362() 		IOCLR0 |= (1<<P0_20_SHIFT)
#define unselect_ADXL362() 		IOSET0 |= (1<<P0_20_SHIFT)


void ADXL362_Init(void){
	configure_pin_ss();
	configure_pin_irq();
	configure_irq_direction();
	configure_int_wakeup();
	configure_VIC_int();
	configure_fifo_en(); /*enables in stream mode with 384 sample deep FIFO (128 in 3 axis)*/
	configure_fifo_samples();
	configure_int1_watermark();
	configure_sampling();
	configure_noise();
	configure_other_ints(); //sets activity, motion thresholds high so they don't trigger as often; ints ignored
	enable_meas();
	//configure_trig_sampling();	//wire trigger to int2

}

bool ADXLDeviceIDCheck() {
	SPI1_Write(XL362_REG_READ);
	SPI1_Write(XL362_DEVID_AD);
	unsigned char deviceID = SPI1_Read();
	return deviceID == 0xAD;
}



int readNumSamplesFifo(){

	SPI1_Write(XL362_REG_READ);
	SPI1_Write(XL362_FIFO_ENTRIES_L);
	unsigned char fifo_num_l = SPI1_Read();
	unsigned char fifo_num_h = SPI1_Read() & 0x03; /*only the last 2 bits of the upper register matter*/

	int fifo_num_samples = (int)(((int)fifo_num_h<<8) | fifo_num_l);
	return fifo_num_samples;
}



