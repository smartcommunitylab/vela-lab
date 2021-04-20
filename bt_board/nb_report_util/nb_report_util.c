

#include "nb_report_util.h"
#include "app_error.h"


void start_periodic_report(uint32_t timeout_ms) {
    PRINTF("Setting BLE report with timeout %u ms!\n",timeout_ms);
	if (timeout_ms != 0 && timeout_ms < MIN_REPORT_TIMEOUT_MS) {

		return;
	}

	if (timeout_ms != 0) {
		ret_code_t err_code;
		if (m_periodic_report_timeout) { //if the report was already active, stop the timer to permit the timeout update
			err_code = app_timer_stop(m_report_timer_id);
			APP_ERROR_CHECK(err_code);
		}
		err_code = app_timer_start(m_report_timer_id, APP_TIMER_TICKS(timeout_ms), NULL);  //in any case if timeout_ms != 0 enable the report
		APP_ERROR_CHECK(err_code);
		m_periodic_report_timeout = timeout_ms;
		beacon_timeout_ms = timeout_ms;
	} else {
		if (m_periodic_report_timeout) { //if this command is sent with timeout_ms == 0 disable the report. If the report wasn't enabled, send a single report, if it was enabled stop everithing
			ret_code_t err_code;
			err_code = app_timer_stop(m_report_timer_id);
			APP_ERROR_CHECK(err_code);
			m_periodic_report_timeout = 0;
		} else {
		    start_procedure(&ble_report);
		}
	}
}


uint8_t send_neighbors_report(void) {

	static uint16_t n = 0;
	bool pkt_full = false;
	uint8_t report_payload[SPI_PKT_PAYLOAD_MAX_LEN_SYMB];
	uint16_t payload_free_from = 0;

	//uart_pkt_t packet;

	if(!sequential_procedure_is_this_running(&ble_report)){ //sanity check, if the report procedure was not active (maybe it was cancelled) reset n to zero
	    n=0;
	}

	packet.payload.p_data = report_payload;
	if (n == 0) {	//if this is the first packet of the report the type will end with the '_start'
		packet.type = uart_resp_bt_neigh_rep_start;
	} else {		//otherwise it will be '_more'
		packet.type = uart_resp_bt_neigh_rep_more;
	}

	while (n < MAXIMUM_NETWORK_SIZE && pkt_full == false) {
		if (UART_PKT_PAYLOAD_MAX_LEN_SYMB - payload_free_from > SINGLE_NODE_REPORT_SIZE) {
			if (!is_position_free(&m_network[n])) {
			    if(m_network[n].node_type!=IFS_TOF_DEVICE){
			        uint32_t occupied_size = add_node_to_report_payload(&m_network[n], &report_payload[payload_free_from], UART_PKT_PAYLOAD_MAX_LEN_SYMB - payload_free_from);
			        payload_free_from += occupied_size;
			    }
			}
			n++;
		} else {
			pkt_full = true;
			PRINTF("Report packet full!!\n");
		}
	}

	packet.payload.data_len = payload_free_from;
	if (n >= MAXIMUM_NETWORK_SIZE) { //if it is the last packet for this report change the message type to uart_resp_bt_neigh_rep_end
		packet.type = uart_resp_bt_neigh_rep_end;
		n = 0;
		payload_free_from = 0;
	}

	if (n == 0) {	//this is executed after the transmission of uart_resp_bt_neigh_rep_end
		reset_network(); //TODO: a specific function for resetting ONLY reported nodes should be used
	}

    PRINTF("Returning n=%u\n",n);

    uart_util_send_pkt(&packet);
	return (uint8_t)n;
}


uint8_t is_position_free(node_t *p_node) {
	return p_node->conn_handle == BLE_CONN_HANDLE_INVALID && p_node->first_contact_ticks == 0;
}