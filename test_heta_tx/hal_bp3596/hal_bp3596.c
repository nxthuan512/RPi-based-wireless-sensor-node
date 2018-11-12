/*
 * hal_bp3596.c
 *
 *  Created on: Feb 21, 2016
 *      Author: nxthuan512
 */

#ifndef HAL_BP3596_HAL_BP3596_C_
#define HAL_BP3596_HAL_BP3596_C_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "hal_bp3596.h"
#include "../hal/hal_config_wiringpi.h"


// ***********************************************************
//
// Enable RF power
//
// ***********************************************************
void hal_bp3596_power_en(uint8_t enable)
{
	// Initialize pins
	hal_GPIOOutputPin(BP3596_EN);

	// Power BP3596
	if (enable == 1)
	{
		hal_GPIOSetPin(BP3596_EN);
	}
	// No power
	else
	{
		hal_GPIOClearPin(BP3596_EN);
	}
}


// ***********************************************************
//
// Initialize SPI channel 1 and all pins 
// (TRX_RST, SLP_TR, IRQ, DIG2) of RF module
//
// ***********************************************************
void hal_bp3596_init()
{
	// Start wiringPi
	printf("Info: --- --- Start wiringPi ... \n");
	if (LibSetup() == -1)
	{
		printf ("Info: --- --- FAILED\n");
		exit(0);
	}
	else
	{
		printf("Info: --- --- SUCCEEDED\n");
		// Initialize SPI in master mode to access the transceiver
		printf("Info: --- --- Initialize SPI channel 1, clock speed 6.4 MHz ... \n");
		if (hal_SPI1Setup(6400000) == -1)
		{
			printf ("Info: --- --- FAILED\n");
			exit(0);
		}
		else
			printf("Info: --- --- SUCCEEDED\n");
	}
	
	// Initialize RST, SLPTR as GPIO output, DIG2 as GPIO input
	hal_GPIOOutputPin(BP3596_RST);
	
	// Initialize EXT_INT as interrupt for transceiver
	hal_GPIOInputPin(BP3596_IRQ);
	/*
	printf("Info: --- --- Initialize IRQ pins ... \n");
	if (hal_GPIOISRRisingEdge(AT86RF212_IRQ, &hal_trx_rf212_irq) < 0)
	{
		printf ("Info: --- --- FAILED: Cannot setup ISR\n") ;
		exit(0);
	}
	else
		printf("Info: --- --- SUCCEEDED\n");
	*/
}


// ***********************************************************
//
// Write data into a register
//
// ***********************************************************
void hal_bp3596_reg_write (unsigned char reg_addr, unsigned char reg_data)
{
	unsigned char dummy_data[2];
	
	// Prepare the command byte
	dummy_data[0] = BP3596_WAC | (reg_addr << 1);
	
	// Do dummy read for initiating SPI read
	dummy_data[1] = reg_data;	
	
	// Send command, dummy_data[0] is PHY status	
	hal_SPI1DataRW (dummy_data, 2);
}


// ***********************************************************
//
// Read current value from a register
//
// ***********************************************************
unsigned char hal_bp3596_reg_read (unsigned char reg_addr)
{
	unsigned char dummy_data[2];
	
	// Saving the current interrupt status & disabling the global interrupt
	// ENTER_CRITICAL_REGION();
	
	// Prepare the command byte
	dummy_data[0] = BP3596_RAC | (reg_addr << 1);
	
	// Do dummy read for initiating SPI read
	dummy_data[1] = 0xFF;	
	
	// Send command, dummy_data[0] is PHY status	
	hal_SPI1DataRW (dummy_data, 2);
									
	// Restoring the interrupt status which was stored & enabling the global interrupt */
	// LEAVE_CRITICAL_REGION();
	
	return dummy_data[1];
}


#endif /* HAL_BP3596_HAL_BP3596_C_ */
