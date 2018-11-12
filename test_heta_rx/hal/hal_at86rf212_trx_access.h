#include <wiringPi.h>
#include <wiringPiSPI.h>

#include "hal_config_pin.h"
#include "hal_config_wiringpi.h"


// ***********************************************************
// Definition for Atmel library compatibility
// ***********************************************************
//////////// From Atmel ////////////
/*
// Enables the transceiver interrupts
#define ENABLE_TRX_IRQ()			gpio_enable_pin_interrupt(AT86RFX_IRQ_PIN, GPIO_RISING_EDGE)
// Disable the transceiver interrupts
#define DISABLE_TRX_IRQ()           gpio_disable_pin_interrupt(AT86RFX_IRQ_PIN)
// Clear the transceiver interrupts
#define CLEAR_TRX_IRQ()             gpio_clear_pin_interrupt_flag(AT86RFX_IRQ_PIN)
// This macro saves the trx interrupt status and disables the trx interrupt
#define ENTER_TRX_REGION()          DISABLE_TRX_IRQ()
// This macro restores the transceiver interrupt status
#define LEAVE_TRX_REGION()          ENABLE_TRX_IRQ()
*/

//////////// To wiringPi ////////////
/*
// Enables the transceiver interrupts
#define ENABLE_TRX_IRQ()  			hal_control_pin_interrupt(AT86RF212_IRQ_STR, "rising")
// Disable the transceiver interrupts
#define DISABLE_TRX_IRQ()   		hal_control_pin_interrupt(AT86RF212_IRQ_STR, "none")
// This macro saves the trx interrupt status and disables the trx interrupt.
#define ENTER_TRX_REGION()			DISABLE_TRX_IRQ()
// This macro restores the transceiver interrupt status
#define LEAVE_TRX_REGION()          ENABLE_TRX_IRQ()

// This macro saves the global interrupt status <-> disable all interrupts
#define ENTER_CRITICAL_REGION()      DISABLE_TRX_IRQ()
// This macro restores the global interrupt status <-> enable all interrupts
#define LEAVE_CRITICAL_REGION()      ENABLE_TRX_IRQ()
*/

// Reset pin low
#define RST_LOW()           hal_GPIOClearPin(AT86RF212_RST)
// Reset pin high
#define RST_HIGH()          hal_GPIOSetPin(AT86RF212_RST)
// Sleep pin low
#define SLP_TR_LOW()		hal_GPIOClearPin(AT86RF212_SLPTR)
// Sleep pin low
#define SLP_TR_HIGH()		hal_GPIOSetPin(AT86RF212_SLPTR)
// IRQ 
#define IRQ_VALUE()			hal_GPIOGetPin(AT86RF212_IRQ)

// *******************************************************************************************
// Definition for commands: Write/Read + Register/Frame/SRAM
// *******************************************************************************************
// Write access command of the transceiver
#define WRITE_ACCESS_COMMAND            (0xC0)
// Read access command to the tranceiver
#define READ_ACCESS_COMMAND             (0x80)
// Frame write command of transceiver
#define TRX_CMD_FW                      (0x60)
// Frame read command of transceiver
#define TRX_CMD_FR                      (0x20)
// SRAM write command of transceiver
#define TRX_CMD_SW                      (0x40)
// SRAM read command of transceiver
#define TRX_CMD_SR                      (0x00)


// ===============================================================================================================================
// *******************************************************************************************
// Function:
//		void hal_trx_rf212_power_en(uint8_t enable)
//
// Description:
//		Enable RF power
//
// Parameters:
//		enable = 0: no-power
//		enable = 1: power RF
//
// Return:
//		None
//
// *******************************************************************************************
void hal_trx_rf212_power_en(uint8_t enable);


// *******************************************************************************************
// Function: 
//		hal_control_pin_interrupt(char *pin, char *command)
// 
// Description:
//		Enable/disable the interrupt, use 'system' command (for Atmel compatibility)
// 
// Parameters:
//		pin 	 - interrupt pin (string form)
//		command	 - "rising": enable interrupt
//				   "none":   disable interrupt	
//
// Return:
//		None
// *******************************************************************************************
void hal_control_pin_interrupt(char *pin_str, char *command);


// *******************************************************************************************
// Function: 
//		void hal_trx_rf212_irq()
// 
// Description:
//		Process the interrupt from RF module
// 
// Parameters:
//		None 
//
// Return:
//		None
// *******************************************************************************************
void hal_trx_rf212_irq();


// *******************************************************************************************
// Function: 
//		void hal_trx_rf212_init()
// 
// Description:
//		Initialize SPI channel 0 and all pins (TRX_RST, SLP_TR, IRQ, DIG2) of RF module
// 
// Parameters:
//		None 
//
// Return:
//		None
// *******************************************************************************************
void hal_trx_rf212_init();


// *******************************************************************************************
// Function: 
//		unsigned char hal_trx_rf212_reg_write (unsigned char reg_addr, unsigned char reg_data)
// 
// Description:
//		Write data into a register
// 
// Parameters:
//		reg_addr	- Register address from 0 to 63
//		reg_data	- Data to be written to register	
//
// Return:
//		None
// *******************************************************************************************
void hal_trx_rf212_reg_write (unsigned char reg_addr, unsigned char reg_data);


// *******************************************************************************************
// Function: 
//		unsigned char hal_trx_rf212_reg_read (unsigned char reg_addr)
// 
// Description:
//		Read current value from a register
// 
// Parameters:
//		reg_addr	- Register address from 0 to 63
//
// Return:
//		Content of register
// *******************************************************************************************
unsigned char hal_trx_rf212_reg_read (unsigned char reg_addr);


// *******************************************************************************************
// Function: 
//		void hal_trx_rf212_bit_write(unsigned char reg_addr, unsigned char mask, unsigned char pos, unsigned char new_value)
// 
// Description:
//		Sub-register write
// 
// Parameters:
//		reg_addr	- Offset of the register
//		mask		- Bit mask of the sub-register
//		pos			- Bit position of the sub-register
//		new_value	- Data, which is muxed into the register
//
// Return:
//		None
// *******************************************************************************************
void hal_trx_rf212_bit_write(unsigned char reg_addr, unsigned char mask, unsigned char pos, unsigned char new_value);


// *******************************************************************************************
// Function: 
//		unsigned char hal_trx_rf212_bit_read(unsigned char reg_addr, unsigned char mask, unsigned char pos)
// 
// Description:
//		Sub-register read
// 
// Parameters:
//		reg_addr	- Offset of the register
//		mask		- Bit mask of the sub-register
//		pos			- Bit position of the sub-register
//
// Return:
//		Data value of the read bit(s)
// *******************************************************************************************
unsigned char hal_trx_rf212_bit_read(unsigned char reg_addr, unsigned char mask, unsigned char pos);


// *******************************************************************************************
// Function: 
//		void hal_trx_rf212_frame_write (unsigned char *data, unsigned char length)
// 
// Description:
//		Write data into frame buffer
// 
// Parameters:
//		data		- Pointer to the location where data stored
//		length		- Number of bytes to be written
//
// Return:
//		None
// *******************************************************************************************
void hal_trx_rf212_frame_write (unsigned char *data, unsigned char length);


// *******************************************************************************************
// Function: 
//		void hal_trx_rf212_frame_read (unsigned char *data, unsigned char length)
// 
// Description:
//		Read frame buffer
// 
// Parameters:
//		data	- Pointer to the location where data stored
//		length	- Number of bytes to be read
//
// Return:
//		None
// *******************************************************************************************
void hal_trx_rf212_frame_read (unsigned char *data, unsigned char length);


// *******************************************************************************************
// Function: 
//		void hal_trx_rf212_sram_write (unsigned char addr, unsigned char *data, unsigned char length)
// 
// Description:
//		Write data into SRAM
// 
// Parameters:
//		addr		- Start address in SRAM for read operation
//		data		- Pointer to the location where data stored
//		length		- Number of bytes to be write from SRAM
//
// Return:
//		None
// *******************************************************************************************
void hal_trx_rf212_sram_write (unsigned char addr, unsigned char *data, unsigned char length);


// *******************************************************************************************
// Function: 
//		void hal_trx_rf212_sram_read (unsigned char addr, unsigned char *data, unsigned char length)
// 
// Description:
//		Read data from SRAM
// 
// Parameters:
//		addr	- Start address in SRAM for read operation
//		data	- Pointer to the location where data stored
//		length	- Number of bytes to be read from SRAM
//
// Return:
//		None
// *******************************************************************************************
void hal_trx_rf212_sram_read (unsigned char addr, unsigned char *data, unsigned char length);
