#include "../at86rf212_param.h"
#include "../hal/hal_config_wiringpi.h"
#include "../tal/tal_at86rf212.h"
#include "../protocol/protocol.h"
#include "../utils/utils.h"
#include "../mydebug/mydebug.h"
#include "fixed_data.h"


// ===========================================================
//
// App init
//
// ===========================================================
void app_fixed_data_init(int argc, char **argv, node_t *NODE)
{
	// System initialization
	printf("Info: Initialize system ... \n");
	if (at86rfx_init() != AT86RFX_SUCCESS)
		printf("Info: FAILED\n");
	else
		printf("Info: SUCCEEDED\n");

	// Get the configuration
	if (argc > 1)
	{
		NODE->sess_window_size = atoi(argv[1]);
		NODE->sess_tx_delay = atoi(argv[2]);
	}
	// If there is no configuration, get the default
	else
	{
		NODE->sess_window_size = PACKETS_PER_TRANS;
		NODE->sess_tx_delay = 0;
	}
}


// ===========================================================
//
// TX app
//
// ===========================================================
void app_fixed_data_tx_data(node_t NODE)
{
	//
	sess_t SESSION;
	appbuff_t BUFFER;
		
	///////// Time calculation /////////
	uint32_t i;

	at86rfx_frame_rx = false;

	// ------ Initialize BUFFER information  ------
	BUFFER.data = (uint8_t*) calloc (APPBUFF_SIZE, sizeof(uint8_t));
	if (BUFFER.data == NULL)
	{
		printf("Info: --- Not enough memory to store data file ... \n");
		exit (1);
	}
	printf("Info: --- Read image data from file ... \n");
	BUFFER.length = writeBinaryFileToArray(FRAME_TX, &BUFFER.data[0]);

#if DEBUG_INFO == 1		// ----------------------------------------
	debug_init();
#endif
	
	printf("Info: --- ====================================== \n");
	printf("Info: --- Sending image data ... \n");
	printf("Info: --- ====================================== \n");

	i = 0;
	do
	{
		// ------ Initialize SESSION information  ------
		SESSION.frame_length = FRAME_SIZE;
		if ((BUFFER.length - i) < FRAME_SIZE)
			SESSION.frame_length = (uint16_t)(BUFFER.length - i);

		SESSION.packet_length = SCPL;
		SESSION.num_of_packet = SESSION.frame_length / SESSION.packet_length;
		if ((SESSION.frame_length % SESSION.packet_length) != 0)
			++SESSION.num_of_packet;
		SESSION.frame_data = &BUFFER.data[i];

		// Get from NODE
		SESSION.src_addr 	= NODE.src_addr;
		SESSION.dest_addr 	= NODE.dest_addr;
		SESSION.window_size = NODE.sess_window_size; // the size of window (number of packets/transaction) (adaptive)
		SESSION.tx_delay 	= NODE.sess_tx_delay; // delay between 2 consecutive send (adaptive)
		SESSION.time_out 	= 0;
		SESSION.guarantee_end = false;	// unused

		// ------ Run SESSION ------
		printf("\n ------------------------------------------------------\n");
		printf("Debug: --- Session - position: %d %d\n", MYDEBUG.loss_msg_index, i);
		pro_tx(&SESSION);

		// Check with system time-out
		if (SESSION.time_out < SESS_TIME_OUT)
		{
			i += FRAME_SIZE;

#if DEBUG_INFO == 1		// ----------------------------------------
			MYDEBUG.loss_msg_total += MYDEBUG.loss_msg_session[MYDEBUG.loss_msg_index];
			++MYDEBUG.loss_msg_index;

			MYDEBUG.crob_total += MYDEBUG.crob_session[MYDEBUG.crob_index];
			++MYDEBUG.crob_index;

			MYDEBUG.crc_invalid_total += MYDEBUG.crc_invalid_session[MYDEBUG.crc_invalid_index];
			++MYDEBUG.crc_invalid_index;

			MYDEBUG.flen_invalid_total += MYDEBUG.flen_invalid_session[MYDEBUG.flen_invalid_index];
			++MYDEBUG.flen_invalid_index;
#endif
		}

	} while ((SESSION.time_out < SESS_TIME_OUT) && (i < BUFFER.length));


	if (SESSION.time_out >= SESS_TIME_OUT)
	{
		printf("Info: --- Exit due to TIME-OUT %d seconds\n", SESS_TIME_OUT/1000000);
	}

#if DEBUG_INFO == 1		// ----------------------------------------
	debug_print();
#endif

	free (BUFFER.data);
}
