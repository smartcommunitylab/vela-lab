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

#include "sequential_procedures.h"

#ifdef CONTIKI
#include "contiki.h"
#else
#include "nrf_delay.h"
#endif

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG 0
#if DEBUG
    #ifdef CONTIKI
        #define PRINTF(...) printf(__VA_ARGS__)
    #else
        #define  NRF_LOG_MODULE_NAME sequential_procedures
        #include "nrf_log.h"
        #include "nrf_log_ctrl.h"
        NRF_LOG_MODULE_REGISTER();
        #define PRINTF(...) NRF_LOG_DEBUG(__VA_ARGS__)/*; \
                            NRF_LOG_PROCESS()*/
    #endif
#else
#define PRINTF(...)
#endif

#define EXECUTE_ACTION_DELAY_US         2000

static procedure_t* running_procedure=NULL;
static uint8_t prev_action_return = 1;

static uint8_t execute_action(void){
    static uint8_t ret;

#ifdef CONTIKI
    clock_delay_usec(EXECUTE_ACTION_DELAY_US);
#else
    nrf_delay_us(EXECUTE_ACTION_DELAY_US);
#endif

    PRINTF("Stepping on procedure \"%s\": action number %u\n",running_procedure->name,running_procedure->i);
    if(running_procedure->procedure_length!=0){
        ret = (*running_procedure->action[running_procedure->i])();
    }else{
        ret = (*running_procedure->action[0])();
    }
    (running_procedure->i)++;

    return ret;
}

uint8_t sequential_procedure_start(procedure_t *m_procedure, uint8_t delayed_start){
    if(m_procedure==NULL){
        return 0;
    }

    if(running_procedure->state != running){
        PRINTF("Starting procedure \"%s\" with delayed_start = %u\n",m_procedure->name,delayed_start);
        running_procedure = m_procedure;
        running_procedure->state=running;
        running_procedure->i=0;
        if(delayed_start == 0){	//start the procedure on the next procedure_execute_action
            sequential_procedure_execute_action();
        }
        return 1;
    }else{
        PRINTF("Cannot start procedure \"%s\" because \"%s\" is still running\n",m_procedure->name,running_procedure->name);
        return 0;
    }
}

void sequential_procedure_execute_action(){
    if(running_procedure != NULL && running_procedure->state != executed){
        if(running_procedure->procedure_length==0){ //variable length procedures
            if(prev_action_return!=0){
                prev_action_return=execute_action();
            }else{
                PRINTF("Stopping variable length procedure\n");
                sequential_procedure_stop();
            }
        }else{                                       //fixed length procedures
            if(running_procedure->i < running_procedure->procedure_length ){
                execute_action();
            }else{
                sequential_procedure_stop();
            }
        }
    }
}

void sequential_procedure_retry(){
    if(running_procedure!=NULL){
        running_procedure->state=running;
        (*running_procedure->action[running_procedure->i])();
    }
}

void sequential_procedure_stop(){
    sequential_procedure_stop_this(running_procedure);
}

void sequential_procedure_stop_this(procedure_t *m_procedure){
    if(m_procedure!=NULL){
        m_procedure->state=executed;
        m_procedure->i=0;
        prev_action_return=1;
        PRINTF("Procedure \"%s\" exectuted!\n",m_procedure->name);
    }
}

uint8_t sequential_procedure_is_running(){
    return sequential_procedure_is_this_running(running_procedure);
}

uint8_t sequential_procedure_is_this_running(procedure_t *m_procedure){
    if(m_procedure!=NULL){
        return m_procedure->state==running;
    }else{
        return 0;
    }
}
