
#ifndef BLE_UTIL_H__
#include "ble_util.h"
#endif
#include "app_error.h"

bool m_advertising_active = false;
bool m_scan_active = false;

ble_gap_adv_params_t m_adv_params; 


/*--------------------------------*/
/*----------ADVERTISING----------*/

void advertising_init(void){

    uint32_t      err_code;
    ble_advdata_t advdata;
    uint8_t       flags = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED;

    ble_advdata_manuf_data_t manuf_specific_data;

    manuf_specific_data.company_identifier = APP_COMPANY_IDENTIFIER;

#if defined(USE_UICR_FOR_MAJ_MIN_VALUES)
    // If USE_UICR_FOR_MAJ_MIN_VALUES is defined, the major and minor values will be read from the
    // UICR instead of using the default values. The major and minor values obtained from the UICR
    // are encoded into advertising data in big endian order (MSB First).
    // To set the UICR used by this example to a desired value, write to the address 0x10001080
    // using the nrfjprog tool. The command to be used is as follows.
    // nrfjprog --snr <Segger-chip-Serial-Number> --memwr 0x10001080 --val <your major/minor value>
    // For example, for a major value and minor value of 0xabcd and 0x0102 respectively, the
    // the following command should be used.
    // nrfjprog --snr <Segger-chip-Serial-Number> --memwr 0x10001080 --val 0xabcd0102
    uint16_t major_value = ((*(uint32_t *)UICR_ADDRESS) & 0xFFFF0000) >> 16;
    uint16_t minor_value = ((*(uint32_t *)UICR_ADDRESS) & 0x0000FFFF);

    uint8_t index = MAJ_VAL_OFFSET_IN_BEACON_INFO;

    m_beacon_info[index++] = MSB_16(major_value);
    m_beacon_info[index++] = LSB_16(major_value);

    m_beacon_info[index++] = MSB_16(minor_value);
    m_beacon_info[index++] = LSB_16(minor_value);
#endif

    manuf_specific_data.data.p_data = (uint8_t *) m_beacon_info;
    manuf_specific_data.data.size   = APP_BEACON_INFO_LENGTH;

    // Build and set advertising data.
    memset(&advdata, 0, sizeof(advdata));

    advdata.name_type             = BLE_ADVDATA_NO_NAME;
    advdata.flags                 = flags;
    advdata.p_manuf_specific_data = &manuf_specific_data;

    err_code = ble_advdata_set(&advdata, NULL);
    APP_ERROR_CHECK(err_code);

    // Initialize advertising parameters (used when starting advertising).
    memset(&m_adv_params, 0, sizeof(m_adv_params));

    m_adv_params.type        = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;
    m_adv_params.p_peer_addr = NULL;    // Undirected advertisement.
    m_adv_params.fp          = BLE_GAP_ADV_FP_ANY;
    m_adv_params.interval    = NON_CONNECTABLE_ADV_INTERVAL;
    m_adv_params.timeout     = 0;       // Never time out.
}



void advertising_start(void){
    ret_code_t err_code;

	if (!m_advertising_active) {
		m_advertising_active = true;
    	err_code = sd_ble_gap_adv_start(&m_adv_params, APP_BLE_CONN_CFG_TAG);
    	APP_ERROR_CHECK(err_code);
		bsp_board_led_on(BSP_BOARD_LED_2);

	}
}


/**@brief Function for starting advertising. */
void advertising_stop(void) {
	if (m_advertising_active) {

		m_advertising_active = false;
		ret_code_t err_code = sd_ble_gap_adv_stop();
		APP_ERROR_CHECK(err_code);
		bsp_board_led_off(BSP_BOARD_LED_2);
	}
}

/*-------------------------*/
/*----------SCAN----------*/

void scan_init(void) {

	if (m_scan_active) {
		ret_code_t err_code = sd_ble_gap_scan_stop();
		APP_ERROR_CHECK(err_code);
	}

	m_scan_param.active = DEFAULT_ACTIVE_SCAN;
	m_scan_param.interval = DEFAULT_SCAN_INTERVAL;
	m_scan_param.window = DEFAULT_SCAN_WINDOW;
	m_scan_param.timeout = DEFAULT_TIMEOUT;

	if (m_scan_active) {
		ret_code_t err_code = sd_ble_gap_scan_start(&m_scan_param);
		APP_ERROR_CHECK(err_code);
	}
}


void scan_set_interval(uint16_t scan_interval) {
	if (m_scan_active) {
		ret_code_t err_code = sd_ble_gap_scan_stop();
		APP_ERROR_CHECK(err_code);
	}

	m_scan_param.interval = scan_interval;

	if (m_scan_active) {
		ret_code_t err_code = sd_ble_gap_scan_start(&m_scan_param);
		APP_ERROR_CHECK(err_code);
	}
}

void scan_set_window(uint16_t scan_window) {
	if (m_scan_active) {
		ret_code_t err_code = sd_ble_gap_scan_stop();
		APP_ERROR_CHECK(err_code);
	}
	m_scan_param.window = scan_window;

	if (m_scan_active) {
		ret_code_t err_code = sd_ble_gap_scan_start(&m_scan_param);
		APP_ERROR_CHECK(err_code);
	}
}


void scan_stop(void) {
	if (m_scan_active) {

		m_scan_active = false;
		ret_code_t err_code = sd_ble_gap_scan_stop();
		APP_ERROR_CHECK(err_code);
	}
}

/**@brief Function to start scanning. */
void scan_start(void) {
	if (!m_scan_active) {
		m_scan_active = true;
		ret_code_t err_code = sd_ble_gap_scan_start(&m_scan_param);
		APP_ERROR_CHECK(err_code);
	}
}