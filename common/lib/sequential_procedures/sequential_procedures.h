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


#ifndef SEQUENTIAL_PROCEDURES_H__
#define SEQUENTIAL_PROCEDURES_H__

#include <stdio.h>

typedef enum{
    stopped,
    running,
    executed,
} procedure_state_t;

typedef uint8_t (*m_f_ptr_t)(void);

typedef struct{
    uint8_t i;
    m_f_ptr_t * action;
    uint8_t procedure_length;               //set to 0 for variable length (variable length procedure should call procedure_ended() when they finish otherwise the procedure keeps going)
    procedure_state_t state;
    char name[];
}procedure_t;

#define PROCEDURE(name, ...) uint8_t (*name##_sequence[])(void)={__VA_ARGS__}; \
                            procedure_t name={0, name##_sequence, sizeof(name##_sequence)/sizeof(m_f_ptr_t) ,stopped, STRINGIFY(name)}

//variable length procedures for now accept only one function. When this function will return false, the procedure will be stopped
#define PROCEDURE_VAR_LEN(name, fun) uint8_t (*name##_sequence[])(void)={fun}; \
                            procedure_t name={0, name##_sequence, 0 ,stopped, STRINGIFY(name)}

uint8_t sequential_procedure_start(procedure_t *m_procedure, uint8_t delayed_start);
void sequential_procedure_execute_action(void);
void sequential_procedure_retry(void);
void sequential_procedure_stop(void);
void sequential_procedure_stop_this(procedure_t *m_procedure);
uint8_t sequential_procedure_is_running(void);
uint8_t sequential_procedure_is_this_running(procedure_t *m_procedure);

#endif
