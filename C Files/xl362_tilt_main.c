/*----------------------------------------------------------------------
  File Name          : xl362_tilt_main.c
  Author             : MPD Application Team
  Description        : Main program to make the activity demo on ADXL362
  File ID            : $Id: xl362_tilt_main.c,v 1.2 2012/05/02 19:43:54 jlee11 Exp $

  Analog Devices ADXL 362 digital output accellerometer 
  with advanced digital features.
  (c) 2011 Analog Devices application support team.  
  ----------------------------------------------------------------------
  The present firmware which is for guidance only aims at providing
  customers with coding information regarding their products in order
  for them to save time.  As a result, Analog Devices shall not be
  held liable for any direct, indirect or consequential damages with
  respect to any claims arising from the content of such firmware and/or
  the use made by customers of the coding information contained herein
  in connection with their products.
----------------------------------------------------------------------*/
#include <htc.h>
#include "XL362.h"
#include "xl362_io.h"
__CONFIG(FOSC_HS & WDTE_OFF & PWRTE_ON & CP_OFF);

void i2cinit(void);
unsigned char buffer[8]; /* a buffer to use with the read and write functions */
void main(){
  OPTION_REG = 0x40 /*INTEDG*/;

 /* power up timer */
 buffer[7] = 255;
 while(buffer[7]--); 
 /* soft reset for safety */
  buffer[0] = XL362_SOFT_RESET_KEY;
  xl362Write(1,XL362_SOFT_RESET,buffer);
 /* wait for soft reset to pass */
 buffer[7] = 255;
 while(buffer[7]--); 

  /* set up a buffer with all the initialization for activity and inactivity */
  buffer[0] = 105;  /* XL362_THRESH_ACTL about 15 degrees*/
  buffer[1] = 0  ;  /* THRESH_ACTH */
  buffer[2] = 3  ;  /* TIME_ACT */
  buffer[3] = 105;  /* THRESH_INACTL*/
  buffer[4] = 0  ;  /* THRESH_INACTH */
  buffer[5] = 37 ;  /* TIME_INACTL 3 seconds at 12.5 hz*/
  buffer[6] = 0  ;  /* TIME_INACTH */
  buffer[7] =     /* ACT_INACT_CTL */
               XL362_ACT_ENABLE | XL362_ACT_AC | XL362_INACT_ENABLE
    | XL362_INACT_AC | XL362_ACT_INACT_LINK | XL362_ACT_INACT_LOOP;
  xl362Write(8,XL362_THRESH_ACTL,buffer);

  /* set up a buffer with all the initization for intmaps filter and power*/
  buffer[0] = 0 ; /* INTMAP1 */
  buffer[1] = XL362_INT_AWAKE ; /* INTMAP2 */
  buffer[2] =     /* FILTER_CTL */
    XL362_RATE_12_5 |  XL362_RANGE_2G;
  buffer[3] =     /* POWER_CTL */
    XL362_MEASURE_3D |  XL362_LOW_POWER;
  xl362Write(4,XL362_INTMAP1,buffer);

  while(1){ /* we only resume here after a wakeup interupt */
    asm("sleep"); /* go into low power mode */
  } /* while */
} /* main */

/* interupt service routine */ 
void interrupt myisr(void){
  INTF=0; // clear the interupt
}
