#ifndef __PROJECT_CONF_H__
#define __PROJECT_CONF_H__

//#undef  UIP_CONF_BUFFER_SIZE
//#define UIP_CONF_BUFFER_SIZE				512
/*---------------------------------------------------------------------------*/
/* Launchpad/sensortag settings */
/* Enable the ROM bootloader */
#define ROM_BOOTLOADER_ENABLE           	1
#define STARTUP_CONF_VERBOSE				1
/*---------------------------------------------------------------------------*/
/* Change to match your configuration */
#define IEEE802154_CONF_PANID            	0xABCD
#define RF_CORE_CONF_CHANNEL               	18
#define RF_BLE_CONF_ENABLED                	0
/*---------------------------------------------------------------------------*/
/* ContikiMAC channel check rate given in Hz, specifying the number of channel checks per second*/
/* NETSTACK_RDC_CONF_CHANNEL_CHECK_RATE must be a power of two (i.e. 1, 2, 4, 8, 16, 32, 64, ...)*/
#define NETSTACK_CONF_RDC     contikimac_driver
//#undef  NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE
//#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 2
/*---------------------------------------------------------------------------*/
#undef  UDP_PORT
#define UDP_PORT 							1234
#define SERVICE_ID 							190
/*---------------------------------------------------------------------------*/
/* Disable linklayer security */
#undef LLSEC802154_CONF_SECURITY
#define LLSEC802154_CONF_SECURITY 0
#undef LLSEC802154_CONF_ENABLED
#define LLSEC802154_CONF_ENABLED 0
#undef LLSEC802154_CONF_USES_EXPLICIT_KEYS
#define LLSEC802154_CONF_USES_EXPLICIT_KEYS 0
#undef LLSEC802154_CONF_USES_FRAME_COUNTER
#define LLSEC802154_CONF_USES_FRAME_COUNTER 0
/*---------------------------------------------------------------------------*/
/*Application layer sending defines */

/* Remove these 2 defines when keep alive will be properly implemented */
#define TIMEOUT_NODE 1200*CLOCK_SECOND
#define KEEP_ALIVE_PACKET "A"

#define MAX_REPORTS_PER_PACKET 5
#define MAX_REPORT_DATA_SIZE MAX_REPORTS_PER_PACKET * SINGLE_NODE_REPORT_SIZE // 5 * 9

//#define SEND_INTERVAL	  					(15 * CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
/* Trickle defines */
#define TRICKLE_IMIN               10 * CLOCK_SECOND   /* ticks */
#define TRICKLE_IMAX               6   /* doublings of IMIN, so τmax = IMIN*(2^IMAX) */
#define REDUNDANCY_CONST   2   /*  aka k*/
#define TRICKLE_PROTO_PORT 30001

/* Remove these 2 defines when trickle will be properly implemented */
#define NEW_SETTING_INTERVAL  120 * CLOCK_SECOND
#define NEW_TOKEN_PROB      2

//DIO INTERVALS
/*
 * The DIO interval (n) represents 2^n ms.
 *
 * According to the specification, the default value is 3 which
 * means 8 milliseconds. That is far too low when using duty cycling
 * with wake-up intervals that are typically hundreds of milliseconds.
 * ContikiRPL thus sets the default to 2^12 ms = 4.096 s.
 */
#define RPL_CONF_DIO_INTERVAL_MIN        10
//
///*
// * Maximum amount of timer doublings.
// *
// * The maximum interval will by default be 2^(12+8) ms = 1048.576 s.
// * RFC 6550 suggests a default value of 20, which of course would be
// * unsuitable when we start with a minimum interval of 2^12.
// */
#define RPL_CONF_DIO_INTERVAL_DOUBLINGS  8
//
#define RPL_CONF_DEFAULT_ROUTE_INFINITE_LIFETIME 1
//
#define RPL_CONF_DEFAULT_LIFETIME_UNIT       20
///*
// * Default route lifetime as a multiple of the lifetime unit.
// */
#define RPL_CONF_DEFAULT_LIFETIME            2
//
#define RPL_CONF_DELAY_BEFORE_LEAVING 50 * CLOCK_SECOND
//
#define RPL_CONF_PROBING_INTERVAL 20 * CLOCK_SECOND
#define RPL_CONF_WITH_PROBING 1
#define RPL_CONF_DIS_INTERVAL 30

#endif /* __PROJECT_CONF_H__ */


