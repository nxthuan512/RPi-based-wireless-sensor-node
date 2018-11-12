#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>


#define TRX_ENABLE 		(0)	// 0: This module is TX
							// 1: This module is RX
#define FRAME_SIZE		(57344)	// The size of each SESSION frame

// *******************************************************************************************
#define NODE_00_ADDR	(0x1234)
#define NODE_01_ADDR	(0x5678)

// -------- Node --------
typedef struct node_t {
	uint16_t	src_addr;		// source address
	uint16_t	dest_addr;		// destination address
} node_t;


// *******************************************************************************************
// Function:
//		void app_rpi_img_init()
//
// Description:
//		Initialize the system
//
// Parameters:
//		None
//
// Return:
//		None
//
// *******************************************************************************************
void app_rpi_img_init();


// *******************************************************************************************
// Function:
//		void app_rpi_img_send_data(node_t NODE)
//
// Description:
//		Read image from file and send to RX
//
// Parameters:
//		NODE		- Node information
//
// Return:
//		None
//
// *******************************************************************************************
void app_rpi_img_send_data(node_t NODE);


// *******************************************************************************************
// Function:
//		void app_rpi_img_recv_store_data(node_t NODE)
//
// Description:
//		Receive image from TX
//
// Parameters:
//		NODE		- Node information
//
// Return:
//		None
//
// *******************************************************************************************
void app_rpi_img_recv_store_data(node_t NODE);


// *******************************************************************************************
// Function:
//		void* app_rpi_img_recv_data(void *arg)
//
// Description:
//		Receive image from TX
//
// Parameters:
//		SESSION	- Session information
//
// Return:
//		None
//
// *******************************************************************************************
void* app_rpi_img_recv_data(void *arg);


// *******************************************************************************************
// Function:
//		void* app_rpi_img_store_data(void *arg)
//
// Description:
//		Store image to file
//
// Parameters:
//		SESSION	- Session information
//
// Return:
//		None
//
// *******************************************************************************************
void* app_rpi_img_store_data(void *arg);


