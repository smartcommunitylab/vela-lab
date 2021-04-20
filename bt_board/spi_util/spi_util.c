
#ifndef SPI_UTIL_H__
#include "spi_util.h"
#endif
#include "ble_util.h"
#include "app_error.h"


static uint8_t       m_tx_buf[20];           /**< TX buffer. */
static uint8_t       m_rx_buf[20];           /**< RX buffer. */
static const uint8_t m_length = 1;           /**< Transfer length. */

static volatile bool spis_xfer_done; /**< Flag used to indicate that SPIS instance completed the transfer. */
nrf_drv_spis_config_t spis_config = NRF_DRV_SPIS_DEFAULT_CONFIG;
spi_rx_status_t state = START_CMD;
spi_rx_status_t new_state = START_CMD;



void spis_event_handler(nrf_drv_spis_event_t event){

    uint8_t cmd = m_rx_buf[0] & 0b01111111;

    uint8_t cmd_type = (m_rx_buf[0] >> 7) & 0b00000001;
    

    if (event.evt_type == NRF_DRV_SPIS_XFER_DONE){
        switch(state){

            case START_CMD:

                switch(cmd){
            
                    case ACK:

                        m_tx_buf[0] = ACK;
                        APP_ERROR_CHECK(nrf_drv_spis_buffers_set(&spis, m_tx_buf, m_length, m_rx_buf, m_length));

                        new_state = START_CMD;

                        break;

                    case BT_ADV:

                        m_tx_buf[0] = m_rx_buf[0];

                        if(cmd_type == CMD_WRITE){
                            
                            APP_ERROR_CHECK(nrf_drv_spis_buffers_set(&spis, m_tx_buf, m_length, m_rx_buf, m_length));
                            new_state = ADV_WRITE;
                        }
                        
                
                        break;

                    case BT_SCAN:
                        bsp_board_led_invert(BSP_BOARD_LED_1);
                        m_tx_buf[0] = m_rx_buf[0];

                        if(cmd_type == CMD_WRITE){
                            
                            APP_ERROR_CHECK(nrf_drv_spis_buffers_set(&spis, m_tx_buf, m_length, m_rx_buf, m_length));
                            new_state = SCAN_WRITE;
                        }
                        
                
                        break;


                    default:

                        APP_ERROR_CHECK(nrf_drv_spis_buffers_set(&spis, m_tx_buf, m_length, m_rx_buf, m_length));
                        new_state = START_CMD;
                        break;
                }

                break;

            case ADV_WRITE:

                if(m_rx_buf[0] == ADV_ON){

                    advertising_start();
                    m_tx_buf[0] = ADV_ON;

                }else{

                    advertising_stop();
                    m_tx_buf[0] = ADV_OFF;

                }

                new_state = START_CMD;
                APP_ERROR_CHECK(nrf_drv_spis_buffers_set(&spis, m_tx_buf, m_length, m_rx_buf, m_length));

                break;


            case SCAN_WRITE:

                if(m_rx_buf[0] == SCAN_ON){

                    // bsp_board_led_on(BSP_BOARD_LED_2);
                    // bsp_board_led_off(BSP_BOARD_LED_3);
                    scan_start();
                    m_tx_buf[0] = SCAN_ON;

                }else{

                    // bsp_board_led_on(BSP_BOARD_LED_3);
                    // bsp_board_led_off(BSP_BOARD_LED_2);
                    scan_stop();
                    m_tx_buf[0] = SCAN_OFF;

                }

                new_state = START_CMD;
                APP_ERROR_CHECK(nrf_drv_spis_buffers_set(&spis, m_tx_buf, m_length, m_rx_buf, m_length));

                break;

            default:
                
                APP_ERROR_CHECK(nrf_drv_spis_buffers_set(&spis, m_tx_buf, m_length, m_rx_buf, m_length));
                new_state = START_CMD;
                break;

    }
    state = new_state;


    }


}

void spi_init(void){
    
    // SPI pin definition
    spis_config.csn_pin               = CS_PIN;
    spis_config.miso_pin              = MISO_PIN; 
    spis_config.mosi_pin              = MOSI_PIN; 
    spis_config.sck_pin               = SCK_PIN;
    spis_config.mode                  = NRF_SPIS_MODE_0;
    
    APP_ERROR_CHECK(nrf_drv_spis_init(&spis, &spis_config, spis_event_handler));

    // Prepare the SPI buffers.
    memset(m_rx_buf, 0, m_length);
    APP_ERROR_CHECK(nrf_drv_spis_buffers_set(&spis, m_tx_buf, m_length, m_rx_buf, m_length));

}


