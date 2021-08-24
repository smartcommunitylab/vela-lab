/*
*Copyright 2017 Fondazione Bruno Kessler
*
*Licensed under the Apache License, Version 2.0 (the "License");
*you may not use this file except in compliance with the License.
*You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
*Unless required by applicable law or agreed to in writing, software
*distributed under the License is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*See the License for the specific language governing permissions and
*limitations under the License.
*/

#include "vela_spi.h"

PROCESS(spi_process, "spi process");

uint8_t is_nordic_ready=false;
uint8_t no_of_attempts = 0;
uint8_t bt_report_buffer[BT_REPORT_BUFFER_SIZE]; //this might be allocated dynamically once the BT is enabled and free once it is disabled (avoid to allocate the space twice!!!)
data_t complete_report_data = {bt_report_buffer, 0};
#if NORDIC_WATCHDOG_ENABLED
struct ctimer m_nordic_watchdog_timer;
uint8_t nordic_watchdog_value = 0;
uint32_t nordic_watchdog_timeout_ms = 0;
#endif
uint8_t active_scan = DEFAULT_ACTIVE_SCAN;     //boolean
uint16_t scan_interval = DEFAULT_SCAN_INTERVAL;       /**< Scan interval between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
uint16_t scan_window = DEFAULT_SCAN_WINDOW;     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
uint16_t timeout = DEFAULT_TIMEOUT;            /**< Scan timeout between 0x0001 and 0xFFFF in seconds, 0x0000 disables timeout. */
uint32_t report_timeout_ms = 0, report_timeout_ms_arg = 0;
uint8_t bt_scan_state = 0;
uint8_t lock_new_commands=false;

static char rx_string[900];
static char tx_string[20];
static char init_spi[20]; 

static struct ctimer nb_report_interval;

//ALM -- called to start this as a contiki THREAD
void vela_spi_process_init() {
  // start the spi process
  process_start(&spi_process, "spi process");
  return;
}

uint32_t report_ready(data_t *p_data){
	//PROCESS REPORT: return APP_ACK_SUCCESS as soon as possible
	process_post(&vela_sender_process, event_data_ready, p_data);
#if NORDIC_WATCHDOG_ENABLED
	nordic_watchdog_value = 0; //reset the nordic watchdog
#endif
	return APP_ACK_SUCCESS;
}
static uint16_t buff_free_from = 0;
static uint32_t ret = APP_ERROR_COMMAND_NOT_VALID;


#if NORDIC_WATCHDOG_ENABLED
void nordic_watchdog_handler(void *ptr){
	nordic_watchdog_value++;

	if(nordic_watchdog_value > 3){
	    LOG_ERR("Nordic didn't respond, resetting it!\n");
	    reset_nordic();
	}else{
		if(nordic_watchdog_timeout_ms != 0){
			ctimer_set(&m_nordic_watchdog_timer, (nordic_watchdog_timeout_ms*CLOCK_SECOND)/1000, nordic_watchdog_handler, NULL);
		}
	}
}
#endif


#define RESET_PIN_IOID			IOID_22
void reset_nordic(void){
	GPIO_clearDio(RESET_PIN_IOID);
	clock_wait(CLOCK_SECOND/10);
	GPIO_setDio(RESET_PIN_IOID);
	clock_wait(CLOCK_SECOND);

    // The very first message on spi at nordic side is not seen. Just send a dummy msg
    init_spi[0]=ACK;
    spi_select(&spi_dev);
    if(spi_transfer(&spi_dev,
            (uint8_t *) init_spi, 1,
            (uint8_t *) rx_string, 1, 0) != SPI_DEV_STATUS_OK){
            LOG_ERR("ERROR!!");
    }
    spi_deselect(&spi_dev);
}


void initialize_reset_pin(void) {
	GPIO_setOutputEnableDio(RESET_PIN_IOID, GPIO_OUTPUT_ENABLE);
	GPIO_setDio(RESET_PIN_IOID);
}

void spi_init(){
    if(spi_acquire(&spi_dev) != SPI_DEV_STATUS_OK) {
        //LOG_INFO("Error in configuring the SPI\n");
        leds_toggle(LEDS_RED);
    }
}

void spi_bt_scan_on(void){
    tx_string[0]=BT_SCAN;
    spi_select(&spi_dev);
    if(spi_transfer(&spi_dev,
            (uint8_t *) tx_string, 1,
            (uint8_t *) rx_string, 1, 0) != SPI_DEV_STATUS_OK){
                LOG_ERR("ERROR!!");
    }
    spi_deselect(&spi_dev);
    clock_wait(CLOCK_SECOND/10);

    tx_string[0]=ON;
    spi_select(&spi_dev);
    if(spi_transfer(&spi_dev,
            (uint8_t *) tx_string, 1,
            (uint8_t *) rx_string, 1, 0) != SPI_DEV_STATUS_OK){
                LOG_ERR("ERROR!!");
    }
    spi_deselect(&spi_dev);
}

void spi_bt_scan_off(void){
    tx_string[0]=BT_SCAN;
    spi_select(&spi_dev);
    if(spi_transfer(&spi_dev,
            (uint8_t *) tx_string, 1,
            (uint8_t *) rx_string, 1, 0) != SPI_DEV_STATUS_OK){
                LOG_ERR("ERROR!!");
    }
    spi_deselect(&spi_dev);
    clock_wait(CLOCK_SECOND/10);

    tx_string[0]=OFF;
    spi_select(&spi_dev);
    if(spi_transfer(&spi_dev,
            (uint8_t *) tx_string, 1,
            (uint8_t *) rx_string, 1, 0) != SPI_DEV_STATUS_OK){
                LOG_ERR("ERROR!!");
    }
    spi_deselect(&spi_dev);
}

static void beacon_report_request(void){
    uint16_t beacon_len;

    tx_string[0]=REQ_BEACONS;
    spi_select(&spi_dev);
    if(spi_transfer(&spi_dev,
            (uint8_t *) tx_string, 1,
            (uint8_t *) rx_string, 1, 0) != SPI_DEV_STATUS_OK){
                LOG_ERR("ERROR!!");
    }
    spi_deselect(&spi_dev);
    clock_wait(CLOCK_SECOND/10);

    tx_string[0]=ACK;
    spi_select(&spi_dev);
    if(spi_transfer(&spi_dev,
            (uint8_t *) tx_string, 2,
            (uint8_t *) rx_string, 2, 0) != SPI_DEV_STATUS_OK){
                LOG_ERR("ERROR!!");
    }
    spi_deselect(&spi_dev);
    memcpy(&beacon_len,&rx_string,sizeof(uint16_t));

    clock_wait(CLOCK_SECOND/10);
    spi_select(&spi_dev);
    if(spi_transfer(&spi_dev,
        (uint8_t *) tx_string, beacon_len,
        (uint8_t *) rx_string, beacon_len, 0) != SPI_DEV_STATUS_OK){
        }
    spi_deselect(&spi_dev);
    complete_report_data.data_len = beacon_len;
    complete_report_data.p_data = (uint8_t  *) rx_string;

    clock_wait(CLOCK_SECOND/20);
    leds_toggle(LEDS_RED);
    clock_wait(CLOCK_SECOND/20);
    leds_toggle(LEDS_RED);

    for (uint8_t i = 0; i < beacon_len/9; i++)
    {
      clock_wait(CLOCK_SECOND/20);
      leds_toggle(LEDS_RED);
      clock_wait(CLOCK_SECOND/20);
      leds_toggle(LEDS_RED);
    }
    report_ready(&complete_report_data);
    ctimer_reset(&nb_report_interval);
  
}

PROCESS_THREAD(spi_process, ev, data) {

	PROCESS_BEGIN();

	    event_data_ready = process_alloc_event();
        event_ping_requested = process_alloc_event();
        event_pong_received = process_alloc_event();
        event_nordic_message_received = process_alloc_event();
        turn_bt_on = process_alloc_event();
        turn_bt_off = process_alloc_event();
        turn_bt_on_w_params = process_alloc_event();
        turn_bt_on_low = process_alloc_event();
        turn_bt_on_def = process_alloc_event();
        turn_bt_on_high = process_alloc_event();
        reset_bt = process_alloc_event();

        //This needs to be here to avoid fires and death
		cc26xx_uart_set_input(serial_line_input_byte);

		clock_wait(CLOCK_SECOND/4); //wait to send all the contiki text before enabling the uart flow control
		initialize_reset_pin();
        spi_init();
		reset_nordic();

        // wait 1s for the Nordic to be ready
        clock_wait(1*CLOCK_SECOND);

        // The very first message on spi at nordic side is not seen. Just send a dummy msg
        init_spi[0]=ACK;
        spi_select(&spi_dev);
        if(spi_transfer(&spi_dev,
                (uint8_t *) init_spi, 1,
                (uint8_t *) rx_string, 1, 0) != SPI_DEV_STATUS_OK){
                  LOG_ERR("ERROR!!");
        }
        spi_deselect(&spi_dev);
        
		while (1) {

			PROCESS_WAIT_EVENT();
            if(ev == reset_bt){ //trickle commands are not accepted if a procedure is running, however always accept reset
#if NORDIC_WATCHDOG_ENABLED
                ctimer_stop(&m_nordic_watchdog_timer);
#endif
                lock_new_commands=false; //this is for safety
                buff_free_from=0;        // thi is also for safety
                reset_nordic();
            }

            if(lock_new_commands==false){
                if (ev == event_ping_requested) {
                    uint8_t* payload = (uint8_t*)data;
                    //send_ping(*payload);
                }

                if(ev == turn_bt_off) {
                    // start_procedure(&bluetooth_off);
                    spi_bt_scan_off();
                    
                    ctimer_stop(&nb_report_interval);
                    // if(process_is_running(&nb_report_process)){
                    //     process_exit(&nb_report_process);
                    // }
                }

                if(ev == turn_bt_on) {
                    // if(report_timeout_ms == 0){   //if turn_bt_on is sent before any call to turn_bt_on_w_params. Load default value
                    //     scan_window = DEFAULT_SCAN_WINDOW;
                    //     scan_interval = DEFAULT_SCAN_INTERVAL;
                    //     report_timeout_ms = DEFAULT_REPORT_TIMEOUT_MS;
                    // }
                    // report_timeout_ms_arg = report_timeout_ms;
                    // start_procedure(&bluetooth_on);
                    spi_bt_scan_on();

                    // if(!(process_is_running(&nb_report_process))){
                    //     process_start(&nb_report_process, NULL);
                    // }
                    ctimer_set(&nb_report_interval, NB_REPORT_INTERVAL, beacon_report_request, NULL);
                }

                if(ev == turn_bt_on_w_params) {

                    // active_scan = ((uint8_t*)data)[0];     //boolean
                    // scan_interval = ((((uint8_t*)data)[1])<<8)+((uint8_t*)data)[2];       /**< Scan interval between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
                    // scan_window = ((((uint8_t*)data)[3])<<8)+((uint8_t*)data)[4];     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
                    // timeout = ((((uint8_t*)data)[5])<<8)+((uint8_t*)data)[6];
                    // report_timeout_ms = ((((uint8_t*)data)[7])<<24)+((((uint8_t*)data)[8])<<16)+((((uint8_t*)data)[9])<<8)+((uint8_t*)data)[10];
                    // report_timeout_ms_arg = report_timeout_ms;

                    // start_procedure(&bluetooth_on);
                }
                if(ev == turn_bt_on_def) {
                    // scan_window = DEFAULT_SCAN_WINDOW;     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
                    // scan_interval = DEFAULT_SCAN_INTERVAL;     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
                    // report_timeout_ms_arg = DEFAULT_REPORT_TIMEOUT_MS;
                    // report_timeout_ms = DEFAULT_REPORT_TIMEOUT_MS;
                    // start_procedure(&bluetooth_on);
                }

                //next commands are deprecated on the gateway but still handled by the TI board
                if(ev == turn_bt_on_low) {
                }
                if(ev == turn_bt_on_high) {
                }
            }else{
                if(ev == turn_bt_off ||
                   ev == turn_bt_on  ||
                   ev == turn_bt_on_w_params  ||
                   ev == turn_bt_on_def  ||
                   ev == turn_bt_on_low  ||
                   ev == turn_bt_on_high ||
                   ev == turn_ble_tof_onoff){
                    LOG_WARN("Cannot accept a command now! Lock is active\n");
                }
            }
		}
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */
