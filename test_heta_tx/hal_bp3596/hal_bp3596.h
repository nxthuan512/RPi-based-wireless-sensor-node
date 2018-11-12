/*
 * hal_bp3596.h
 *
 *  Created on: Feb 21, 2016
 *      Author: nxthuan512
 */

#ifndef HAL_BP3596_HAL_BP3596_H_
#define HAL_BP3596_HAL_BP3596_H_

#include <wiringPi.h>
#include <wiringPiSPI.h>

#include "hal_bp3596_config_pin.h"


// *******************************************************************************************
// Definition for commands: Write/Read + Register/Frame/SRAM
// *******************************************************************************************
// Write access command of the transceiver
#define BP3596_WAC     (0x01)
// Read access command to the transceiver
#define BP3596_RAC     (0x00)


// ===============================================================================================================================
// *******************************************************************************************
// Function:
//		void hal_bp3596_power_en(uint8_t enable)
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
void hal_bp3596_power_en(uint8_t enable);


void hal_bp3596_init();




#endif /* HAL_BP3596_HAL_BP3596_H_ */
