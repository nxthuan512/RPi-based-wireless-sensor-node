/*
** FUNCTIONS TO READ/WRITE AT86RF212 REGISTER/FRAME/SRAM
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <assert.h>
#include <unistd.h>

#include "../at86rf212_param.h"
#include "../tal/tal_at86rf212_trx.h"
#include "hal_at86rf212_trx_access.h"
#include "hal_config_pin.h"


// ***********************************************************
//
// Enable RF power
//
// ***********************************************************
void hal_trx_rf212_power_en(uint8_t enable)
{
	// Initialize pins
	hal_GPIOOutputPin(AT86RF212_EN);

	// Power AT86RF212
	if (enable == 1)
	{
		hal_GPIOSetPin(AT86RF212_EN);
	}
	// No power
	else
	{
		hal_GPIOClearPin(AT86RF212_EN);
	}
}


// ===============================================================================================================================
// ***********************************************************
//
// The IRQ interrupt in here ...
//
// ***********************************************************





/*
// INTERRUPT
void hal_trx_rf212_irq (void)	// Atmel: 	ISR(ext_int_isr, EXT_INT_ISR_GROUP, EXT_INT_ISR_PRIORITY)
								//				-> trx_irq_handler_cb()
{
	// Calling the interrupt routines
	///////// Debug only /////////
	// printf("Debug: --- --- --- --- Interrupt function is called\n");
	at86rfx_irq = true;
}
*/

// POLLING
void hal_trx_rf212_irq(void)
{
	// int time_out = 0;

	// Wait for IRQ pin asserts
	// while ((IRQ_VALUE() == false) && (time_out < 1000))
	while (IRQ_VALUE() == false)
	{
		hal_delay_us(1);
		// ++time_out;
		// if (time_out >= 1000)
			// assert("Interrupt got error" == 0);
	}
	
	// After read interrupt value, IRQ pin is de-asserted automatically
	trx_irq_handler_cb();
}


// *******************************************************************************************
//
// Enable/disable the interrupt, use 'system' command (for Atmel compatibility)
// 
// *******************************************************************************************
/*
void hal_control_pin_interrupt(char *pin_str, char *command)
{
	char str1[32] = "/usr/local/bin/gpio edge ";
	char str3[40];
	
	// Make the command
	strcpy(str3, str1);
	strcat(str3, pin_str);
	strcat(str3, " ");
	strcat(str3, command);
	// Debug only
	// printf("Info: --- --- --- --- Command for IRQ: %s\n", str3);
	// Run the command
	system (str3);
}
*/


// ***********************************************************
//
// Initialize SPI channel 0 and all pins 
// (TRX_RST, SLP_TR, IRQ, DIG2) of RF module
//
// ***********************************************************
void hal_trx_rf212_init()
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
		printf("Info: --- --- Initialize SPI channel 0, clock speed 6.4 MHz ... \n");
		if (hal_SPI0Setup(6400000) == -1)
		{
			printf ("Info: --- --- FAILED\n");
			exit(0);
		}
		else
			printf("Info: --- --- SUCCEEDED\n");
	}
	
	// Initialize RST, SLPTR as GPIO output, DIG2 as GPIO input
	hal_GPIOOutputPin(AT86RF212_RST);
	hal_GPIOOutputPin(AT86RF212_SLPTR);
	hal_GPIOInputPin(AT86RF212_DIG2);
	
	// Initialize EXT_INT as interrupt for transceiver


	hal_GPIOInputPin(AT86RF212_IRQ);
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
void hal_trx_rf212_reg_write (unsigned char reg_addr, unsigned char reg_data)
{
	unsigned char dummy_data[2];
	
	// Saving the current interrupt status & disabling the global interrupt
	// ENTER_CRITICAL_REGION();
	
	// Prepare the command byte
	dummy_data[0] = WRITE_ACCESS_COMMAND | reg_addr;
	
	// Do dummy read for initiating SPI read
	dummy_data[1] = reg_data;	
	
	// Send command, dummy_data[0] is PHY status	
	hal_SPI0DataRW (dummy_data, 2);
									
	// Restoring the interrupt status which was stored & enabling the global interrupt */
	// LEAVE_CRITICAL_REGION();
}


// ***********************************************************
//
// Read current value from a register
//
// ***********************************************************
unsigned char hal_trx_rf212_reg_read (unsigned char reg_addr)
{
	unsigned char dummy_data[2];
	
	// Saving the current interrupt status & disabling the global interrupt
	// ENTER_CRITICAL_REGION();
	
	// Prepare the command byte
	dummy_data[0] = READ_ACCESS_COMMAND | reg_addr;
	
	// Do dummy read for initiating SPI read
	dummy_data[1] = 0xFF;	
	
	// Send command, dummy_data[0] is PHY status	
	hal_SPI0DataRW (dummy_data, 2);
									
	// Restoring the interrupt status which was stored & enabling the global interrupt */
	// LEAVE_CRITICAL_REGION();
	
	return dummy_data[1];
}


// ***********************************************************
//
// Sub-register write
//
// ***********************************************************
void hal_trx_rf212_bit_write(unsigned char reg_addr, unsigned char mask, unsigned char pos, unsigned char new_value)
{
	unsigned char current_reg_value;
	
    current_reg_value = hal_trx_rf212_reg_read(reg_addr);	// Ex: 8 bits, mask = 0010_0000, pos = 5, new_value = 0
															// current_value = 1110_1010
	current_reg_value &= ~mask;								// current_value = 1101_1010 
    new_value <<= pos;										// new_value	 = 0000_0000
    new_value &= mask;										// new_value	 = 0000_0000
    new_value |= current_reg_value;							//  new_value	 = 1101_1010
    
	hal_trx_rf212_reg_write(reg_addr, new_value);
}


// ***********************************************************
//
// Sub-register read
//
// ***********************************************************
unsigned char hal_trx_rf212_bit_read(unsigned char reg_addr, unsigned char mask, unsigned char pos)
{
	unsigned char result;
	
    result = hal_trx_rf212_reg_read(reg_addr);	
	result &= mask;
	result >>= pos;	
    
	return result;
}


// ***********************************************************
//
// Write data into frame buffer
//
// ***********************************************************
void hal_trx_rf212_frame_write (unsigned char *data, unsigned char length)
{
	unsigned char *dummy_data;
	
	// Saving the current interrupt status & disabling the global interrupt
	// ENTER_CRITICAL_REGION();
	
	// Create an array to store command and data
	dummy_data = (unsigned char*) calloc ((length + 1), sizeof(unsigned char));
	if (dummy_data == NULL)
	{	
		printf("Error: hal_trx_rf212_frame_write -> Cannot create dummy_data array\n");
		exit (1);
	}
	
	// Prepare the command byte
	dummy_data[0] = TRX_CMD_FW;
	
	// Copy the content of data to dummy_data
	memcpy(&dummy_data[1], &data[0], length);
	
	// Send command, dummy_data[0] is PHY status	
	hal_SPI0DataRW (dummy_data, (length + 1));
	free (dummy_data);
	
	// Restoring the interrupt status which was stored & enabling the global interrupt */
	// LEAVE_CRITICAL_REGION();
}


// ***********************************************************
//
// Read frame buffer
//
// ***********************************************************
void hal_trx_rf212_frame_read (unsigned char *data, unsigned char length)
{
	unsigned char *dummy_data;
	
	// Saving the current interrupt status & disabling the global interrupt
	// ENTER_CRITICAL_REGION();
	
	// Create an array to store command and data
	dummy_data = (unsigned char*) calloc ((length + 1), sizeof(unsigned char));
	if (dummy_data == NULL)
	{	
		printf("Error: hal_trx_rf212_frame_read -> Cannot create dummy_data array\n");
		exit (1);
	}
	
	// Prepare the command byte
	dummy_data[0] = TRX_CMD_FR;
	
	// Send command, dummy_data[0] is PHY status	
	hal_SPI0DataRW (dummy_data, (length + 1));
	
	// Copy data from dummy_data to data
	memcpy(&data[0], &dummy_data[1], length);
	
	free (dummy_data);
	
	// Restoring the interrupt status which was stored & enabling the global interrupt */
	// LEAVE_CRITICAL_REGION();
}


// ***********************************************************
//
// Write data into the SRAM
//
// ***********************************************************
void hal_trx_rf212_sram_write (unsigned char addr, unsigned char *data, unsigned char length)
{
	unsigned char *dummy_data;
	
	// Saving the current interrupt status & disabling the global interrupt
	// ENTER_CRITICAL_REGION();
	
	// Create an array to store command and data
	dummy_data = (unsigned char*) calloc ((length + 2), sizeof(unsigned char));
	if (dummy_data == NULL)
	{	
		printf("Error: hal_trx_rf212_sram_write -> Cannot create dummy_data array\n");
		exit (1);
	}
	
	// Prepare the command byte
	dummy_data[0] = TRX_CMD_SW;
	
	// The address from which the write operation should start
	dummy_data[1] = addr;
	
	// Copy the content of data to dummy_data
	memcpy(&dummy_data[2], &data[0], length);
	
	// Send command, dummy_data[0] is PHY status	
	hal_SPI0DataRW (dummy_data, (length + 2));
	
	free (dummy_data);
	
	// Restoring the interrupt status which was stored & enabling the global interrupt */
	// LEAVE_CRITICAL_REGION();
}


// ***********************************************************
//
// Read data from SRAM
//
// ***********************************************************
void hal_trx_rf212_sram_read (unsigned char addr, unsigned char *data, unsigned char length)
{
	unsigned char *dummy_data;
	
	// Wait for 500 ns ???
	hal_delay_us(1);
	
	// Saving the current interrupt status & disabling the global interrupt
	// ENTER_CRITICAL_REGION();
	
	// Create an array to store command and data
	dummy_data = (unsigned char*) calloc ((length + 2), sizeof(unsigned char));
	if (dummy_data == NULL)
	{	
		printf("Error: hal_trx_rf212_sram_read -> Cannot create dummy_data array\n");
		exit (1);
	}
	
	// Prepare the command byte
	dummy_data[0] = TRX_CMD_SR;
	
	// The address from which the write operation should start
	dummy_data[1] = addr;
	
	// Send command, dummy_data[0] is PHY status	
	hal_SPI0DataRW (dummy_data, (length + 2));
	
	// Copy data from dummy_data to data
	memcpy(&data[0], &dummy_data[2], length);
		
	free (dummy_data);
	
	// Restoring the interrupt status which was stored & enabling the global interrupt */
	// LEAVE_CRITICAL_REGION();
}


// ***********************************************************
//
// Writes and reads data into/from SRAM
//
// ***********************************************************
// void hal_trx_rf212_aes_wrrd (unsigned char addr, unsigned char *idata, unsigned char length)
// pal_trx_access.c, line 357

