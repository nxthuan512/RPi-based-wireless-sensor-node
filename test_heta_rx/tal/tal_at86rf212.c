#include <stdio.h>
#include <stdlib.h>

#include "../at86rf212_param.h"
#include "../hal/hal_at86rf212_trx_access.h"
#include "tal_at86rf212_trx.h"
#include "tal_at86rf212.h"

#include "../hal_bp3596/hal_bp3596.h"


// ***********************************************************
//
// Initialize Transceiver module (top level)
//
// ***********************************************************
at86rfx_retval_t at86rfx_init(void)
{
	printf("Info: --- Initialize AT86RF212 Power ... \n");
	// No-power BP3596
	hal_bp3596_power_en(0);
	hal_delay_ms(500);
	// Power AT86RF212 and wait for 500 ms
	hal_trx_rf212_power_en(1);
	hal_delay_ms(500);

	// Initialize wiringPi library, SPI, interrupt pin
	printf("Info: --- Initialize Raspberry Pi ... \n");
	hal_trx_rf212_init();
	
	// Initialize RF transceiver
	printf("Info: --- Initialize RF Transceiver ... \n");
	if (tal_init() != TRX_SUCCESS) {
		printf("Info: --- FAILED\n");
		return AT86RFX_FAILURE;
	}
	else
		printf("Info: --- SUCCEEDED\n");

	hal_trx_rf212_bit_write(SR_CHANNEL, CURRENT_CHANNEL_DEFAULT);
	hal_trx_rf212_reg_write(RG_TRX_STATE, CMD_RX_ON);

	return AT86RFX_SUCCESS;
}


// ***********************************************************
//
// This function configures the tranceiver in TX mode and transmits the frame
//
// ***********************************************************
void at86rfx_tx_frame(unsigned char * frame_tx)
{
	// DISABLE_TRX_IRQ();
	tx_frame_config();

	//
	// Send the frame to the transceiver.
	// Note: The PhyHeader is the first byte of the frame to be sent to 
	// the transceiver and this contains the frame length.
	// The actual length of the frame to be downloaded (parameter two
	// of hal_trx_rf212_frame_write) is
	// 	1 octet frame length octet
	// 	+ n octets frame (i.e. value of frame_tx[0])
	// 	- 2 octets FCS. Shall be added automatically
	//
	hal_trx_rf212_frame_write(frame_tx, frame_tx[0] - LENGTH_FIELD_LEN);
	
	hal_trx_rf212_irq();
		
	// ENABLE_TRX_IRQ();
}


// ***********************************************************
//
// If the transceiver has received a frame and it has been placed
// into the RF buffer, frame needs to be processed further in application.
//
// ***********************************************************
/*
void at86rfx_task(void)
{
	If the transceiver has received a frame and it has been placed
	into the RF buffer, frame needs to be processed further in application.
	if (at86rfx_frame_rx) {
		AT86RFX_RX_NOTIFY(at86rfx_rx_buffer);
		at86rfx_frame_rx = false;
	}

	handle_tal_state();
}
*/

