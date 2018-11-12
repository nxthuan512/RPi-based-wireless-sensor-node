#include <stdio.h>

#include "mydebug.h"


// ===========================================================
//
// Initialize the debug parameters
//
// ===========================================================
void debug_init()
{
	// Non-order messages
	MYDEBUG.recv_msgid_current = 0;
	MYDEBUG.recv_msgid_index = 0;
	MYDEBUG.recv_msgid_order_total = 0;
	memset(MYDEBUG.recv_msgid_order_session, 0, DEBUG_SESS_SIZE);

	// Loss message
	MYDEBUG.src_dest_addr_index = 0;
	MYDEBUG.src_dest_addr_total = 0;
	memset(MYDEBUG.src_dest_addr_session, 0, DEBUG_SESS_SIZE);

	// Loss message
	MYDEBUG.loss_msg_index = 0;
	MYDEBUG.loss_msg_total = 0;
	memset(MYDEBUG.loss_msg_session, 0, DEBUG_SESS_SIZE);

	// CMD_RX_ON: BUSY: TRX status = 1
	MYDEBUG.crob_index = 0;
	MYDEBUG.crob_total = 0;
	memset(MYDEBUG.crob_session, 0, DEBUG_SESS_SIZE);

	// CMD_PLL_ON: BUSY: TRX status = 1
	MYDEBUG.cpob_index = 0;
	MYDEBUG.cpob_total = 0;
	memset(MYDEBUG.cpob_session, 0, DEBUG_SESS_SIZE);

	// ------ Invalid CRC  ------
	MYDEBUG.crc_invalid_index = 0;
	MYDEBUG.crc_invalid_total = 0;
	memset(MYDEBUG.crc_invalid_session, 0, DEBUG_SESS_SIZE);

	// ------ Invalid frame length  ------
	MYDEBUG.flen_invalid_index = 0;
	MYDEBUG.flen_invalid_total = 0;
	memset(MYDEBUG.flen_invalid_session, 0, DEBUG_SESS_SIZE);

	// Execution time
	time(&MYDEBUG.timer_start);
}


// ===========================================================
//
// Display the debug results
//
// ===========================================================
void debug_print()
{
	register uint32_t i;

	// The total execution time
	time(&MYDEBUG.timer_moment);
	MYDEBUG.timer_second = difftime(MYDEBUG.timer_moment, MYDEBUG.timer_start);
	printf("Info: --- Execution time %.f seconds\n", MYDEBUG.timer_second);

	// Number of non-order messages
	printf("Debug: --- Total non-order messages: %d\n", MYDEBUG.recv_msgid_order_total);
	for (i = 0; i < MYDEBUG.recv_msgid_index; ++i)
	{
		if (MYDEBUG.recv_msgid_order_session[i] > 0)
			printf("Debug: --- --- Session - Non-order messages: %d - %d\n", i, MYDEBUG.recv_msgid_order_session[i]);
	}

	// Number of loss messages
	printf("Debug: --- Total Source different Destination: %d\n", MYDEBUG.src_dest_addr_total);
	for (i = 0; i < MYDEBUG.src_dest_addr_index; ++i)
	{
		if (MYDEBUG.src_dest_addr_session[i] > 0)
			printf("Debug: --- --- Session - Source different Destination: %d - %d\n", i, MYDEBUG.src_dest_addr_session[i]);
	}

	// Number of loss messages
	printf("Debug: --- Total loss messages: %d\n", MYDEBUG.loss_msg_total);
	for (i = 0; i < MYDEBUG.loss_msg_index; ++i)
	{
		if (MYDEBUG.loss_msg_session[i] > 0)
			printf("Debug: --- --- Session - Loss messages: %d - %d\n", i, MYDEBUG.loss_msg_session[i]);
	}

	// Number of "CMD_RX_ON: BUSY: TRX status = 1"
	printf("Debug: --- Total CMD_RX_ON - BUSY: %d\n", MYDEBUG.crob_total);
	for (i = 0; i < MYDEBUG.crob_index; ++i)
	{
		if (MYDEBUG.crob_session[i] > 0)
			printf("Debug: --- Session - CMD_RX_ON - BUSY: %d - %d\n", i, MYDEBUG.crob_session[i]);
	}

	// Number of "CMD_PLL_ON: BUSY: TRX status = 1"
	printf("Debug: --- Total CMD_PLL_ON - BUSY: %d\n", MYDEBUG.cpob_total);
	for (i = 0; i < MYDEBUG.cpob_index; ++i)
	{
		if (MYDEBUG.cpob_session[i] > 0)
			printf("Debug: --- Session - CMD_PLL_ON - BUSY: %d - %d\n", i, MYDEBUG.cpob_session[i]);
	}

	// Number of packets that have invalid CRC
	printf("Debug: --- Total invalid CRC packets: %d\n", MYDEBUG.crc_invalid_total);
	for (i = 0; i < MYDEBUG.crc_invalid_index; ++i)
	{
		if (MYDEBUG.crc_invalid_session[i] > 0)
			printf("Debug: --- Session - Invalid CRC packets: %d - %d\n", i, MYDEBUG.crc_invalid_session[i]);
	}

	// Number of packets that have invalid frame length
	printf("Debug: --- Total invalid frame length packets: %d\n", MYDEBUG.flen_invalid_total);
	for (i = 0; i < MYDEBUG.flen_invalid_index; ++i)
	{
		if (MYDEBUG.flen_invalid_session[i] > 0)
			printf("Debug: --- Session - Invalid frame length packets: %d - %d\n", i, MYDEBUG.flen_invalid_session[i]);
	}
}
