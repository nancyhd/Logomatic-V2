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

//Needed for main function calls
#include "main_msc.h"
#include "fat16.h"
#include "armVIC.h"
#include "itoa.h"
#include "rootdir.h"
#include "sd_raw.h"




		if(asc == 'Y')
		{
			itoa(temp2, 10, temp_buff);
			if(temp_buff[0] >= 48 && temp_buff[0] <= 57)
			{
				q[ind] = temp_buff[0];
				ind++;
			}
			if(temp_buff[1] >= 48 && temp_buff[1] <= 57)
			{
				q[ind] = temp_buff[1];
				ind++;
			}
			if(temp_buff[2] >= 48 && temp_buff[2] <= 57)
			{
				q[ind] = temp_buff[2];
				ind++;
			}
			if(temp_buff[3] >= 48 && temp_buff[3] <= 57)
			{
				q[ind] = temp_buff[3];
				ind++;
			}

			q[ind] = 0;
			ind++;
			temp = 0; 
			temp2 = 0;
			temp_buff[0] = 0;
			temp_buff[1] = 0;
			temp_buff[2] = 0;
			temp_buff[3] = 0;

		}
		else if(asc == 'N')
		{
			a = ((short)temp2 & 0xFF00) / 0x00000100;
			q[ind] = (char)a;
			
			q[ind+1]  = (char)temp2 & 0xFF;
			ind += 2;
			temp = 0;
		}

		


void Log_init(void)
{
	int x, mark = 0, ind = 0;
	char temp, temp2 = 0, safety = 0;
	//signed char handle;

	if(root_file_exists("LOGCON.txt"))
	{
		//rprintf("\n\rFound LOGcon.txt\n");
		fd = root_open("LOGCON.txt");
		stringSize = fat16_read_file(fd, (unsigned char *)stringBuf, 512);
		stringBuf[stringSize] = '\0';
		fat16_close_file(fd);
	}
	else
	{
		//rprintf("Couldn't find LOGcon.txt, creating...\n");
		fd = root_open_new("LOGCON.txt");
		if(fd == NULL)
		{
		 	rprintf("Error creating LOGCON.txt, locking up...\n\r");
		 	while(1)
			{
				stat(0,ON);
				delay_ms(50);
				stat(0,OFF);
				stat(1,ON);
				delay_ms(50);
				stat(1,OFF);
			}
		}

		strcpy(stringBuf, "MODE = 2\r\nASCII = N\r\nBaud = 4\r\nFrequency = 100\r\nTrigger Character = $\r\nText Frame = 100\r\nAD1.3 = N\r\nAD0.3 = N\r\nAD0.2 = N\r\nAD0.1 = N\r\nAD1.2 = N\r\nAD0.4 = N\r\nAD1.7 = N\r\nAD1.6 = N\r\nSafety On = N\r\n");
		stringSize = strlen(stringBuf);
		fat16_write_file(fd, (unsigned char*)stringBuf, stringSize);
		sd_raw_sync();
	}

	for(x = 0; x < stringSize; x++)
	{
		temp = stringBuf[x];
		if(temp == 10)
		{
			mark = x;
			ind++;
			if(ind == 1)
			{
				mode = stringBuf[mark-2]-48; // 0 = auto uart, 1 = trigger uart, 2 = adc
				rprintf("mode = %d\n\r",mode);
			}
			else if(ind == 2)
			{
				asc = stringBuf[mark-2]; // default is 'N'
				rprintf("asc = %c\n\r",asc);
			}
			else if(ind == 3)
			{
				if(stringBuf[mark-2] == '1'){ baud = 1200; }
				else if(stringBuf[mark-2] == '2'){ baud = 2400; }
				else if(stringBuf[mark-2] == '3'){ baud = 4800; }
				else if(stringBuf[mark-2] == '4'){ baud = 9600; }
				else if(stringBuf[mark-2] == '5'){ baud = 19200; }
				else if(stringBuf[mark-2] == '6'){ baud = 38400; }
				else if(stringBuf[mark-2] == '7'){ baud = 57600; }
				else if(stringBuf[mark-2] == '8'){ baud = 115200; }

				rprintf("baud = %d\n\r",baud);
			}
			else if(ind == 4)
			{
				freq = (stringBuf[mark-2]-48) + (stringBuf[mark-3]-48) * 10;
				if((stringBuf[mark-4] >= 48) && (stringBuf[mark-4] < 58))
				{
					freq+= (stringBuf[mark-4]-48) * 100;
					if((stringBuf[mark-5] >= 48) && (stringBuf[mark-5] < 58)){ freq += (stringBuf[mark-5]-48)*1000; }
				}
				rprintf("freq = %d\n\r",freq);
			}
			else if(ind == 5)
			{
				trig = stringBuf[mark-2]; // default is $
				
				rprintf("trig = %c\n\r",trig);
			}
			else if(ind == 6)
			{
				frame = (stringBuf[mark-2]-48) + (stringBuf[mark-3]-48) * 10 + (stringBuf[mark-4]-48)*100;
				if(frame > 510){ frame = 510; } // up to 510 characters
				rprintf("frame = %d\n\r",frame);
			}
			else if(ind == 7)
			{
				ad1_3 = stringBuf[mark-2]; // default is 'N'
				if(ad1_3 == 'Y'){ temp2++; }
				rprintf("ad1_3 = %c\n\r",ad1_3);
			}
			else if(ind == 8)
			{
				ad0_3 = stringBuf[mark-2]; // default is 'N'
				if(ad0_3 == 'Y'){ temp2++; }
				rprintf("ad0_3 = %c\n\r",ad0_3);
			}
			else if(ind == 9)
			{
				ad0_2 = stringBuf[mark-2]; // default is 'N'
				if(ad0_2 == 'Y'){ temp2++; }
				rprintf("ad0_2 = %c\n\r",ad0_2);
			}
			else if(ind == 10)
			{
				ad0_1 = stringBuf[mark-2]; // default is 'N'
				if(ad0_1 == 'Y'){ temp2++; }
				rprintf("ad0_1 = %c\n\r",ad0_1);
			}
			else if(ind == 11)
			{
				ad1_2 = stringBuf[mark-2]; // default is 'N'
				if(ad1_2 == 'Y'){ temp2++; }
				rprintf("ad1_2 = %c\n\r",ad1_2);
			}
			else if(ind == 12)
			{
				ad0_4 = stringBuf[mark-2]; // default is 'N'
				if(ad0_4 == 'Y'){ temp2++; }
				rprintf("ad0_4 = %c\n\r",ad0_4);
			}
			else if(ind == 13)
			{
				ad1_7 = stringBuf[mark-2]; // default is 'N'
				if(ad1_7 == 'Y'){ temp2++; }
				rprintf("ad1_7 = %c\n\r",ad1_7);
			}
			else if(ind == 14)
			{
				ad1_6 = stringBuf[mark-2]; // default is 'N'
				if(ad1_6 == 'Y'){ temp2++; }
				rprintf("ad1_6 = %c\n\r",ad1_6);
			}
			else if(ind == 15)
			{
				safety = stringBuf[mark-2]; // default is 'Y'
				rprintf("safety = %c\n\r",safety);
			}
		}
	}

	if(safety == 'Y')
	{
		if((temp2 ==10) && (freq > 150)){ freq = 150; }
		else if((temp2 == 9) && (freq > 166)){ freq = 166; }
		else if((temp2 == 8) && (freq > 187)){ freq = 187; }
		else if((temp2 == 7) && (freq > 214)){ freq = 214; }
		else if((temp2 == 6) && (freq > 250)){ freq = 250; }
		else if((temp2 == 5) && (freq > 300)){ freq = 300; }
		else if((temp2 == 4) && (freq > 375)){ freq = 375; }
		else if((temp2 == 3) && (freq > 500)){ freq = 500; }
		else if((temp2 == 2) && (freq > 750)){ freq = 750; }
		else if((temp2 == 1) && (freq > 1500)){ freq = 1500; }
		else if((temp2 == 0)){ freq = 100; }
	}
	
	if(safety == 'T'){ test(); }

}