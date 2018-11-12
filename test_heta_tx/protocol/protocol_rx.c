#include "../at86rf212_param.h"
#include "../tal/tal_at86rf212.h"
#include "../tal/tal_at86rf212_trx.h"
#include "../hal/hal_at86rf212_trx_access.h"
#include "../mydebug/mydebug.h"
#include "protocol.h"


// ===========================================================
//
// Set the number of loss packets
//
// ===========================================================
uint8_t pro_rx_check_loss(sess_t SESSION, scrp_t *RECV_TAB, uint8_t *msg_recv)
{
	register uint16_t i, j, n;
	uint16_t chk_pktid_start, chk_pktid_end;
	uint16_t min_id, max_id;
	uint16_t result;

	min_id = 0xFFFF;
	max_id = 0;


	// Get the Check packet ID and Check packet ID end
	chk_pktid_start = (msg_recv[CPARSP] << 8)     + msg_recv[CPARSP + 1];
	chk_pktid_end 	= (msg_recv[CPARSP + 2] << 8) + msg_recv[CPARSP + 3];

	// Check the condition
	// Guarantee that chk_pktid_end and chk_pktid_start are smaller than SESSION.num_of_packet
	if ((chk_pktid_end <= SESSION.num_of_packet) && (chk_pktid_end > chk_pktid_start))
	{
		printf("Debug: --- --- --- --- Packet ID start = %d, packet ID end = %d\n", chk_pktid_start, chk_pktid_end);
		// Calculate the new length of table
		j = chk_pktid_end - chk_pktid_start;
		n = j >> 3;
		j = j % 8;		// the corresponding bit
		if (j != 0)
			++n;
		// Check the table
		for (i = 0; i < n; ++i)
		{
			result = 0x00FF;
			if ((i == n-1) && (j != 0))
				result = 0x00FF >> (8 - j);

			if (RECV_TAB->table[i] != (uint8_t)(result))
			{
				// Find the first position
				if (min_id > i)
					min_id = i;

				// Find the last position
				if (max_id < i)
					max_id = i;
			}
		}

		// After check, if min_id = 0xFFFF, i.e. all packets are received properly
		// Clear the RECV_TAB and set new ID base afterward
		if (min_id == 0xFFFF)
		{
			RECV_TAB->pktid_base = chk_pktid_end;
			RECV_TAB->pktid_update = chk_pktid_end;
			RECV_TAB->length = 0;
		}
		else
		{
			RECV_TAB->pktid_base = chk_pktid_start;
			RECV_TAB->pktid_update = chk_pktid_start + (min_id << 3);
			RECV_TAB->length = max_id - min_id + 1;
			// If the table size is larger than the capacity of PHY packets
			if (RECV_TAB->length > MAX_NUM_LOSS_PKTS)
				RECV_TAB->length = MAX_NUM_LOSS_PKTS;
		}

		printf("Debug: --- --- --- --- Packet ID base = %d, packet ID update = %d, table length = %d\n", RECV_TAB->pktid_base, RECV_TAB->pktid_update, RECV_TAB->length);
		for (i = 0; i < (SESSION.window_size >> 3); ++i)
			printf("%x ", RECV_TAB->table[i]);
		printf("\n");

		return false;	// no received error
	}

	else
		return true;
}


// ===========================================================
//
// Receive the CMD, send the ACK
//
// ===========================================================
void pro_rx_recv_cmd_send_ack(pro_fsm *PRO_STATE, sess_t *SESSION, msg_t SAR_MSG, scrp_t *RECV_TAB, uint8_t *msg_recv)
{
	uint16_t i;
	uint8_t cmd_prefix, recv_error;
	uint8_t msg_send[LARGE_BUFFER_SIZE];

	// 0x38 <-> 00 111 000: mask at Command prefix
	cmd_prefix = msg_recv[0] & CMD_PREFIX_MASK;
	recv_error = false;

	// Initialization
	SAR_MSG.cmd_header = ISACK_PREFIX | cmd_prefix;	// default for PING, START, END
	SAR_MSG.cmd_param_length = 0;
	SAR_MSG.cmd_data_length = 0;

	switch (cmd_prefix)
	{
		// ------ CHECK command ------
		case CHECK:
			if ((*PRO_STATE == SEND) || (*PRO_STATE == CHECK))
			{
				*PRO_STATE = CHECK;
				// Initialize the reset request
				RECV_TAB->reset_req = 1;
				// Update the position and length in RECV_TAB
				recv_error = pro_rx_check_loss((*SESSION), RECV_TAB, &msg_recv[0]);

				if (recv_error == false)
				{
					SAR_MSG.cmd_header |= CHECK_CPL;
					SAR_MSG.cmd_param_length = (CHECK_CPL << 1);
					SAR_MSG.cmd_data_length = RECV_TAB->length;
					GET16TO8(SAR_MSG.cmd_param[0], SAR_MSG.cmd_param[1], RECV_TAB->pktid_update);
					GET16TO8(SAR_MSG.cmd_param[2], SAR_MSG.cmd_param[3], RECV_TAB->length);

					printf("Info: --- --- --- Send CHECK acknowledge\n");
					printf("Debug: --- --- --- --- Packet ID update = %d, table length = %d\n", RECV_TAB->pktid_update, RECV_TAB->length);
				}
			}
			break;

		// ------ PING command ------
		case PING:
			if ((*PRO_STATE == END) || (*PRO_STATE == PING))
			{
				*PRO_STATE = PING;
				// Guarantee END ACK is received
				SESSION->guarantee_end = true;
				printf("Info: --- --- --- Send PING acknowledge\n");
			}
			break;

		// ------ CONFIG command ------
		case CONFIG:
			if ((*PRO_STATE == PING) || (*PRO_STATE == CONFIG))
			{
				*PRO_STATE = CONFIG;

				// Get configuration parameters
				SESSION->frame_length =  (msg_recv[CPARSP] << 8) 	 + msg_recv[CPARSP + 1];
				SESSION->packet_length = (msg_recv[CPARSP + 2] << 8) + msg_recv[CPARSP + 3];
				SESSION->num_of_packet = (msg_recv[CPARSP + 4] << 8) + msg_recv[CPARSP + 5];

				// Because TX will check them again, so we do not need to check here

				// Re-send configuration parameters to sender
				SAR_MSG.cmd_header |= CONFIG_CPL;
				SAR_MSG.cmd_param_length = (CONFIG_CPL << 1);
				GET16TO8(SAR_MSG.cmd_param[0], SAR_MSG.cmd_param[1], SESSION->frame_length);
				GET16TO8(SAR_MSG.cmd_param[2], SAR_MSG.cmd_param[3], SESSION->packet_length);
				GET16TO8(SAR_MSG.cmd_param[4], SAR_MSG.cmd_param[5], SESSION->num_of_packet);

				printf("Info: --- --- --- Send CONFIG acknowledge\n");
				printf("Debug: --- --- --- --- Frame length = %d, packet length = %d, number of packets = %d", SESSION->frame_length, SESSION->packet_length, SESSION->num_of_packet);
			}
			break;

		// ------ START command ------
		case START:
			if ((*PRO_STATE == CONFIG) || (*PRO_STATE == START))
			{
				*PRO_STATE = START;
				printf("Info: --- --- --- Send START acknowledge\n");
			}
			break;

		// ------ END command ------
		case END:
			// In case END_ACK command is sent from RX to TX, but TX doesn't receive it yet.
			// TX then sends END command once again, but RX now is in PING state.
			// For this reason, we have to check (PRO_STATE==PING) in this case.
#if DEBUG_USED_CHECK == 1
			if ((*PRO_STATE == CHECK) || (*PRO_STATE == PING) || (*PRO_STATE == END))
#else
			if ((*PRO_STATE == SEND) || (*PRO_STATE == END))
#endif
			{
				*PRO_STATE = END;
				printf("Info: --- --- --- Send END acknowledge\n");
			}
			break;

		// -------------------------
		default:
			break;
	}

	// If there is no error in received data
	if (recv_error == false)
	{
		// Make the command
		if (*PRO_STATE == CHECK)
		{
			i = (RECV_TAB->pktid_update - RECV_TAB->pktid_base) >> 3;
			generate_command(SAR_MSG, &RECV_TAB->table[i], &msg_send[0]);
		}
		else
			generate_command(SAR_MSG, NULL, &msg_send[0]);

		at86rfx_tx_frame(msg_send);
		handle_tal_state();
	}
}


// ===========================================================
//
// Receive image data
//
// ===========================================================
uint8_t pro_rx_recv_data(pro_fsm *PRO_STATE, scrp_t *RECV_TAB, sess_t *SESSION, uint8_t *msg_recv)
{
	uint16_t i, j, frame_index;
	uint16_t recv_pktid, recv_pktid_double;
	uint8_t bit_select;

	*PRO_STATE = SEND;

	// Check the condition to clear the table
	if ((RECV_TAB->length == 0) && (RECV_TAB->reset_req == 1))
	{
		memset(&RECV_TAB->table[0], 0, (SESSION->window_size >> 3));
		RECV_TAB->reset_req = 0;
	}


	// Get the packet id
	recv_pktid = (msg_recv[CPARSP] << 8) + msg_recv[CPARSP + 1];
	// Get the double check packet id
	recv_pktid_double = (msg_recv[SCPL + CPARSP + 2] << 8) + msg_recv[SCPL + CPARSP + 3];

#if DEBUG_INFO == 1
	if (recv_pktid_double != recv_pktid)
		++MYDEBUG.recv_msgid_order_session[MYDEBUG.recv_msgid_index];
	MYDEBUG.recv_msgid_current = recv_pktid;
#endif

	// Check condition and update received-data-table
	if (recv_pktid >= 0 && (recv_pktid_double == recv_pktid))
	{
		j = (recv_pktid - RECV_TAB->pktid_base);
		i = j >> 3;
		bit_select = 0x1 << (j % 8);

		// If this packet is not received yet, update received-data-table
		if ((RECV_TAB->table[i] & bit_select) == 0x0)
		{
			RECV_TAB->table[i] |= bit_select;

			// Copy received data to SESSION frame data
			frame_index = recv_pktid * SESSION->packet_length;
			memcpy(&SESSION->frame_data[frame_index], &msg_recv[CPARSP + 2], SESSION->packet_length);

			// printf("Debug: --- --- --- --- Receive data from position of = %d\n", recv_pktid);
			return (true);
		}
	}
	return (false);
}


// ===========================================================
//
// Send the CMD ACK
//
// ===========================================================
void pro_rx(sess_t *SESSION)
{
	msg_t SAR_MSG;
	scrp_t RECV_TAB;	// Send Check Re-send (SCR)
	pro_fsm PRO_STATE;

	uint8_t i, result, cmd_length;
	uint8_t msg_recv[LARGE_BUFFER_SIZE];
	uint16_t src_addr_recv, dest_addr_recv;
	uint8_t cmd_prefix;

	// Initialization
	result = false;

	SAR_MSG.src_addr = SESSION->src_addr;
	SAR_MSG.dest_addr = SESSION->dest_addr;

	RECV_TAB.pktid_base = 0;
	RECV_TAB.length = 0;
	RECV_TAB.reset_req = 1;

	// Start main loop
	PRO_STATE = PING;

	while ((SESSION->time_out < SESS_TIME_OUT) && (PRO_STATE != HALT))
	{
		if (IRQ_VALUE() == true)
		{
			// If data are already stored
			trx_irq_handler_cb();
			if (at86rfx_frame_rx == true)
			{
				at86rfx_frame_rx = false;
				cmd_length = at86rfx_rx_buffer[0] - FCS_LEN;
				memcpy(&msg_recv[0], &at86rfx_rx_buffer[1], cmd_length);


				// Check whether packet data are corrected
				src_addr_recv = (msg_recv[1] << 8)  + msg_recv[2];
				dest_addr_recv = (msg_recv[3] << 8) + msg_recv[4];

#if DEBUG_INFO == 1
				if ((src_addr_recv != SESSION->dest_addr) || (dest_addr_recv != SESSION->src_addr))
					++MYDEBUG.src_dest_addr_session[MYDEBUG.src_dest_addr_index];
#endif

				if ((src_addr_recv == SESSION->dest_addr) && (dest_addr_recv == SESSION->src_addr))
				{
					// 0x38 <-> 00 111 000: mask at Command prefix
					cmd_prefix = msg_recv[0] & CMD_PREFIX_MASK;

					// ------ SEND command ------
					if (cmd_prefix == SEND)
					{
						// Clear the system time-out
						SESSION->time_out = 0;

						result = pro_rx_recv_data(&PRO_STATE, &RECV_TAB, SESSION, &msg_recv[0]);
						//if (result == true)
						//	MYDEBUG.recv_pkt_session_correct++;
					}

					// ------ PING, CONFIG, START, CHECK, END command ------
					else if ((cmd_prefix == PING) || (cmd_prefix == CONFIG) ||
							 (cmd_prefix == START) || (cmd_prefix == END) ||
							 (cmd_prefix == CHECK))
					{
						// Clear the system time-out
						SESSION->time_out = 0;

						pro_rx_recv_cmd_send_ack(&PRO_STATE, SESSION, SAR_MSG, &RECV_TAB, &msg_recv[0]);
						// Ending condition
						if (PRO_STATE == END)
						{
							PRO_STATE = HALT;
						}
					}
				}
			}
		} 	//
		else
		{
			hal_delay_us(SESS_WAIT_RECV);

			// System time-out, if time-out reaches, halt the program
			SESSION->time_out += SESS_WAIT_RECV;
		}
	}	// while;
}
