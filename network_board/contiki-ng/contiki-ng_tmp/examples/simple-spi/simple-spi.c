/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         A very simple Contiki application showing how Contiki programs look
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "sys/etimer.h"
#include "dev/leds.h"
#include "dev/spi.h"
#include <stdio.h>
#include <stdint.h>

#define SEND_TIME 5 * CLOCK_SECOND
#define DELAY 5 * CLOCK_SECOND
#define WAIT_RX 10 * CLOCK_SECOND
#define WAIT_BT 10 * CLOCK_SECOND
/*Pin configurations ---------------------------------------------------------*/
#define SPI_PIN_SCK         IOID_18
#define SPI_PIN_MOSI        IOID_2
#define SPI_PIN_MISO        IOID_3
#define SPI_PIN_CS          IOID_19


#define SPI_CONTROLLER  SPI_CONTROLLER_SPI0



/*SPI commands --------------------------------------------------------------*/
#define ACK                 0x01
#define RESET               0x02
#define BT_SCAN             0x04
#define BT_ADV              0x05
#define REQ_BEACONS         0x06
#define BT_SCAN_WINDOW      0x07
#define BT_SCAN_INTERVAL    0x08

#define ON                     1
#define OFF                    0
/*---------------------------------------------------------------------------*/
static struct etimer et;
static struct etimer wait_for_rx;
//static struct etimer wait_bt;
//static bool bt_off = true;

static const spi_device_t spi_dev = {
  .spi_controller = SPI_CONTROLLER,
  .pin_spi_sck = SPI_PIN_SCK,
  .pin_spi_miso = SPI_PIN_MISO,
  .pin_spi_mosi = SPI_PIN_MOSI,
  .pin_spi_cs = SPI_PIN_CS,
  .spi_bit_rate = 1000000,
  .spi_pha = 0,
  .spi_pol = 0
};

#define RESET_PIN_IOID			IOID_22
void reset_nodric(void){
	GPIO_clearDio(RESET_PIN_IOID);
	clock_wait(CLOCK_SECOND/10);
	GPIO_setDio(RESET_PIN_IOID);
	clock_wait(CLOCK_SECOND/10);
}

void initialize_reset_pin(void) {
	GPIO_setOutputEnableDio(RESET_PIN_IOID, GPIO_OUTPUT_ENABLE);
	GPIO_setDio(RESET_PIN_IOID);
}

/*---------------------------------------------------------------------------*/
PROCESS(spi_process, "spi_process");
AUTOSTART_PROCESSES(&spi_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(spi_process, ev, data)
{

  char tx_string[20];
  char rx_string[20]; 
  uint16_t beacon_len;
  uint8_t last_rssi;
  uint8_t max_rssi;
  //char dummy_msg[20] = {"0"};
  //tx_string[0]=HELLO_WORLD;

  PROCESS_BEGIN();

  if(spi_acquire(&spi_dev) != SPI_DEV_STATUS_OK) {
    printf("Error");
  }

  //initialize_reset_pin();
  //etimer_set(&et, SEND_TIME);
  //etimer_set(&wait_for_rx, WAIT_RX);
  while(1) {

    //reset_nodric();

    // etimer_set(&et, DELAY);

    // PROCESS_WAIT_UNTIL(etimer_expired(&et));
    
    // tx_string[0]=ACK;

    // printf("Sending Writing adv : %x\n",tx_string[0]);

    // spi_select(&spi_dev);
    //   // Transmission 
    // if(spi_transfer(&spi_dev,
    //             (uint8_t *) tx_string, 1,
    //             (uint8_t *) rx_string, 1, 0) != SPI_DEV_STATUS_OK){
    //               printf("ERROR!!");
    //             }

    // spi_deselect(&spi_dev);

    // etimer_set(&wait_for_rx, DELAY);

    // PROCESS_WAIT_UNTIL(etimer_expired(&wait_for_rx));

    // tx_string[0] = OFF;
      
    // printf("received message : %x\n",rx_string[0]);

    // spi_select(&spi_dev);
    // // Reading the response. Should be 0x11
    // if(spi_transfer(&spi_dev,
    //         (uint8_t *) tx_string, 1,
    //         (uint8_t *) rx_string, 1, 0) != SPI_DEV_STATUS_OK){
    //           printf("ERROR!!");
    //         }
    // spi_deselect(&spi_dev);

    etimer_set(&et, DELAY);

    PROCESS_WAIT_UNTIL(etimer_expired(&et));

    // PROCESS_WAIT_UNTIL(etimer_expired(&et));

    leds_toggle(LEDS_GREEN);

    tx_string[0]=REQ_BEACONS | 0x80;

    printf("REQ BEACONS LEN : %x\n",tx_string[0]);

    spi_select(&spi_dev);
    // Transmission 
    if(spi_transfer(&spi_dev,
                (uint8_t *) tx_string, 1,
                (uint8_t *) rx_string, 1, 0) != SPI_DEV_STATUS_OK){
                  printf("ERROR!!");
                }
    spi_deselect(&spi_dev);

    printf("Message received : %x\n",rx_string[0]);

    // Wait a random time (10ms) for the response
    etimer_set(&wait_for_rx, DELAY);

    PROCESS_WAIT_UNTIL(etimer_expired(&wait_for_rx));
    
    printf("SENDING DUMMY MESSAGE\n");

    spi_select(&spi_dev);
    // Reading the response. Should be 0x11
    if(spi_transfer(&spi_dev,
            (uint8_t *) tx_string, 2,
            (uint8_t *) rx_string, 2, 0) != SPI_DEV_STATUS_OK){
            printf("ERROR!!");
            }
    spi_deselect(&spi_dev);

    memcpy(&beacon_len,&rx_string,sizeof(uint16_t));

    // See the response on the serial port
    printf("message received: %hx\n",beacon_len);

        // Wait a random time (10ms) for the response
    etimer_set(&wait_for_rx, DELAY);

    PROCESS_WAIT_UNTIL(etimer_expired(&wait_for_rx));
    
    printf("SENDING DUMMY MESSAGE\n");

    spi_select(&spi_dev);
    // Reading the response. Should be 0x11
    if(spi_transfer(&spi_dev,
            (uint8_t *) tx_string, beacon_len,
            (uint8_t *) rx_string, beacon_len, 0) != SPI_DEV_STATUS_OK){
            printf("ERROR!!");
            }
    spi_deselect(&spi_dev);

    int i;

    for(i=0;i<6;i++){
      printf("%x",rx_string[i]);
    }
    printf("\n");

    last_rssi = rx_string[6];
    max_rssi = rx_string[7];

    // See the response on the serial port
    printf("last rssi: %d\n",(int8_t)last_rssi);
    printf("max rssi: %d\n",(int8_t)max_rssi);

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */
