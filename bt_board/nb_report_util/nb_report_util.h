#ifndef NB_REPORT_UTIL_H__
#define NB_REPORT_UTIL_H__

#include "app_error.h"
#include "timer_util.h"
#include "constraints.h"
#include "ble.h"
#include "ble_types.h"
#include <string.h>

/*------------MACRO-------------*/
#define MAXIMUM_NETWORK_SIZE  100
#define SINGLE_NODE_REPORT_SIZE 9
#define SPI_PKT_PAYLOAD_MAX_LEN MAXIMUM_NETWORK_SIZE*SINGLE_NODE_REPORT_SIZE // 900


/*------------Structures-------------*/
typedef struct{
    uint8_t    p_data[31];      /**< Pointer to data. */
    uint16_t   len;             /**< Length of data. */
} s_data_t;

typedef enum{
    CLIMB_BEACON,
    EDDYSTONE_BEACON,
    NORDIC_BEACON,  //not used for now
    IFS_TOF_DEVICE,  //not used for now
	UNKNOWN_TYPE,
} node_type_t;


typedef struct{
    uint16_t  			conn_handle;
    uint8_t             local_role;
	ble_gap_addr_t 		bd_address;
	s_data_t			adv_data;
	s_data_t			scan_data;
	uint32_t			first_contact_ticks;
	uint32_t 			last_contact_ticks;
	int8_t				last_rssi;
	int8_t				max_rssi;
	uint8_t				beacon_msg_count;
	uint8_t				namespace_id[EDDYSTONE_NAMESPACE_ID_LENGTH];
	uint8_t				instance_id[EDDYSTONE_INSTANCE_ID_LENGTH];
	node_type_t			node_type;
} node_t;


/*------------Variables-------------*/
node_t m_network[MAXIMUM_NETWORK_SIZE];
uint8_t report_payload[SPI_PKT_PAYLOAD_MAX_LEN];
uint16_t payload_data_len;
extern bool preparing_payload; 



/*------------Functions-------------*/
uint32_t add_node_to_report_payload(node_t *m_network, uint8_t* report_payload, uint16_t max_size);
uint8_t is_position_free(node_t *p_node);
void prepare_neighbors_report(void);
void reset_node(node_t* p_node);
void reset_network(void);


/** @brief 
 *
 *
 */


#endif /* NB_REPORT_UTIL_H__ */