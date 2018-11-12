// =================================================================================================================
// These are the return values of the RF API.
// =================================================================================================================
typedef enum at86rfx_retval_tag
{
	AT86RFX_SUCCESS = 0x00,
	AT86RFX_FAILURE = 0x01,
	AT86RFX_CHANNEL_ACCESS_FAILURE = 0x02
} SHORTENUM at86rfx_retval_t;



// *******************************************************************************************
// Function: 
//		at86rfx_retval_t at86rfx_init(void)
// 
// Description:
//		Initialize Transceiver and RPi module (top level)
// 
// Parameters:
//		None 
//
// Return:
//		AT86RFX_SUCCESS
//		AT86RFX_FAILURE
// *******************************************************************************************
at86rfx_retval_t at86rfx_init(void);


// *******************************************************************************************
// Function: 
//		void at86rfx_tx_frame(unsigned char * frame_tx)
// 
// Description:
//		This function configures the tranceiver in TX mode and transmits the frame
// 
// Parameters:
//		frame_tx - Pointer to data to be transmitted 
//
// Return:
//		None
// *******************************************************************************************
void at86rfx_tx_frame(unsigned char * frame_tx);


// *******************************************************************************************
// Function: 
//		void at86rfx_task(void)
// 
// Description:
//		RF task handling
//		- Checks and calls back with received packet
//		- Processes the TAL incoming frame queue
//		- Calls the TAL state machine handling
// 
// Parameters:
//		None 
//
// Return:
//		None
// *******************************************************************************************
void at86rfx_task(void);


