#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

// ********************************************************************************************
// USER CONFIGURATION
// ********************************************************************************************
// Path to image file
#define FRAME_TX 		"/home/pi/my_code/tmp/big_hero_6_720p.mp4"
#define FRAME_RX 		"/home/pi/my_code/tmp/output"
#define APPBUFF_SIZE 	(298331)	// The total size of test file, max:

#define TRX_ENABLE 		(1)	// 0: This module is TX
							// 1: This module is RX
#define FRAME_SIZE		(57344)	// The size of each SESSION frame

// *******************************************************************************************
#define NODE_00_ADDR	(0x1234)
#define NODE_01_ADDR	(0x5678)

// -------- Node --------
typedef struct node_t {
	uint16_t	src_addr;		// source address
	uint16_t	dest_addr;		// destination address
	uint16_t 	sess_window_size;	// the size of window (number of packets/transaction) (adaptive)
	uint16_t	sess_tx_delay;		// delay between 2 consecutive send (adaptive)
} node_t;

// -------- Data buffer --------
typedef struct appbuff_t {
	uint32_t	length;
	uint8_t 	*data;
} appbuff_t;


// *******************************************************************************************
// Function:
//		void app_fixed_data_init(int argc, char **argv, node_t *NODE)
//
// Description:
//		Initialize the system
//
// Parameters:
//		argc	-	from main()
//		argv	- 	from main()
//		NODE	- 	Node information
//
// Return:
//		None
//
// *******************************************************************************************
void app_fixed_data_init(int argc, char **argv, node_t *NODE);


// *******************************************************************************************
// Function: 
//		void app_fixed_data_tx_data(node_t NODE)
// 
// Description:
//		Create the transmission
// 
// Parameters:
//		NODE		- Node information
//
// Return:
//		None
//
// *******************************************************************************************
void app_fixed_data_tx_data(node_t NODE);


// *******************************************************************************************
// Function: 
//		void app_fixed_data_rx_data(node_t NODE)
// 
// Description:
//		Create the transmission
// 
// Parameters:
//		NODE		- Node information
//
// Return:
//		None
//
// *******************************************************************************************
void app_fixed_data_rx_data(node_t NODE);





