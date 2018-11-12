#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#include "../at86rf212_param.h"
#include "../hal/hal_at86rf212_trx_access.h" 
#include "../mydebug/mydebug.h"
#include "tal_at86rf212_trx.h"


// =========================================================================================================================
// ***********************************************************
//
// Initialize Transceiver Abstraction Layer (TAL)
//
// ***********************************************************
trx_retval_t tal_init(void)
{
	printf("Info: --- --- Initialize Transceiver module (TRX) ... \n");
	if (trx_init() != TRX_SUCCESS) {
		printf("Info: --- --- FAILED\n");
		return TRX_FAILURE;
	}
	else
		printf("Info: --- --- SUCCEEDED\n");

	// Do the reset stuff. Generate random seed.
	printf("Info: --- --- Configure TRX ... \n");
	if (internal_tal_reset() != TRX_SUCCESS) {
		printf("Info: --- --- FAILED\n");
		return TRX_FAILURE;
	}
	printf("Info: --- --- SUCCEEDED\n");

	// Set the default CCA mode, CCA_MODE_DEFAULT = 0x01 = 906 MHz at86rf212, 126/172
	hal_trx_rf212_bit_write(SR_CCA_MODE, CCA_MODE_DEFAULT);

	// Default configuration to perform auto CSMA-CA
	hal_trx_rf212_reg_write(RG_CSMA_BE, ((MAXBE_DEFAULT << 4) | MINBE_DEFAULT));
	hal_trx_rf212_bit_write(SR_MAX_CSMA_RETRIES, MAX_CSMA_BACKOFFS_DEFAULT);

	// Set the trx in promiscous mode to receive all frame with CRC OK
	hal_trx_rf212_bit_write(SR_AACK_PROM_MODE, PROM_MODE_ENABLE);

	// Configuration to perform auto CRC for transmission
	hal_trx_rf212_bit_write(SR_TX_AUTO_CRC_ON, TX_AUTO_CRC_ENABLE);

	return TRX_SUCCESS;
}


// ***********************************************************
//
// Initialize the RF (after power on)
//
// ***********************************************************
static trx_retval_t trx_init(void)
{
	tal_trx_status_t trx_status;
	unsigned char poll_counter = 0;
	unsigned char part_number = 0xFF;
	unsigned char version_number = 0xFF;
	unsigned char man_id_0 = 0xFF;
	unsigned char man_id_1 = 0xFF;

	// Ensure control lines have correct levels
	RST_HIGH();
	SLP_TR_LOW();

	// Wait typical time
	hal_delay_us(P_ON_TO_CLKM_AVAILABLE_TYP_US);

	// Apply reset pulse
	RST_LOW();
	hal_delay_us(RST_PULSE_WIDTH_US);
	RST_HIGH();

	// Verify that TRX_OFF can be written
	do {
		// Wait not more than max. value of TR1
		if (poll_counter == P_ON_TO_CLKM_ATTEMPTS) {
			return TRX_FAILURE;
		}
		// Wait a short time interval
		hal_delay_us(TRX_POLL_WAIT_TIME_US);
		poll_counter++;
		// Check if AT86RF212 is connected; omit manufacturer id check
		part_number 	= hal_trx_rf212_reg_read (RG_PART_NUM);
	}
	while (part_number != PART_NUM_AT86RF212);
	
	version_number 	= hal_trx_rf212_reg_read (RG_VERSION_NUM);
	man_id_0 		= hal_trx_rf212_reg_read (RG_MAN_ID_0);
	man_id_1 		= hal_trx_rf212_reg_read (RG_MAN_ID_1);
	printf("Info: --- --- --- TRX PART_NUM=%x, VER_NUM=%x, MAN_ID0=%x, MAN_ID1=%x\n", part_number, version_number, man_id_0, man_id_1);
	
	// Set trx to off mode
	hal_trx_rf212_reg_write(RG_TRX_STATE, CMD_FORCE_TRX_OFF);

	// Verify that the trx has reached TRX_OFF
	poll_counter = 0;
	do {
		// Wait a short time interval.
		hal_delay_us(TRX_POLL_WAIT_TIME_US);

		trx_status = (tal_trx_status_t) hal_trx_rf212_bit_read(SR_TRX_STATUS);

		// Wait not more than max attempts for state transition
		if (poll_counter == SLEEP_TO_TRX_OFF_ATTEMPTS) {
			return TRX_FAILURE;
		}
		poll_counter++;
	} while (trx_status != TRX_OFF);
	printf("Info: --- --- --- TRX has reached TRX_OFF\n");
	tal_trx_status = TRX_OFF;

	return TRX_SUCCESS;
}


// *******************************************************************************************
//
// Internal TAL reset function
//
// *******************************************************************************************
static trx_retval_t internal_tal_reset(void)
{
	if (trx_reset() != TRX_SUCCESS) {
		return TRX_FAILURE;
	}

	// Write the transceiver values except of the CSMA seed
	trx_config();

	// Generate a seed for the random number generator in function rand().
	// This is required (for example) as seed for the CSMA-CA algorithm.
	generate_rand_seed();

	// Reset TAL variables.
	tal_state = TAL_IDLE;

	return TRX_SUCCESS;
}


// *******************************************************************************************
//
// Reset transceiver
//
// *******************************************************************************************
static trx_retval_t trx_reset(void)
{
	tal_trx_status_t trx_status;
	unsigned char poll_counter = 0;

	// trx might sleep, so wake it up
	SLP_TR_LOW();
	hal_delay_us(SLEEP_TO_TRX_OFF_TYP_US);

	// Apply reset pulse
	RST_LOW();
	hal_delay_us(RST_PULSE_WIDTH_US);
	RST_HIGH();

	// verify that trx has reached TRX_OFF
	do {
		// Wait a short time interval.
		hal_delay_us(TRX_POLL_WAIT_TIME_US);

		trx_status = (tal_trx_status_t) hal_trx_rf212_bit_read(SR_TRX_STATUS);

		// Wait not more than max.
		if (poll_counter == SLEEP_TO_TRX_OFF_ATTEMPTS) {
			return TRX_FAILURE;
		}
		poll_counter++;
	} while (trx_status != TRX_OFF);

	tal_trx_status = TRX_OFF;

	return TRX_SUCCESS;
}


// *******************************************************************************************
//
// This function is called to configure the transceiver after reset
//
// *******************************************************************************************
static void trx_config(void)
{
	// Set pin driver strength
	hal_trx_rf212_bit_write(SR_PAD_IO_CLKM, PAD_CLKM_2_MA);
	hal_trx_rf212_bit_write(SR_CLKM_SHA_SEL, CLKM_SHA_DISABLE);
	hal_trx_rf212_bit_write(SR_CLKM_CTRL, CLKM_1MHZ);

	// ACKs for data requests, indicate pending data	// at86rf212.pdf, p.65/172
	hal_trx_rf212_bit_write(SR_AACK_SET_PD, SET_PD);
	// hal_trx_rf212_bit_write(SR_AACK_DIS_ACK, 1);
	// OQPSK-SIN-500
	hal_trx_rf212_reg_write(RG_TRX_CTRL_2, DEFAULT_PHY_MODE); 	// Thuan 20141209, at86rf212_param.h
	// Enable buffer protection mode
	hal_trx_rf212_bit_write(SR_RX_SAFE_MODE, RX_SAFE_MODE_ENABLE);

	// Enable poll mode
	hal_trx_rf212_bit_write(SR_IRQ_MASK_MODE, IRQ_MASK_MODE_ON);

	// The TRX_END interrupt of the transceiver is enabled.
	hal_trx_rf212_reg_write(RG_IRQ_MASK, TRX_IRQ_DEFAULT);
}


// *******************************************************************************************
//
// Generates a 16-bit random number used as initial seed for srand()
//
// *******************************************************************************************
static void generate_rand_seed(void)
{
	unsigned int seed = 0;
	unsigned char cur_random_val = 0;
	unsigned char i;

	set_trx_state(CMD_RX_ON);

	// We need to disable TRX IRQs while generating random values in RX_ON,
	// we do not want to receive frames at this point of time at all.
	// ENTER_TRX_REGION();		// POOLING is used instead INTERRUPT

	// The 16-bit random value is generated from various 2-bit random values.
	for (i = 0; i < 8; i++) {
		// Now we can safely read the 2-bit random number
		cur_random_val = hal_trx_rf212_bit_read(SR_RND_VALUE);
		seed = seed << 2;
		seed |= cur_random_val;
	}

	set_trx_state(CMD_FORCE_TRX_OFF);

	// Now we need to clear potential pending TRX IRQs and
	// enable the TRX IRQs again.
	hal_trx_rf212_reg_read(RG_IRQ_STATUS);

	// LEAVE_TRX_REGION();	// POOLING is used instead INTERRUPT

	// Set the seed for the random number generator.
	srand(seed);
}


// *******************************************************************************************
//
// Sets transceiver state
//
// *******************************************************************************************
static tal_trx_status_t set_trx_state(trx_cmd_t trx_cmd)
{
	tal_trx_status = (tal_trx_status_t) hal_trx_rf212_bit_read(SR_TRX_STATUS);

	// State transition is handled among FORCE_TRX_OFF, RX_ON and PLL_ON.
	// These are the essential states required for a basic transmission and reception.
	switch (trx_cmd) 
	{	
		case CMD_FORCE_TRX_OFF:
			// Handling FORCE_TRX_OFF state
			switch (tal_trx_status) 
			{
				case TRX_OFF:
					// Do nothing - maintain the previous state
					break;

				default:
					hal_trx_rf212_reg_write(RG_TRX_STATE, CMD_FORCE_TRX_OFF);
					hal_delay_us(1);
					break;
			}
			break;

		case CMD_PLL_ON:
			// Handling PLL_ON state
			switch (tal_trx_status) 
			{
				case PLL_ON:
					// Do nothing - maintain the previous state
					break;

				case TRX_OFF:
					switch_pll_on();
					break;

				case RX_ON:
				case RX_AACK_ON:
                case TX_ARET_ON:
					hal_trx_rf212_reg_write(RG_TRX_STATE, CMD_PLL_ON);
					hal_delay_us(1);
					break;
				
				case BUSY_RX:
                case BUSY_TX:
                case BUSY_RX_AACK:
                case BUSY_TX_ARET:
                    // Do nothing if TRX is busy
#if DEBUG_INFO == 1
                	// printf("CMD_PLL_ON: BUSY: TRX status = %x\n", tal_trx_status);
                	++MYDEBUG.cpob_session[MYDEBUG.cpob_index];
#endif
                    break;
                    break;
					
				default:
					printf("CMD_PLL_ON: TRX status = %x\n", tal_trx_status);
					assert("State transition not handled" == 0);
					break;
			}
			break;

		case CMD_RX_ON:
			// Handling the RX_ON state
			switch (tal_trx_status) 
			{
				case RX_ON:
					// Do nothing - maintain the previous state
					break;

				case PLL_ON:
				case RX_AACK_ON:
                case TX_ARET_ON:
					hal_trx_rf212_reg_write(RG_TRX_STATE, CMD_RX_ON);
					hal_delay_us(1);
					break;

				case TRX_OFF:
					switch_pll_on();
					hal_trx_rf212_reg_write(RG_TRX_STATE, CMD_RX_ON);
					hal_delay_us(1);
					break;

				case BUSY_RX:
                case BUSY_TX:
                case BUSY_RX_AACK:
                case BUSY_TX_ARET:
                    // Do nothing if trx is busy
#if DEBUG_INFO == 1
					// printf("CMD_RX_ON: BUSY: TRX status = %x\n", tal_trx_status);
                	++MYDEBUG.crob_session[MYDEBUG.crob_index];
#endif
                    break;
					
				default:
					printf("TRX status = %x\n", tal_trx_status);
					assert("CMD_RX_ON: State transition not handled" == 0);
					break;
			}
			break;

		default:
			printf("TRX status = %x\n", tal_trx_status);
			assert("TRX command not handled" == 0);
			break;
	}

	// Hold till the state transition is complete
	do {
		tal_trx_status = (tal_trx_status_t) hal_trx_rf212_bit_read(SR_TRX_STATUS);
	} while (tal_trx_status == STATE_TRANSITION_IN_PROGRESS);

	return tal_trx_status;
}


// *******************************************************************************************
//
// Switches the PLL on
//
// *******************************************************************************************
static void switch_pll_on(void)
{
	trx_irq_reason_t irq_status;
	unsigned char poll_counter = 0;

	// Check if trx is in TRX_OFF; only from PLL_ON the following procedure is applicable
	if (hal_trx_rf212_bit_read(SR_TRX_STATUS) != TRX_OFF) {
		assert("Switch PLL_ON failed, because trx is not in TRX_OFF" == 0);
		return;
	}

	// clear PLL lock bit
	hal_trx_rf212_reg_read(RG_IRQ_STATUS);	

	// Switch PLL on 
	hal_trx_rf212_reg_write(RG_TRX_STATE, CMD_PLL_ON);

	// Check if PLL has been locked
	do {
		irq_status = (trx_irq_reason_t) hal_trx_rf212_reg_read(RG_IRQ_STATUS);

		if (irq_status & TRX_IRQ_PLL_LOCK) {
			return;	// PLL is locked now
		}

		// Wait a time interval of typical value for state change
		hal_delay_us(TRX_OFF_TO_PLL_ON_TIME_US);

		poll_counter++;
	} while (poll_counter < PLL_LOCK_ATTEMPTS);
}


// *******************************************************************************************
//
// Transceiver interrupt handler
//
// *******************************************************************************************
void trx_irq_handler_cb(void)
{
	trx_irq_reason_t trx_irq_cause;
	trx_irq_cause = (trx_irq_reason_t) hal_trx_rf212_reg_read(RG_IRQ_STATUS);
	
	unsigned char phy_frame_len;
	unsigned char *rx_frame_ptr = at86rfx_rx_buffer;
	
	tal_trx_status_t trx_status;
	
	// TRX_END reason depends on if the TRX is currently used for transmission or reception.
	if (trx_irq_cause & TRX_IRQ_TRX_END) 
	{
		// Handle TX interrupt
		if (tal_state == TAL_TX_AUTO)
		{
			// TRX has handled the entire transmission incl. CSMA
			tal_state = TAL_TX_END;	// Further handling is done by tx_end_handling()

			// After transmission has finished, switch receiver on again.
			do {
				trx_status = set_trx_state(CMD_RX_ON);
			} while (trx_status != RX_ON);
		}
		
		// Handle RX interrupt
		else 
		{
			// Perform FCS check for frame validation
			if (CRC16_NOT_VALID == hal_trx_rf212_bit_read(SR_RX_CRC_VALID)) 
			{
#if DEBUG_INFO == 1
				// printf("Info: --- --- --- --- CRC16 not valid\n");
				++MYDEBUG.crc_invalid_session[MYDEBUG.crc_invalid_index];
#endif
				return;
			}
				
			// Get frame length from transceiver
			hal_trx_rf212_frame_read(&phy_frame_len, LENGTH_FIELD_LEN);

			// Check for valid frame length
			if (phy_frame_len > PHY_MAX_LENGTH) 
			{
#if DEBUG_INFO == 1
				// printf("Info: --- --- --- --- PHY FRAME LENGTH not valid\n");
				++MYDEBUG.flen_invalid_session[MYDEBUG.flen_invalid_index];
#endif
				return;
			}
				
			// Frame read from transceiver buffer
			hal_trx_rf212_frame_read(rx_frame_ptr, LENGTH_FIELD_LEN + phy_frame_len);

			// Set flag indicating received frame to be handled
			at86rfx_frame_rx = true;
		}

		// After transmission has finished, switch receiver on again.
		// do {
		//	trx_status = set_trx_state(CMD_RX_ON);
		// } while (trx_status != RX_ON);
	}
	
	else if (trx_irq_cause != TRX_IRQ_TRX_END)
    {
        // PLL_LOCK interrupt might be set, because poll mode is enabled.
        /*
        if (trx_irq_cause & TRX_IRQ_PLL_LOCK)
        {
            ASSERT("unexpected IRQ: TRX_IRQ_PLL_LOCK" == 0);
        }
        */
        if (trx_irq_cause & TRX_IRQ_PLL_UNLOCK)
        {
            assert("Unexpected IRQ: TRX_IRQ_PLL_UNLOCK" == 0);
        }
        /* RX_START interrupt might be set, because poll mode is enabled. */
        /*
        if (trx_irq_cause & TRX_IRQ_RX_START)
        {
            ASSERT("unexpected IRQ: TRX_IRQ_RX_START" == 0);
        }
        */
        if (trx_irq_cause & TRX_IRQ_CCA_ED_DONE)
        {
            assert("Unexpected IRQ: TRX_IRQ_CCA_ED_DONE" == 0);
        }
        /* AMI interrupt might set, because poll mode is enabled. */
        /*
        if (trx_irq_cause & TRX_IRQ_AMI)
        {
            ASSERT("unexpected IRQ: TRX_IRQ_AMI" == 0);
        }
        */
        if (trx_irq_cause & TRX_IRQ_TRX_UR)
        {
            assert("Unexpected IRQ: TRX_IRQ_TRX_UR" == 0);
        }
        if (trx_irq_cause & TRX_IRQ_BAT_LOW)
        {
            assert("Unexpected IRQ: TRX_IRQ_BAT_LOW" == 0);
        }
    }
}


// *******************************************************************************************
//
// Configures the transceiver for frame transmission
//
// *******************************************************************************************
void tx_frame_config(void)
{
	tal_trx_status_t trx_status;

	// Set trx to PLL_ON state to initiate transmission procedure
	do {
		trx_status = set_trx_state(CMD_PLL_ON);
	} while (trx_status != PLL_ON);

	tal_state = TAL_TX_AUTO;

	// Toggle the SLP_TR pin triggering transmission
	SLP_TR_HIGH();
	hal_delay_ns(65);	// 65ns (hal_config_wiringpi.h)
	SLP_TR_LOW();
}

// ***********************************************************
//
// Handles the transceiver state
//
// ***********************************************************
void handle_tal_state(void)
{
	// Handle the TAL state machines
	switch (tal_state) {
	case TAL_IDLE:
		// Do nothing, but fall through ...
		
	case TAL_TX_AUTO:
		// Wait until state is changed to TAL_TX_END inside TX end ISR
		break;

	case TAL_TX_END:
		tx_end_handling();
		break;

	default:
		// Assert("tal_state is not handled" == 0);
		printf("Info: --- --- --- --- handle_tal_state -> tal_state is not handled\n");
		break;
	}
}


// ***********************************************************
//
// This function handles the callback for the transmission end.
//
// ***********************************************************
static void tx_end_handling(void)
{
	tal_state = TAL_IDLE;
	
	// Read the register TRAC_STATUS (at86rf212.pdf -> pp.61/172)					// 20141128 - Thuan
	trx_trac_status = (trx_trac_status_t) hal_trx_rf212_bit_read(SR_TRAC_STATUS);
	
	// call back function is called based on tx status
	switch (trx_trac_status) {
	case TRAC_SUCCESS:
		// AT86RFX_TX_STATUS_NOTIFY(AT86RFX_SUCCESS); // From Atmel code
		printf("Info: --- --- --- --- tx_end_handling -> AT86RFX_SUCCESS\n");
		break;

	case TRAC_CHANNEL_ACCESS_FAILURE:
		// AT86RFX_TX_STATUS_NOTIFY(AT86RFX_CHANNEL_ACCESS_FAILURE); // From Atmel code
		printf("Info: --- --- --- --- tx_end_handling -> AT86RFX_CHANNEL_ACCESS_FAILURE\n");
		break;

	case TRAC_INVALID:
		// AT86RFX_TX_STATUS_NOTIFY(AT86RFX_FAILURE); // From Atmel code
		// printf("Info: --- --- --- --- tx_end_handling -> AT86RFX_FAILURE\n");
		break;

	default:
		// Assert("Unexpected tal_tx_state" == 0);
		// AT86RFX_TX_STATUS_NOTIFY(AT86RFX_FAILURE); // From Atmel code
		printf("Info: --- --- --- --- tx_end_handling -> AT86RFX_FAILURE\n");
		break;
	}
}
