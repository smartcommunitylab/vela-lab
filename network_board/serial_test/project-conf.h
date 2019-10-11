#ifndef __PROJECT_CONF_H__
#define __PROJECT_CONF_H__

/*---------------------------------------------------------------------------*/
#define ROM_BOOTLOADER_ENABLE           	1
#define STARTUP_CONF_VERBOSE				1
/*---------------------------------------------------------------------------*/
/* Change to match your configuration */
#define IEEE802154_CONF_PANID            	0xABCD
#define RF_CORE_CONF_CHANNEL               	18
#define RF_BLE_CONF_ENABLED                	0
/* Disable linklayer security */
#undef LLSEC802154_CONF_SECURITY
#define LLSEC802154_CONF_SECURITY           0
#undef LLSEC802154_CONF_ENABLED
#define LLSEC802154_CONF_ENABLED            0
#undef LLSEC802154_CONF_USES_EXPLICIT_KEYS
#define LLSEC802154_CONF_USES_EXPLICIT_KEYS 0
#undef LLSEC802154_CONF_USES_FRAME_COUNTER
#define LLSEC802154_CONF_USES_FRAME_COUNTER 0
/*---------------------------------------------------------------------------*/
#undef  UDP_PORT
#define UDP_PORT 							1234
#define SERVICE_ID 							190
/*---------------------------------------------------------------------------*/
/* Trickle defines */
#define TRICKLE_IMIN                        10 * CLOCK_SECOND   /* ticks */
#define TRICKLE_IMAX                        15   /* doublings of IMIN, so τmax = IMIN*(2^IMAX) */
#define REDUNDANCY_CONST                    2   /*  aka k*/
#define TRICKLE_PROTO_PORT                  30001
//-------------------------------------------------------------------------------
/* Do not start TSCH at init, wait for NETSTACK_MAC.on() */
#define TSCH_CONF_AUTOSTART                 0
#define ORCHESTRA_CONF_UNICAST_PERIOD       7

/* 6TiSCH minimal schedule length.
 * Larger values result in less frequent active slots: reduces capacity and saves energy. */
#define TSCH_SCHEDULE_CONF_DEFAULT_LENGTH   3

/* Five nines reliability paper used the config below */
#define RPL_CONF_DIO_INTERVAL_MIN           14 /* 2^14 ms = 16.384 s */
#define RPL_CONF_DIO_INTERVAL_DOUBLINGS     6 /* 2^(14+6) ms = 1048.576 s */
#define RPL_CONF_PROBING_INTERVAL           (60 * CLOCK_SECOND)

/* Five nines reliability paper used the config below */
#define TSCH_CONF_KEEPALIVE_TIMEOUT         (20 * CLOCK_SECOND)
#define TSCH_CONF_MAX_KEEPALIVE_TIMEOUT     (60 * CLOCK_SECOND)

#endif /* __PROJECT_CONF_H__ */

