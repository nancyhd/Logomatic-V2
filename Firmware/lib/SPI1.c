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

/*Defines*/
#define SSP_SELECT		2
#define GPIO_SELECT		0
#define P0_17_SHIFT		2
#define P0_18_SHIFT 	4
#define P0_19_SHIFT 	6

//SSPCR0 register settings
#define SPI_8_BIT_FRAME 		7 
#define SPI_FRAME_TYPE_OFFSET 	4
#define SPI_FRAME_TYPE 			(0 << SPI_FRAME_TYPE_OFFSET)
#define SPI_POLARITY_OFFSET		6
#define SPI_POLARITY 			(0 << SPI_POLARITY_OFFSET)
#define SPI_PHASE_OFFSET		7
#define SPI_PHASE 				(0 << SPI_PHASE_OFFSET)
#define SPI_PCLK_DIVIDER_OFFSET 8
#define SPI_PCLK_DIVIDER 		(29 << SPI_PCLK_DIVIDER_OFFSET)

//SSPCR1 register settings
#define SPI_ENABLE_OFFSET	 	1
#define SPI_ENABLE 				(1 << SPI_ENABLE_OFFSET)

//SSPSR register fields
#define SPI_BUSY				(1 << 4)

/*Macros*/
#define configure_pin_mosi() 	PINSEL1 |= ((SSP_SELECT) << (P0_19_SHIFT))
#define configure_pin_sck() 	PINSEL1 |= ((SSP_SELECT) << (P0_17_SHIFT))
#define configure_pin_miso() 	PINSEL1 |= ((SSP_SELECT) << (P0_18_SHIFT))



void SPI1_Init(void)
{
	//Set configuration of peripheral
	SSPCPSR = 2;//min value, caution SPI clock = PCLK/(SSPCPSR * (SPI_PCLK_DIVIDER + 1))
	SSPCR0 = SPI_8_BIT_FRAME | SPI_FRAME_TYPE | SPI_PCLK_DIVIDER | SPI_PHASE | SPI_POLARITY;
	SSPCR1 = 0;

	//turn on the pins
	configure_pin_miso();
	configure_pin_sck();
	configure_pin_mosi();

	//fire it up
	SSPCR1 |= SPI_ENABLE;
}

unsigned char SPI1_Write(unsigned char data)
{
	SSPDR = (unsigned long)data;
	while (SSPSR & SPI_BUSY);//wait for the send to complete
	//I am assuming RNE is set, because we just clocked data out
	return (unsigned char)(SSPDR & 0x000000ff);
}

unsigned char SPI1_Read(void)
{
	return SPI1_Write(0x00);
}
