#ifndef __ADXL362_H 
#define __ADXL362_H

void select_ADXL362(void);
void deselect_ADXL362(void);

void ADXL362_Init(void);
unsigned char ConfigureAcc(unsigned char reg, unsigned char value);
unsigned char ReadAcc(unsigned char reg);
void WriteAcc(unsigned char reg, unsigned char value);
unsigned int ADXLDeviceIDCheck(void);
int readNumSamplesFifo(void);
void assertADXLConversionTrigger(void);
void deassertADXLConversionTrigger(void);

#endif
