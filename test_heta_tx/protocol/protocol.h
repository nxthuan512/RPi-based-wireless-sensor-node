#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

// *******************************************************************************************
// Supported commands
#define CMD_PREFIX_MASK	(0x38)	// Get command prefix in command header

// Command header Bit 7
#define ISACK_PREFIX	(0x80)	// Be ACK command or not
// Command header Bit 5 ..3
typedef enum pro_fsm {
	PING 	= 0x00,		// (0x00 << 3)	PING_PREFIX
	CONFIG 	= 0x08,		// (0x01 << 3)	CONFIG_PREFIX
	START 	= 0x10,		// (0x02 << 3)	START_PREFIX
	END 	= 0x18,		// (0x03 << 3)	END_PREFIX
	SEND 	= 0x20,		// (0x04 << 3)	SEND_PREFIX
	CHECK 	= 0x28,		// (0x05 << 3)	CHECK_PREFIX
	RESEND,
	HALT
} pro_fsm;
// Command header Bit 2 .. 0
#define CONFIG_CPL		(0x3)	// 3 parameters, 6 bytes
#define SEND_CPL	 	(0x1)	// 1 parameters, 2 bytes
#define CHECK_CPL 		(0x2)	// 2 parameters, 4 bytes

#define CPARSP			(0x05)	// Command parameter starting position
								// 1-byte cmd, 4-byte src/dest address
// Session parameters
#define PACKETS_PER_TRANS	(128)	// 128 packets/transaction
#define RECV_PACKET_TAB_MAX (256)	// received-data-table, support up to 2,048 packets/transaction
#define SCPL		 		(115)	// 115 bytes/packet
#define MAX_NUM_LOSS_PKTS	(116)	// Maximum number of loss packets ID in one transaction

#define SESS_WAIT_RECV		(100)	// us
#define SESS_WAIT_SEND		(10)	// us
#define SESS_TIME_OUT		(60000000)	// max 60 seconds

// DQIS framework
#define TIME_OUT_1			(SESS_WAIT_RECV*10)
#define PTX_SEND_WAIT(a)	hal_delay_us(a)

#define SAR_DELAY_MIN		(0)
#define SAR_DELAY_MAX		(3200)
#define SAR_DELAY_AVG		(1600)
#define SAR_COEFF_DELAY_INC	(1)		// 1/2
#define SAR_COEFF_DELAY_DEC	(2)		// 1/4
#define SAR_THRESHOLD		(5)

// ------------------
#define GET16TO8(a8, b8, c16) {(b8) = (uint8_t)((c16) & 0xFF); (a8) = (uint8_t)((c16) >> 8);}


// *******************************************************************************************
// -------- Structure of SAR message --------
typedef struct msg_t {
	uint8_t 	cmd_header;
	uint16_t 	src_addr;
	uint16_t	dest_addr;
	uint8_t		cmd_param[14];
	uint8_t		cmd_param_length;
	uint8_t 	cmd_data_length;	// length in byte of cmd_data_length
									// command data are obtained directly from SESSION frame data
} msg_t;

// -------- Session information --------
typedef struct sess_t {
	uint16_t	src_addr;			// source address
	uint16_t	dest_addr;			// destination address
	uint16_t 	frame_length;		// frame length in this session
	uint16_t 	packet_length;		// packet length in this session
	uint16_t 	num_of_packet;		// number of packets in this session
	uint16_t 	window_size;		// the size of window (number of packets/transaction) (adaptive)
	uint16_t	tx_delay;			// delay between 2 consecutive send (adaptive)
	uint32_t	time_out;			// control the session time-out, if occur, halt the system
	uint8_t 	guarantee_end;		// guarantee that END ACK is received properly
	uint8_t		*frame_data;		// frame data in this session
} sess_t;

// -------- Send and re-send protocol --------
// For example, only data at index=3,6,10 is not 0xFF
// -> RX sends smallest packet ID at index=3 (e.g. 24), table length=8 bytes,
// and table from index=3 with length=8 (to index=10)
typedef struct scrp_t {
	uint16_t	pktid_base;			// because the recv_data_table is refresh after each transaction, we need a base
	uint16_t	pktid_update;
	uint16_t	length;				// length of recv_data_table in one transaction
	uint8_t		reset_req;			// 1: reset the table when length = 0, in SEND state
	uint8_t 	table[RECV_PACKET_TAB_MAX];	// store the receive data in one transaction
} scrp_t;


// =========================================================================================================================================
// *******************************************************************************************
// Function: 
//		void generate_command(msg_t SAR_MSG, uint8_t *cmd_data, uint8_t *msg)
// 
// Description:
//		Generate the command
// 
// Parameters:
//		SAR_MSG		- SAR message
//		cmd_data	- Command data which are obtained directly from SESSION frame data
//		msg			- Full message
//
// Return:
//		None
//
// *******************************************************************************************
void generate_command(msg_t SAR_MSG, uint8_t *cmd_data, uint8_t *msg);


// =========================================================================================================================================
// *******************************************************************************************
// Function: 
//		void pro_tx_send_cmd_recv_ack(pro_fsm PRO_STATE, msg_t SAR_MSG, sess_t *SESSION, uint8_t *msg_recv)
// 
// Description:
//		Send the command and try to receive the ACK command
// 
// Parameters:
//		PRO_STATE	- State of command which has ACK
//		SAR_MSG		- SAR message
//		SESSION		- Session information
//		msg_recv	- Full receive message
//
// Return:
//		None
//
// *******************************************************************************************
void pro_tx_send_cmd_recv_ack(pro_fsm PRO_STATE, msg_t SAR_MSG, sess_t *SESSION, uint8_t *msg_recv);


// *******************************************************************************************
// Function: 
//		void pro_tx_send_data(msg_t SAR_MSG, sess_t SESSION, uint16_t send_pktid)
// 
// Description:
//		Send image data
// 
// Parameters:
//		SAR_MSG		- SAR message
//		SESSION		- Session information
//		send_pktid	-
//		msg_send	-
//
// Return:
//		None
//
// *******************************************************************************************
void pro_tx_send_data(msg_t SAR_MSG, sess_t SESSION, uint16_t send_pktid);


// *******************************************************************************************
// Function: 
//		void pro_tx_resend_data(msg_t SAR_MSG, sess_t SESSION, scrp_t RECV_TAB)
//
// Description:
//		Check received-data-table and re-send image data
//
// Parameters:
//		SAR_MSG		-
//		SESSION		- Session information
//		RECV_TAB	- Received data table
//		msg_send	-
//
// Return:
//		None
//
// *******************************************************************************************
void pro_tx_resend_data(msg_t SAR_MSG, sess_t SESSION, scrp_t RECV_TAB);


// *******************************************************************************************
// Function:
//		void pro_tx(sess_t *SESSION)
// 
// Description:
//		Send image data
// 
// Parameters:
//		SESSION		- Session information
//
// Return:
//		None
//
// *******************************************************************************************
void pro_tx(sess_t *SESSION);


// =========================================================================================================================================
// *******************************************************************************************
// Function:
//		uint8_t pro_rx_check_loss(sess_t SESSION, scrp_t *RECV_TAB, uint8_t *msg_recv)
//
// Description:
//		Get the configuration parameters
//
// Parameters:
//		SESSION			- Session information
//		RECV_TAB		- Received-data-table
//		msg_recv		- Full receive message
//
// Return:
//		true or false
//
// *******************************************************************************************
uint8_t pro_rx_check_loss(sess_t SESSION, scrp_t *RECV_TAB, uint8_t *msg_recv);


// *******************************************************************************************
// Function:
//		void pro_rx_recv_cmd_send_ack(pro_fsm *PRO_STATE, sess_t *SESSION, msg_t SAR_MSG, scrp_t *RECV_TAB, uint8_t *msg_recv)
// 
// Description:
//		Send the acknowledge after receive the command
// 
// Parameters:
//		PRO_STATE		- FSM state
//		SESSION			- Session information
//		SAR_MSG			- SAR message
//		RECV_TAB		- Received-data-table
//		msg_recv		- Full receive message
//
// Return:
//		None
//
// *******************************************************************************************
void pro_rx_recv_cmd_send_ack(pro_fsm *PRO_STATE, sess_t *SESSION, msg_t SAR_MSG, scrp_t *RECV_TAB, uint8_t *msg_recv);


// *******************************************************************************************
// Function: 
//		uint8_t pro_rx_recv_data(pro_fsm *PRO_STATE, scrp_t *SCR_PRO, sess_t *SESSION, uint8_t *msg_rev)
// 
// Description:
//		Receive the command
// 
// Parameters:
//		PRO_STATE	- FSM state
//		SCR_PRO		- Send, check, re-send protocol
//		SESSION		- Session information
//		msg_recv	- Full receive message
//
// Return:
//		True/False
//
// *******************************************************************************************
uint8_t pro_rx_recv_data(pro_fsm *PRO_STATE, scrp_t *SCR_PRO, sess_t *SESSION, uint8_t *msg_recv);


// *******************************************************************************************
// Function: 
//		void pro_rx(sess_t *SESSION)
// 
// Description:
//		Receive image data
// 
// Parameters:
//		SESSION		- Session information
//
// Return:
//		None
//
// *******************************************************************************************
void pro_rx(sess_t *SESSION);
