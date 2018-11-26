#ifndef __PROJECT_CONF_H__
#define __PROJECT_CONF_H__

#undef  UIP_CONF_BUFFER_SIZE
#define UIP_CONF_BUFFER_SIZE				512

/*---------------------------------------------------------------------------*/
/* Launchpad/sensortag settings */
/* Enable the ROM bootloader */
#define ROM_BOOTLOADER_ENABLE           	1
#define STARTUP_CONF_VERBOSE				1
/*---------------------------------------------------------------------------*/
/* Change to match your configuration */
#define IEEE802154_CONF_PANID            	0xABCD
#define RF_CORE_CONF_CHANNEL               	20
#define RF_BLE_CONF_ENABLED                	0
/*---------------------------------------------------------------------------*/
/* ContikiMAC channel check rate given in Hz, specifying the number of channel checks per second*/
/* NETSTACK_RDC_CONF_CHANNEL_CHECK_RATE must be a power of two (i.e. 1, 2, 4, 8, 16, 32, 64, ...)*/
#undef  NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE
#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 32
/*---------------------------------------------------------------------------*/
#undef  UDP_PORT
#define UDP_PORT 							1234
#define SERVICE_ID 							190

//#define SEND_INTERVAL	  					(15 * CLOCK_SECOND)


#endif /* __PROJECT_CONF_H__ */


