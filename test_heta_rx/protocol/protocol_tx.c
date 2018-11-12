#include "../at86rf212_param.h"
#include "../tal/tal_at86rf212.h"
#include "../tal/tal_at86rf212_trx.h"
#include "../hal/hal_at86rf212_trx_access.h"
#include "../mydebug/mydebug.h"
#include "protocol.h"


// *********************************************************************************************************************************
// ===========================================================
//
// Generate the command
//
// ===========================================================
inline void generate_command(msg_t SAR_MSG, uint8_t *cmd_data, uint8_t *msg)
{
	uint8_t i = 0;
	
	// Generate Command ID
	msg[1] = SAR_MSG.cmd_header; 			// If the command is ACK command, add ISACK_PREFIX
	
	// Add source and destination address
	GET16TO8(msg[2], msg[3], SAR_MSG.src_addr);
	GET16TO8(msg[4], msg[5], SAR_MSG.dest_addr);

	// Add Command parameters (if any)
	i = CPARSP + 1;
	if (SAR_MSG.cmd_param_length > 0)
	{
		memcpy(&msg[i], &SAR_MSG.cmd_param[0], SAR_MSG.cmd_param_length);
		i += SAR_MSG.cmd_param_length;
	}
	
	// Add Command data
	if (SAR_MSG.cmd_data_length > 0)
	{
		memcpy(&msg[i], &cmd_data[0], SAR_MSG.cmd_data_length);
		i += SAR_MSG.cmd_data_length;
	}	
	
	// Add packet ID in the end of SEND message
	if ((SAR_MSG.cmd_header & SEND) == SEND)
	{
		msg[i] 	= SAR_MSG.cmd_param[0];
		msg[i + 1] = SAR_MSG.cmd_param[1];
		i += 2;
	}
	
	// The first byte is the length of raw data in packet
	// FCS_LEN = 2: 2-byte of hardware CRC-16
	msg[0] = i + FCS_LEN - 1;
}


// *********************************************************************************************************************************
// ===========================================================
//
// Send the CMD which has ACK (PING, CONFIG, START, END) 
//
// ===========================================================
void pro_tx_send_cmd_recv_ack(pro_fsm PRO_STATE, msg_t SAR_MSG, sess_t *SESSION, uint8_t *msg_recv)
{
	uint16_t i;
	uint16_t src_addr_recv, dest_addr_recv;
	int32_t local_time_out;
	uint8_t cmd_prefix, ack_recv, cmd_length_recv;
	uint8_t msg_send[LARGE_BUFFER_SIZE];

	// ------------- Generate command -------------
	SAR_MSG.cmd_param_length = 0;
	SAR_MSG.cmd_data_length = 0;
	SAR_MSG.cmd_header = PRO_STATE;		// default for PING, START, END

	if (PRO_STATE == CHECK) {
		SAR_MSG.cmd_header |= CHECK_CPL;	// has 2 parameters
		SAR_MSG.cmd_param_length = (CHECK_CPL << 1);
	}		

	else if (PRO_STATE == CONFIG) {
		SAR_MSG.cmd_header |= CONFIG_CPL;	// has 3 parameters
		SAR_MSG.cmd_param_length = (CONFIG_CPL << 1);
	}

	generate_command(SAR_MSG, NULL, &msg_send[0]);

	// ------------- Send, wait, and check ACK -------------
	local_time_out = TIME_OUT_1;
	ack_recv = false;
	
	// Start main loop

	while ((SESSION->time_out < SESS_TIME_OUT) && (ack_recv == false))
	{
		// Send the command
		if (local_time_out == TIME_OUT_1)
		{
			// Send the command			
			at86rfx_tx_frame(&msg_send[0]);
			handle_tal_state();
		}
		
		// Wait for reply
		else if (IRQ_VALUE() == true)	
		{
			trx_irq_handler_cb();

			// If data are already stored
			if (at86rfx_frame_rx == true)
			{
				at86rfx_frame_rx = false;
				cmd_length_recv = at86rfx_rx_buffer[0] - FCS_LEN;
				memcpy(&msg_recv[0], &at86rfx_rx_buffer[1], cmd_length_recv);

				// Check whether ACK, source, and destination addresses are correct
				src_addr_recv = (msg_recv[1] << 8) + msg_recv[2];
				dest_addr_recv = (msg_recv[3] << 8) + msg_recv[4];
				
				if (((msg_recv[0] & ISACK_PREFIX) == ISACK_PREFIX) &&
					(src_addr_recv == SAR_MSG.dest_addr) &&
					(dest_addr_recv == SAR_MSG.src_addr))
				{											
					// 0x38 <-> 00 111 000: mask at Command prefix
					cmd_prefix = msg_recv[0] & CMD_PREFIX_MASK;
					if (cmd_prefix == PRO_STATE)
					{
						// Clear the system time-out
						SESSION->time_out = 0;

						local_time_out = TIME_OUT_1 - 1;
						ack_recv = true;
					}
				}				
			}
		}	// no need to wait for IRQ_VALUE goes to 0 because the int32_t code above
		//
		hal_delay_us(SESS_WAIT_SEND);
		if (local_time_out == TIME_OUT_1)
			local_time_out = 0;
		else
			local_time_out += SESS_WAIT_SEND;
		

		// System time-out, if time-out reaches, halt the program
		SESSION->time_out += SESS_WAIT_RECV;
	}
}


// ===========================================================
//
// Send image data
//
// ===========================================================
void pro_tx_send_data(msg_t SAR_MSG, sess_t SESSION, uint16_t send_pktid)
{
	uint16_t frame_index;
	uint8_t msg_send[LARGE_BUFFER_SIZE];

	// Initialize SAR
	SAR_MSG.cmd_header = SEND | SEND_CPL;	// has 1 parameter
	SAR_MSG.cmd_param_length = (SEND_CPL << 1);
	SAR_MSG.cmd_data_length = SESSION.packet_length;

	frame_index = send_pktid * SESSION.packet_length;
	// Send data
	do {
		GET16TO8(SAR_MSG.cmd_param[0], SAR_MSG.cmd_param[1], send_pktid);
		// Make command
		generate_command(SAR_MSG, &SESSION.frame_data[frame_index], &msg_send[0]);

		// Send command
		at86rfx_tx_frame(&msg_send[0]);
		handle_tal_state();
		PTX_SEND_WAIT(SESSION.tx_delay);

		////// Debug only ///////
		// printf("Debug: --- --- --- --- Send data from position of %d\n", send_pktid);
		/////////////////////////

		--SESSION.window_size;
		if (SESSION.window_size > 0)
		{
			++send_pktid;
			frame_index += SESSION.packet_length;
		}
	} while (SESSION.window_size > 0);
}


// ===========================================================
//
// Re-send image data
//
// ===========================================================
void pro_tx_resend_data(msg_t SAR_MSG, sess_t SESSION, scrp_t RECV_TAB)
{
	uint16_t i, j, k, n;
	uint16_t send_pktid;
	uint16_t frame_index;
	uint8_t msg_send[LARGE_BUFFER_SIZE];

	// Initialize SAR
	SAR_MSG.cmd_header = SEND | SEND_CPL;	// has 1 parameter
	SAR_MSG.cmd_param_length = (SEND_CPL << 1);
	SAR_MSG.cmd_data_length = SESSION.packet_length;

	// printf("RECV_TAB.length = %d:\n", RECV_TAB.length);
	// for (i = 0; i < (SESSION.window_size >> 3); ++i)
	//	printf("%x ", RECV_TAB.table[i]);
	// printf("\n");

	// Read table and re-send data
	i = 0;
	do {
		n = RECV_TAB.table[i];
		// If there is any loss packets
		if (n != 0xFF)
		{
			// Calculate the packet ID
			j = 0;
			k = i << 3;
			send_pktid = 0;
			do {
				if ((n & 0x01) == 0)
				{
					send_pktid = RECV_TAB.pktid_update + k + j;
					// Check the condition:
					// For example: if there is 28 packets left -> table will be ff ff ff f0
					// We only count ff ff ff f
					if (send_pktid < SESSION.num_of_packet)
					{
						GET16TO8(SAR_MSG.cmd_param[0], SAR_MSG.cmd_param[1], send_pktid);
						frame_index = send_pktid * SESSION.packet_length;
						// Make command
						generate_command(SAR_MSG, &SESSION.frame_data[frame_index], &msg_send[0]);
						// Send command
						at86rfx_tx_frame(&msg_send[0]);
						handle_tal_state();
						PTX_SEND_WAIT(SESSION.tx_delay);

#if DEBUG_INFO == 1		// ----------------------------------------
						// printf("Debug: --- --- --- --- Re-send data from position of %d\n", send_pktid);
						++MYDEBUG.loss_msg_session[MYDEBUG.loss_msg_index];
#endif

					}
				}
				n >>= 1;
				++j;
			} while ((j < 8) && (send_pktid < SESSION.num_of_packet));
		}
		++i;

	} while (i < RECV_TAB.length);
}


// ===========================================================
//
// Protocol for send progress
//
// ===========================================================
void pro_tx(sess_t *SESSION)
{
	// SAR message
	msg_t SAR_MSG;
	scrp_t RECV_TAB;	// Send Check Re-send (SCR)
	pro_fsm PRO_STATE;

	uint16_t i, tmp_length;
	uint8_t msg_recv[LARGE_BUFFER_SIZE];
	uint16_t packet_length_ack, frame_length_ack, num_of_packet_ack;
	uint16_t send_pktid, chk_pktid_start, chk_pktid_end;	// send and check packet ID (start, end)
	

	// Initialization
	SAR_MSG.src_addr = SESSION->src_addr;
	SAR_MSG.dest_addr = SESSION->dest_addr;
	PRO_STATE = PING;
	send_pktid = 0;
	chk_pktid_start = 0;
	chk_pktid_end = 0;
	RECV_TAB.pktid_base = 0;
	RECV_TAB.reset_req = 0;


	while ((SESSION->time_out < SESS_TIME_OUT) && (PRO_STATE != HALT))
	{
		switch (PRO_STATE) {

			// ---------- Send PING and wait for PING_ACK ----------
			case PING:
				printf("Info: --- --- --- Send PING ... \n");
				pro_tx_send_cmd_recv_ack(PING, SAR_MSG, SESSION, &msg_recv[0]);
				PRO_STATE = CONFIG;
				break;

			// ---------- Send CONFIG and wait for CONFIG_ACK ----------
			case CONFIG:
				printf("Info: --- --- --- Send CONFIG ... \n");
				printf("Debug: --- --- --- --- Frame length = %d, packet length = %d, Number of packets = %d\n", SESSION->frame_length, SESSION->packet_length, SESSION->num_of_packet);
				// Put frame_length, packet_length, and num_of_packet to cmd_param in SAR message.
				GET16TO8(SAR_MSG.cmd_param[0], SAR_MSG.cmd_param[1], SESSION->frame_length);
				GET16TO8(SAR_MSG.cmd_param[2], SAR_MSG.cmd_param[3], SESSION->packet_length);
				GET16TO8(SAR_MSG.cmd_param[4], SAR_MSG.cmd_param[5], SESSION->num_of_packet);

				do 	{
						pro_tx_send_cmd_recv_ack(CONFIG, SAR_MSG, SESSION, &msg_recv[0]);

						if (SESSION->time_out < SESS_TIME_OUT)
						{
							// Check whether configuration parameters are correct
							frame_length_ack  = (msg_recv[CPARSP] << 8)     + msg_recv[CPARSP + 1];
							packet_length_ack = (msg_recv[CPARSP + 2] << 8) + msg_recv[CPARSP + 3];
							num_of_packet_ack = (msg_recv[CPARSP + 4] << 8) + msg_recv[CPARSP + 5];
						}
					} while ((SESSION->time_out < SESS_TIME_OUT) &&
							 ((SESSION->frame_length  != frame_length_ack) ||
							 (SESSION->packet_length != packet_length_ack) ||
							 (SESSION->num_of_packet != num_of_packet_ack)));

				PRO_STATE = START;
				break;

			// ---------- Send START and wait for START ACK ----------
			case START:
				printf("Info: --- --- --- Send START ... \n");
				pro_tx_send_cmd_recv_ack(START, SAR_MSG, SESSION, &msg_recv[0]);
				PRO_STATE = SEND;
				break;

			// ---------- Send SEND command ----------
			case SEND:
				if (send_pktid < SESSION->num_of_packet)
				{
					printf("Info: --- --- --- Send SEND ... \n");
					if ((send_pktid + SESSION->window_size) > SESSION->num_of_packet)
						SESSION->window_size = SESSION->num_of_packet - send_pktid;


					tmp_length = SESSION->window_size >> 3;
					if ((SESSION->window_size % 8) != 0)
						++tmp_length;

					pro_tx_send_data(SAR_MSG, *SESSION, send_pktid);

					chk_pktid_start = send_pktid;
					chk_pktid_end += SESSION->window_size;
#if DEBUG_USED_CHECK == 1
					PRO_STATE = CHECK;
#else
					PRO_STATE = SEND;
#endif
				}
				else
					PRO_STATE = END;
				break;

			// ---------- Send CHECK command ----------
			case CHECK:
				printf("Info: --- --- --- Send CHECK ... \n");
				printf("Debug: --- --- --- --- Packet ID start = %d, packet ID end = %d ... \n", chk_pktid_start, chk_pktid_end);
				GET16TO8(SAR_MSG.cmd_param[0], SAR_MSG.cmd_param[1], chk_pktid_start);	// RECV_TAB.pktid_base = chk_pktid_start
				GET16TO8(SAR_MSG.cmd_param[2], SAR_MSG.cmd_param[3], chk_pktid_end);
				SAR_MSG.cmd_param_length = (CHECK_CPL << 1);

				do {
					pro_tx_send_cmd_recv_ack(CHECK, SAR_MSG, SESSION, &msg_recv[0]);

					if (SESSION->time_out < SESS_TIME_OUT)
					{
						RECV_TAB.pktid_update = (msg_recv[CPARSP] << 8)     + msg_recv[CPARSP + 1];
						RECV_TAB.length 	= (msg_recv[CPARSP + 2] << 8) + msg_recv[CPARSP + 3];
						printf("Debug: --- --- --- --- Packet ID update = %d, table length = %d ... \n", RECV_TAB.pktid_update, RECV_TAB.length);
					}
				} while ((SESSION->time_out < SESS_TIME_OUT) &&
						 ((RECV_TAB.pktid_update < chk_pktid_start) ||
						 (RECV_TAB.pktid_update > chk_pktid_end) ||
						 (RECV_TAB.length > tmp_length)));








				// Check with system time-out
				if (SESSION->time_out < SESS_TIME_OUT)
				{
					// If there is any error, move to RESEND
					if (RECV_TAB.length > 0)
					{
						memcpy(&RECV_TAB.table[0], &msg_recv[CPARSP + 4], RECV_TAB.length);
						PRO_STATE = RESEND;
					}
					else
					{
						send_pktid += SESSION->window_size;
						PRO_STATE = SEND;
					}

					printf("Debug: --- --- --- --- Packet ID update = %d, table length = %d\n", RECV_TAB.pktid_update, RECV_TAB.length);
					for (i = 0; i < RECV_TAB.length; ++i)
						printf("%x ", RECV_TAB.table[i]);
					printf("\n");
				}





				break;

			// ---------- Send RESEND command ----------
			case RESEND:
				printf("Info: --- --- --- Send RESEND ... \n");
				pro_tx_resend_data(SAR_MSG, *SESSION, RECV_TAB);
				PRO_STATE = CHECK;
				break;

			// ---------- Send END command ----------
			case END:
				printf("Info: --- --- --- Send END ... \n");
				pro_tx_send_cmd_recv_ack(END, SAR_MSG, SESSION, &msg_recv[0]);
				PRO_STATE = HALT;
				break;

			// --------------------------------------
			default:
				break;

		} // switch (PRO_STATE)


	}
}
