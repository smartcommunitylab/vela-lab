#ifndef SPI_UTIL_H__
#define SPI_UTIL_H__

#include "sdk_config.h"
#include "nrf_drv_spis.h"
#include "nrf_gpio.h"
#include "boards.h"
#include "app_error.h"
#include  "nb_report_util.h"
#include <string.h>



/*------------MACRO-------------*/
#define SPIS_INSTANCE 1 /**< SPIS instance index. */


#define SCK_PIN  22
#define CS_PIN   23
#define MOSI_PIN 24
#define MISO_PIN 25

#define ADV_OFF   0
#define ADV_ON    1

#define SCAN_OFF   0
#define SCAN_ON    1

/*------------SPI COMMANDS-------------*/

#define CMD_READ       1
#define CMD_WRITE      0

#define ACK                 0x01
#define RESET               0x02
#define BT_SCAN             0x04
#define BT_ADV              0x05
#define REQ_BEACONS         0x06
#define BT_SCAN_WINDOW      0x07
#define BT_SCAN_INTERVAL    0x08

/*------------Variables-------------*/
static const nrf_drv_spis_t spis = NRF_DRV_SPIS_INSTANCE(SPIS_INSTANCE);/**< SPIS instance. */


/*------------Structures-------------*/
typedef enum{
  START_CMD,
  ADV_WRITE,
  SCAN_WRITE,
  WAITING_CMD,
  BEACONS_DATA,
  ERROR,
} spi_rx_status_t;

/*------------Functions-------------*/


/** @brief SPI init function
 *
 *
 */
void spi_init(void);


#endif /* SPI_UTIL_H__ */