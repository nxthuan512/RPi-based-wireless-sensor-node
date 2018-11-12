// *******************************************************************************************
// Variables
// *******************************************************************************************
// Current state of the transceiver
static tal_trx_status_t tal_trx_status;		
// Current state of the TAL state machine
static tal_state_t tal_state;
// Keep track of transceiver status
static trx_trac_status_t trx_trac_status;
// Static receive buffer that can be used to upload a frame from the trx.
extern unsigned char at86rfx_rx_buffer[LARGE_BUFFER_SIZE];
// at86rfx_frame_rx = true: received data
extern unsigned char at86rfx_frame_rx;


// *******************************************************************************************
// Function: 
//		trx_retval_t tal_init(void)
// 
// Description:
//		Initialize Transceiver Abstraction Layer (TAL)
// 
// Parameters:
//		None 
//
// Return:
//		TRX_SUCCESS
//		TRX_FAILURE
// *******************************************************************************************
trx_retval_t tal_init(void);


// *******************************************************************************************
// Function: 
//		static trx_retval_t trx_init(void)
// 
// Description:
//		Initialize RF module after power on
// 
// Parameters:
//		None 
//
// Return:
//		TRX_SUCCESS
//		TRX_FAILURE
// *******************************************************************************************
static trx_retval_t trx_init(void);


// *******************************************************************************************
// Function: 
//		static trx_retval_t internal_tal_reset(void)
// 
// Description:
//		Internal TAL reset function
// 
// Parameters:
//		None 
//
// Return:
//		TRX_SUCCESS - the transceiver was successfully resetted
//		TRX_FAILURE - otherwise
// *******************************************************************************************
static trx_retval_t internal_tal_reset(void);


// *******************************************************************************************
// Function: 
//		static trx_retval_t trx_reset(void)
// 
// Description:
//		Reset transceiver
// 
// Parameters:
//		None 
//
// Return:
//		TRX_SUCCESS - the transceiver state is changed to TRX_OFF
//		TRX_FAILURE - otherwise
// *******************************************************************************************
static trx_retval_t trx_reset(void);


// *******************************************************************************************
// Function: 
//		static void trx_config(void)
// 
// Description:
//		This function is called to configure the transceiver after reset
// 
// Parameters:
//		None 
//
// Return:
//		None
// *******************************************************************************************
static void trx_config(void);


// *******************************************************************************************
// Function: 
//		static void generate_rand_seed(void)
// 
// Description:
//		Generates a 16-bit random number used as initial seed for srand()
//		This function generates a 16-bit random number by means of using the
// 		Random Number Generator from the transceiver.
// 		The Random Number Generator generates 2-bit random values. These 2-bit
// 		random values are concatenated to the required 16-bit random seed.
// 		For further information please check the SWPM AT86RF212.
//
// 		The generated random 16-bit number is feed into function srand()
// 		as initial seed.
//
// 		The transceiver state is initally set to RX_ON.
// 		After the completion of the random seed generation, the
// 		trancseiver is set to TRX_OFF.
//
// 		As a prerequisite the Preamble Detector must not be disabled.
// 		Since this function is called right after trx_reset(), the Preamble
// 		Detector has its original value, i.e. it is enabled.
// 		In case this function is used at a different point of time, having the
// 		proper value set must be checked additionally.
//
// 		Also in case the function is called from a different state than TRX_OFF,
// 		additional trx state handling is required, such as reading the original
// 		value and restoring this state after finishing the sequence.
// 		Since in our case the function is called from TRX_OFF, this is not required
// 		here.
// 
// Parameters:
//		None 
//
// Return:
//		None
// *******************************************************************************************
static void generate_rand_seed(void);


// *******************************************************************************************
// Function: 
//		static tal_trx_status_t set_trx_state(trx_cmd_t trx_cmd)
// 
// Description:
//		Sets transceiver state
// 
// Parameters:
//		trx_cmd	-  needs to be one of the trx commands
//
// Return:
//		Current trx state
// *******************************************************************************************
static tal_trx_status_t set_trx_state(trx_cmd_t trx_cmd);


// *******************************************************************************************
// Function: 
//		static void switch_pll_on(void)
// 
// Description:
//		Switches the PLL on
// 
// Parameters:
//		None
//
// Return:
//		None
// *******************************************************************************************
static void switch_pll_on(void);


// *******************************************************************************************
// Function: 
//		void trx_irq_handler_cb(void)
// 
// Description:
//		Transceiver interrupt handler
//		This function handles the transceiver generated interrupts
// 
// Parameters:
//		None
//
// Return:
//		None
// *******************************************************************************************
void trx_irq_handler_cb(void);


// *******************************************************************************************
// Function: 
//		void tx_frame_config(void)
// 
// Description:
//		Configures the transceiver for frame transmission
// 
// Parameters:
//		None
//
// Return:
//		None
// *******************************************************************************************
void tx_frame_config(void);


// *******************************************************************************************
// Function: 
//		static void tx_end_handling(void)
// 
// Description:
//		Implements the handling of the transmission end
//		This function handles the callback for the transmission end
// 
// Parameters:
//		None 
//
// Return:
//		None
// *******************************************************************************************
static void tx_end_handling(void);


// *******************************************************************************************
// Function: 
//		static void handle_tal_state(void)
// 
// Description:
//		Handles the transceiver state
// 
// Parameters:
//		None 
//
// Return:
//		None
// *******************************************************************************************
void handle_tal_state(void);

