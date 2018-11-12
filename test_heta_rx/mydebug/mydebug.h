#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>

#define DEBUG_SESS_SIZE	(1024)

// *******************************************************************************************
#define DEBUG_USED_CHECK		(1)	// 1: use CHECK command: SEND -> CHECK -> RESEND -> ...
									// 0: otherwise: SEND -> SEND -> ...
#define DEBUG_USED_REED_SOLOMON	(0) // 1: use error correction
									// 0: otherwise

#define DEBUG_REALTIME			(0)	// 1: real-time with camera
									// 0: send/receive a file. In this case, END_ACK is sent
									// several time to guarantee that SENDER receives properly.
#define DEBUG_INFO				(1) // 1: print the debug information of MYDEBUG
									// 0: otherwise


// *******************************************************************************************
typedef struct debug_t {
	// Count the packets that non-double check
	uint16_t recv_msgid_current;						// Check whether the RX receives data in order or not
	uint16_t recv_msgid_index;							// For example: if order is packet ID: 10 -> 12 -> 13 -> 14
	uint16_t recv_msgid_order_session[DEBUG_SESS_SIZE];	// or 10 -> 14 -> 12 -> 13 ...
	uint32_t recv_msgid_order_total;

	// Count packets different source and destination address
	uint16_t src_dest_addr_index;
	uint16_t src_dest_addr_session[DEBUG_SESS_SIZE];		// Total loss packets in one session
	uint32_t src_dest_addr_total;

	// Count the loss packets
	uint16_t loss_msg_index;
	uint16_t loss_msg_session[DEBUG_SESS_SIZE];		// Total loss packets in one session
	uint32_t loss_msg_total;						// Total loss packets

	// Count the CMD_RX_ON_BUSY
	uint16_t crob_index;
	uint16_t crob_session[DEBUG_SESS_SIZE];
	uint32_t crob_total;

	// Count the CMD_PLL_ON_BUSY
	uint16_t cpob_index;
	uint16_t cpob_session[DEBUG_SESS_SIZE];
	uint32_t cpob_total;

	// Count the packets that have invalid CRC
	uint16_t crc_invalid_index;
	uint16_t crc_invalid_session[DEBUG_SESS_SIZE];	// Total packets in one session that have invalid CRC
	uint32_t crc_invalid_total;						// Total packets in the experiment

	// Count the packets that have invalid frame length
	uint16_t flen_invalid_index;
	uint16_t flen_invalid_session[DEBUG_SESS_SIZE];
	uint32_t flen_invalid_total;

	// Execution time
	time_t timer_start;
	time_t timer_moment;
	double timer_second;
} debug_t;

debug_t MYDEBUG;


// *******************************************************************************************
// Function:
//		void debug_init()
//
// Description:
//		Initialize the debug parameters
//
// Parameters:
//		None
//
// Return:
//		None
//
// *******************************************************************************************
void debug_init();


// *******************************************************************************************
// Function:
//		void debug_print()
//
// Description:
//		Display the debug results
//
// Parameters:
//		None
//
// Return:
//		None
//
// *******************************************************************************************
void debug_print();
