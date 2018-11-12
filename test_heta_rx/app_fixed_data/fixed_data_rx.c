#include "../at86rf212_param.h"
#include "../hal/hal_config_wiringpi.h"
#include "../tal/tal_at86rf212.h"
#include "../tal/tal_at86rf212_trx.h"
#include "../protocol/protocol.h"
#include "../utils/utils.h"
#include "../mydebug/mydebug.h"
#include "fixed_data.h"


// ===========================================================
//
// RX app
//
// ===========================================================
void app_fixed_data_rx_data(node_t NODE)
{
	sess_t SESSION;
	appbuff_t BUFFER;

	register uint32_t i;
	char i_str[10];
	char file_name[64];


	at86rfx_frame_rx = false;

	// ------ Initialize BUFFER information  ------
	BUFFER.data = (uint8_t*) calloc (APPBUFF_SIZE, sizeof(uint8_t));
	if (BUFFER.data == NULL)
	{
		printf("Info: --- Not enough memory to store data file ... \n");
		exit (1);
	}
	printf("Info: --- Read image data from file ... \n");
	BUFFER.length = 0;

#if DEBUG_INFO == 1		// ----------------------------------------
	debug_init();
#endif

	// Read image data and store to array
	printf("Info: --- ============================================ \n");
	printf("Info: --- Receiving image data ... \n");
	printf("Info: --- ============================================ \n");

	i = 0;
	do
	{
		// ------ Initialize SESSION information  ------
		SESSION.frame_length = 0;
		SESSION.packet_length = 0;
		SESSION.num_of_packet = 0;
		SESSION.frame_data = &BUFFER.data[i];

		SESSION.src_addr = NODE.src_addr;
		SESSION.dest_addr = NODE.dest_addr;
		SESSION.window_size = PACKETS_PER_TRANS;
		SESSION.tx_delay = 0;
		SESSION.time_out = 0;
		SESSION.guarantee_end = false;	// it is set to 1 in PING command,
									// i.e., END is sent to TX perfectly

		// ------ Run SESSION ------
		printf("Debug: --- Session - position: %d %d\n", MYDEBUG.loss_msg_index, i);
		pro_rx(&SESSION);

		if ((SESSION.guarantee_end == true) && (SESSION.time_out < SESS_TIME_OUT))
		{
			BUFFER.length += SESSION.frame_length;
			i += FRAME_SIZE;

#if DEBUG_INFO == 1		// ----------------------------------------
		// +1: the 1st message ID is 0
		MYDEBUG.recv_msgid_current = 0;
		MYDEBUG.recv_msgid_order_total += (MYDEBUG.recv_msgid_order_session[MYDEBUG.recv_msgid_index] + 1);
		++MYDEBUG.recv_msgid_index;

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
	} while ((SESSION.time_out < SESS_TIME_OUT) && (i < APPBUFF_SIZE));


	// If no time-out
	if (SESSION.time_out < SESS_TIME_OUT)
	{
#if DEBUG_REALTIME == 0
		// For debug only. END_ACK is sent several time to
		// guarantee that SENDER receives properly.
		file_name[0] = CPARSP + FCS_LEN;
		file_name [1] = ISACK_PREFIX | END;
		GET16TO8(file_name[2], file_name[3], SESSION.src_addr);
		GET16TO8(file_name[4], file_name[5], SESSION.dest_addr);

		for (i = 0; i < 10; ++i)
		{
			at86rfx_tx_frame(file_name);
			handle_tal_state();
			PTX_SEND_WAIT(5000);
		}
#endif

		int2str(BUFFER.length, i_str, 10);
		strcpy(file_name, FRAME_RX);
		strcat(file_name, i_str);
		strcat(file_name, ".jpg");

		printf("Info: --- Write image data to file %s ... \n", file_name);
		writeArrayToBinaryFile (file_name, &BUFFER.data[0], BUFFER.length);
		printf("Info: --- --- SUCCEEDED \n");
	}

	//
	else
	{
		printf("Info: --- Exit due to TIME-OUT %d seconds\n", SESS_TIME_OUT/1000000);
	}

#if DEBUG_INFO == 1		// ----------------------------------------
		debug_print();
#endif

	free(BUFFER.data);
}
