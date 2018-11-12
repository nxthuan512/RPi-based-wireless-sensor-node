#include "../app_rpi_img/rpi_img.h"
#include "../at86rf212_param.h"
#include "../hal/hal_config_wiringpi.h"
#include "../tal/tal_at86rf212.h"
#include "../protocol/protocol.h"
#include "../utils/utils.h"
#include "../mydebug/mydebug.h"


// ===========================================================
//
// App init
//
// ===========================================================
void app_rpi_img_init()
{
	// System initialization
	printf("Info: Initialize system ... \n");
	if (at86rfx_init() != AT86RFX_SUCCESS)
		printf("Info: FAILED\n");
	else
		printf("Info: SUCCEEDED\n");
}


// ===========================================================
//
// Read image from file and send to RX
//
// ===========================================================
void app_rpi_img_send_data(node_t NODE)
{
	uint16_t i, n;
	uint16_t time_out;
	char cmd[256];
	char cmd_sub[32];
	sess_t SESSION;


	// Initialization
	SESSION.frame_data = (uint8_t*) calloc (FRAME_SIZE, sizeof(uint8_t));
	if (SESSION.frame_data == NULL)
	{
		printf("Info: --- Not enough memory to store data file ... \n");
		exit (1);
	}

#if DEBUG_INFO == 1		// ----------------------------------------
	debug_init();
#endif


	n = 0;
	time_out = 0;
	// If time_out, exit this function; 50 * 200ms = 10s
	while (time_out < 50)
	{
		i = n;
		while ((i == n) || (i == n + 1))
		{
			// Make the full path
			strcpy(cmd, "/home/pi/my_code/tmp/data/img");
			int2str(i, cmd_sub, 10);
			strcat(cmd, cmd_sub);
			strcat(cmd, ".jpg");

			// Read the file
			SESSION.frame_length = writeBinaryFileToArray (&cmd[0], &SESSION.frame_data[0]);

			// If file is not available yet or mmap skip this file index
			if (SESSION.frame_length == 0)
			{
				++i;
				if (i == n + 2)
				{
					hal_delay_ms(100);
					++time_out;
				}
			}

			// Otherwise, send the file
			else
			{
				printf("Debug: --- Process %s, frame_length = %d\n", cmd, SESSION.frame_length);
				time_out = 0;
				n = i + 1;

				// ------ Initialize SESSION information  ------
				SESSION.packet_length = SCPL;
				SESSION.num_of_packet = SESSION.frame_length / SESSION.packet_length;
				if ((SESSION.frame_length % SESSION.packet_length) != 0)
					++SESSION.num_of_packet;

				SESSION.src_addr = NODE.src_addr;
				SESSION.dest_addr = NODE.dest_addr;
				SESSION.window_size = PACKETS_PER_TRANS; // the size of window (number of packets/transaction) (adaptive)
				SESSION.tx_delay 	= 80; // delay between 2 consecutive send (adaptive)
				SESSION.time_out 	= 0;

				// pro_tx(SESSION);

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
		}
	}

#if DEBUG_INFO == 1		// ----------------------------------------
	debug_print();
#endif
	free (SESSION.frame_data);
}


