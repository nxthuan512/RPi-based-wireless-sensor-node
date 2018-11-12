#include <stdio.h>
#include <string.h>
#include "utils.h"


// ========================================================
//
// Write binary file to array
//
// ========================================================
long writeBinaryFileToArray(char frame_name[], unsigned char *frame_data)
{
	FILE *fp; 
	long frame_data_size;
	long result;
  
	fp = fopen(frame_name, "rb");
	frame_data_size = 0;
	//
	if (fp == NULL)
	{
		// printf ("Error: Open %s file FAILED\n", frame_name);
	}
	else
	{
		// Obtain file size
		fseek (fp, 0, SEEK_END);
		frame_data_size = ftell(fp);
		rewind(fp);

		// Copy the file to frame_data array
		result = fread (frame_data, sizeof(unsigned char), frame_data_size, fp);
		if (result != frame_data_size)
		{
			printf ("Error: Reading %s file FAILED\n", frame_name);
		}
		else
		{
			// printf ("Info: --- --- SUCCEEDED\n");
		}  
		fclose(fp);
	}
	return frame_data_size;  
}

// ========================================================
//
// Write array to binary file
//
// ========================================================
void writeArrayToBinaryFile (char frame_name[], unsigned char *frame_data, long frame_data_size)
{
	FILE *fp;

	fp = fopen(frame_name, "wb");
	//
	if (fp == NULL)
	{
		printf ("Error: Open %s file FAILED\n", frame_name);
	}
	else
	{
		// Copy the frame_data array to file
		fwrite (frame_data, sizeof(unsigned char), frame_data_size, fp);
#ifdef DEBUG
		//printf ("Info: Write array to binary file %s ... SUCCEEDED\n", frame_name);
#endif
		fclose(fp);
	}
}


// ========================================================
//
// Convert integer to string
//
// ========================================================
char* int2str (int value, char str[], char base)
{  
	char const digit[] = "0123456789ABCDEF";
	char *p = str;
	int shifter = value;

	//
	if (value < 0)
	{
		*p++ = '-';
		value *= -1;
	}

	//
	do 
	{
		++p;
		shifter = shifter / base;
	} while (shifter);

	//
	*p = '\0';
	do
	{
		*--p = digit[value % base];
		value = value / base;
	} while (value);

	return str;
}
