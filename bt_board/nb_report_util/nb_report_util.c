

#include "nb_report_util.h"
#include "app_error.h"


bool preparing_payload = 0;
/*TO BE CALLED BY THE SPI LIBRARY*/

void prepare_neighbors_report(void) {

    preparing_payload = true;
	static uint16_t n = 0;
	uint16_t payload_free_from = 0;

	//uart_pkt_t packet;

	while (n < MAXIMUM_NETWORK_SIZE) {
    
        if (!is_position_free(&m_network[n])) {
            
            uint32_t occupied_size = add_node_to_report_payload(&m_network[n], &report_payload[payload_free_from], SPI_PKT_PAYLOAD_MAX_LEN_SYMB - payload_free_from);
            payload_free_from += occupied_size;

        }
        n++;
	}

    payload_data_len = payload_free_from;
    n = 0;
    payload_free_from = 0;
    reset_network(); //TODO: a specific function for resetting ONLY reported nodes should be used


    //uart_util_send_pkt(&packet);
    preparing_payload = false;
	return;
}


uint8_t is_position_free(node_t *p_node) {
	return p_node->conn_handle == BLE_CONN_HANDLE_INVALID && p_node->first_contact_ticks == 0;
}

uint32_t add_node_to_report_payload(node_t *p_node, uint8_t* report_payload, uint16_t max_size) {

	if (p_node == NULL || report_payload == NULL || max_size < SINGLE_NODE_REPORT_SIZE) {
		return 0;
	}

	uint16_t idx = 0;

	//add ID
	memcpy(&report_payload[idx], p_node->instance_id, EDDYSTONE_INSTANCE_ID_LENGTH);
	idx += EDDYSTONE_INSTANCE_ID_LENGTH;

	//add last rssi
	report_payload[idx++] = (uint8_t) p_node->last_rssi;

	//add max rssi
	report_payload[idx++] = (uint8_t) p_node->max_rssi;

	//add packet counter
	report_payload[idx++] = p_node->beacon_msg_count;
	return idx;
}

void reset_node(node_t* p_node) { //TODO: check this, many of the fields of node_t are not reset...It may be better to use memset? In this case the code should be carefully checked to avoid problems such as it could happen in is_position_free(..) if the node reset is performed with memset(p_node, 0x00, sizeof(node_t)); (is_position_free would be zero everytime)
	memset(&p_node->bd_address, 0x00, sizeof(ble_gap_addr_t));
	p_node->conn_handle = BLE_CONN_HANDLE_INVALID;
	p_node->local_role = BLE_GAP_ROLE_INVALID;
	p_node->max_rssi = INT8_MIN;
	p_node->first_contact_ticks = 0;
	p_node->beacon_msg_count = 0;
}

void reset_network(void) {
	//memset(m_network, 0x00, MAXIMUM_NETWORK_SIZE * sizeof(node_t));
	for (uint8_t n = 0; n < MAXIMUM_NETWORK_SIZE; n++) {
        
        reset_node(&m_network[n]);

	}
}