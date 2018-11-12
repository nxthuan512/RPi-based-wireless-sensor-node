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


#endif /* HAL_BP3596_HAL_BP3596_C_ */
