/*
 * hal_bp3596_config_pin.h
 *
 *  Created on: Feb 21, 2016
 *      Author: nxthuan512
 */

#include <stdint.h>


#ifndef HAL_BP3596_HAL_BP3596_CONFIG_PIN_H_
#define HAL_BP3596_HAL_BP3596_CONFIG_PIN_H_

// *******************************************************************************************
// Define the RF pins <-> RPCM pin
// *******************************************************************************************
#define BP3596_RST		(23)	// Reset pin connects with GPIO 23 in RPCM	- O
#define BP3596_IRQ		(21)	// Interrupt pin connects with GPIO 21 in RPCM - I

#define BP3596_EN		(17)

#endif /* HAL_BP3596_HAL_BP3596_CONFIG_PIN_H_ */
