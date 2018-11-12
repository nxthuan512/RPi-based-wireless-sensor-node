#include "../app_rpi_img/rpi_img.h"
#include "../at86rf212_param.h"
#include "../hal/hal_config_wiringpi.h"
#include "../tal/tal_at86rf212.h"
#include "../protocol/protocol.h"
#include "../utils/utils.h"
#include "../mydebug/mydebug.h"


pthread_mutex_t app_recv_done_mutex;
pthread_cond_t app_recv_done_cond;

// ===========================================================
//
// RX app
//
// ===========================================================
void app_rpi_img_recv_store_data(node_t NODE)
{
	uint8_t i;
	sess_t SESSION;
	pthread_t tid[2];


	printf("Info: --- ============================================ \n");
	printf("Info: --- Receiving image data ... \n");
	printf("Info: --- ============================================ \n");

	// ------ Initialize DEBUG  ------
#if DEBUG_INFO == 1
	debug_init();
#endif

	at86rfx_frame_rx = false;

	// ------ Initialize SESSION information  ------
	SESSION.frame_data = (uint8_t*) calloc (FRAME_SIZE, sizeof(uint8_t));
	if (SESSION.frame_data == NULL)
	{
		printf("Info: --- Not enough memory to store data file ... \n");
		exit (1);
	}
	printf("Info: --- Read image data from file ... \n");

	SESSION.frame_length = 0;
	SESSION.packet_length = 0;
	SESSION.num_of_packet = 0;
	SESSION.src_addr = NODE.src_addr;
	SESSION.dest_addr = NODE.dest_addr;
	SESSION.window_size = PACKETS_PER_TRANS;
	SESSION.tx_delay = 0;
	SESSION.time_out = 0;
	SESSION.guarantee_end = false;	// it is set to true in PING command,
									// i.e., END is sent to TX perfectly

	// ------ Initialize THREAD  ------
	pthread_mutex_init(&app_recv_done_mutex, NULL);
	pthread_cond_init (&app_recv_done_cond, NULL);

	printf("Debug: --- Create thread to receive data\n");
	pthread_create(&tid[0], NULL, app_rpi_img_recv_data, &SESSION);
	printf("Debug: --- Create thread to store data\n");
	pthread_create(&tid[1], NULL, app_rpi_img_store_data, &SESSION);

	// 0 will make time-out and exit -> 1 and 2 catch time-out and exit

	// Wait for all threads to complete
	for (i = 0; i < 2; ++i)
		pthread_join(tid[i], NULL);

#if DEBUG_INFO == 1		// ----------------------------------------
	debug_print();
#endif

	free(SESSION.frame_data);

	// Clean up and destroy
	pthread_mutex_destroy(&app_recv_done_mutex);
	pthread_cond_destroy(&app_recv_done_cond);

	pthread_exit(NULL);
}


// ===========================================================
//
// Receive image from TX
//
// ===========================================================
void* app_rpi_img_recv_data(void *arg)
{
	sess_t *SESSION;

	// Initialization
	SESSION = (sess_t *)arg;


	while (SESSION->time_out < SESS_TIME_OUT)
	{
		// ------ Run SESSION ------
		printf("Debug: --- Session %d\n", MYDEBUG.loss_msg_index);
		pro_rx(SESSION);

		pthread_mutex_lock(&app_recv_done_mutex);
		if ((SESSION->guarantee_end == true) || (SESSION->time_out >= SESS_TIME_OUT))
		{
			// Waiting thread when condition is reached.
			pthread_cond_signal(&app_recv_done_cond);

			if (SESSION->guarantee_end == true)
			{
				SESSION->guarantee_end = false;

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
		}
		pthread_mutex_unlock(&app_recv_done_mutex);
	}

	printf("Debug: --- Time-out, exit app_recv_data()\n");
	pthread_exit(NULL);
}


// ===========================================================
//
// Store image to file
//
// ===========================================================
void* app_rpi_img_store_data(void *arg)
{
	char cmd[256];
	char cmd_sub[32];
	uint16_t sess_fram_length;
	uint8_t *sess_frame_data;
	sess_t *SESSION;

	// Initialize
	SESSION = (sess_t *) arg;

	// Lock mutex and wait for signals
	pthread_mutex_lock(&app_recv_done_mutex);
	while (SESSION->time_out < SESS_TIME_OUT)
	{
		pthread_cond_wait(&app_recv_done_cond, &app_recv_done_mutex);

		// In case app_recv_data() is halt due to time-out
		if (SESSION->time_out < SESS_TIME_OUT)
		{
			sess_frame_data = &SESSION->frame_data[0];
			sess_fram_length = SESSION->frame_length;

			// Make the command
			strcpy(cmd, "/home/pi/my_code/tmp/data/img");

			int2str(MYDEBUG.recv_msgid_index, cmd_sub, 10);
			strcat(cmd, cmd_sub);
			strcat(cmd, ".jpg");

			printf("Debug: --- Store to %s, %d bytes\n", cmd, sess_fram_length);
			writeArrayToBinaryFile (cmd, sess_frame_data, sess_fram_length);
		}
	}
	pthread_mutex_unlock(&app_recv_done_mutex);

	printf("Debug: --- Time-out, exit app_store_data()\n");
	pthread_exit(NULL);
}
