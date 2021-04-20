

#include "timer_util.h"
#include "app_error.h"
#include "boards.h"






/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
void timer_init(void) {
	ret_code_t err_code = app_timer_init();
	APP_ERROR_CHECK(err_code);

	// Create timers.
	err_code = app_timer_create(&m_periodic_timer_id, APP_TIMER_MODE_REPEATED, periodic_timer_timeout_handler);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_create(&m_report_timer_id, APP_TIMER_MODE_REPEATED, report_timeout_handler);
	APP_ERROR_CHECK(err_code);

	// err_code = app_timer_create(&m_led_blink_timer_id, APP_TIMER_MODE_SINGLE_SHOT, led_timeout_handler);
	// APP_ERROR_CHECK(err_code);

	// err_code = app_timer_create(&m_lp_led_timer_id, APP_TIMER_MODE_REPEATED, lp_led_timeout_handler);
	// APP_ERROR_CHECK(err_code);


}

void application_timers_start(void) {
	ret_code_t err_code;

	// Start application timers.
	err_code = app_timer_start(m_periodic_timer_id, PERIODIC_TIMER_INTERVAL, NULL);
	APP_ERROR_CHECK(err_code);

	// // Start application timers.
	// err_code = app_timer_start(m_lp_led_timer_id, LP_LED_INTERVAL, NULL);
	// APP_ERROR_CHECK(err_code);
}




void report_timeout_handler(void * p_context) {
	UNUSED_PARAMETER(p_context);


    //start_procedure(&ble_report);
}

void periodic_timer_timeout_handler(void * p_context) {
    bsp_board_led_invert(BSP_BOARD_LED_3);
	UNUSED_PARAMETER(p_context);
}

// void led_timeout_handler(void * p_context) {
// 	uint32_t * p_blinking_led_bit_map = (uint32_t*)p_context;
// 	for(uint8_t led_idx = 0; led_idx < LEDS_NUMBER; led_idx++){
// 		if((*p_blinking_led_bit_map >> led_idx) & 0b1){
// 			bsp_board_led_off(led_idx);
// 		}
// 	}
// 	*p_blinking_led_bit_map = 0;
// }

// void lp_led_timeout_handler(void * p_context) {
// 	uint32_t led_to_blink = 0;
// 	//if(m_scan_active || m_advertising_active) led_to_blink |= SCAN_ADV_LED;
// 	if(m_ready_received)  led_to_blink |= READY_LED;
// 	if(m_tx_error)  led_to_blink |= ERROR_LED;
// 	if(m_periodic_report_timeout) led_to_blink |= PROGRESS_LED;

// 	blink_led(led_to_blink, LED_BLINK_TIMEOUT_MS);
// }