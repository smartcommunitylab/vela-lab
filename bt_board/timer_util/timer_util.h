#ifndef TIMER_UTIL_H__
#define TIMER_UTIL_H__


#include "app_timer.h"
#include "app_error.h"


/*------------MACRO-------------*/
#define PERIODIC_TIMER_INTERVAL         APP_TIMER_TICKS(1000)
#define NODE_TIMEOUT_DEFAULT_MS			(uint32_t)60000

#define MIN_REPORT_TIMEOUT_MS					1000

#define APP_TIMER_MS(TICKS)  (uint32_t)(ROUNDED_DIV (((uint64_t)TICKS * ( ( (uint64_t)APP_TIMER_CONFIG_RTC_FREQUENCY + 1 ) * 1000 )) , (uint64_t)APP_TIMER_CLOCK_FREQ ))

#define UART_ACK_DELAY_US               2000


APP_TIMER_DEF(m_periodic_timer_id);
APP_TIMER_DEF(m_report_timer_id);
APP_TIMER_DEF(m_led_blink_timer_id);
APP_TIMER_DEF(m_lp_led_timer_id);


/*------------Variables-------------*/



/*------------Structures-------------*/


/*------------Functions-------------*/
void timer_init(void);
void application_timers_start(void);
void start_periodic_report(uint32_t timeout_ms);
void report_timeout_handler(void * p_context);
void periodic_timer_timeout_handler(void * p_context);
//void led_timeout_handler(void * p_context);
//void lp_led_timeout_handler(void * p_context);


/** @brief Timer init function
 *
 *
 */










#endif /* TIMER_UTIL_H__ */