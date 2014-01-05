#ifndef __ADXL362_H 
#define __ADXL362_H

void ADXL362_Init(void);
void readAccDataFromFifo(void);
unsigned char ConfigureAcc(unsigned char reg, unsigned char value);
unsigned char ReadAcc(unsigned char reg);
void WriteAcc(unsigned char reg, unsigned char value);
bool ADXLDeviceIDCheck(void);
int readNumSamplesFifo(void);




#endif
