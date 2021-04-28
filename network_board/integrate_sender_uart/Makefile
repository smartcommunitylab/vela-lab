DEFINES+=PROJECT_CONF_H=\"project-conf.h\"
CONTIKI_PROJECT = {CONTIKI_PROJECT}

CONTIKI =${CONTIKI_ROOT}
COMMON_DIR=../../common

CFLAGS += -DROM_BOOTLOADER_ENABLE=1
CFLAGS += -DSERIAL_LINE_CONF_BUFSIZE=$(SERIAL_LINE_CONF_BUFSIZE)
CFLAGS += -DVERSION_STRING=$(VERSION_STRING)
CFLAGS += -DCC26XX_UART_CONF_BAUD_RATE=$(CC26XX_UART_CONF_BAUD_RATE)
CFLAGS += -DOTA_DEBUG=${OTA}
CFLAGS += -DSMALL=1
CFLAGS += -DSTACK_CHECK_CONF_ENABLED=1
CFLAGS += -DUIP_CONF_BUFFER_SIZE=$(UIP_CONF_BUFFER_SIZE)
CFLAGS += -g3

#COMMON_DIR_REL_PATH=$(shell python -c "import os.path; print os.path.relpath('${COMMON_DIR}', '${CONTIKI}')")
#PROJ_DIR_REL_PATH=$(shell python -c "import os.path; print os.path.relpath('./../external_modules', '${CONTIKI}')")
COMMON_DIR_REL_PATH=../../common
PROJ_DIR_REL_PATH=../external_modules
PROJECT_PATH=../integrate_sender_uart
$(info $$COMMON_DIR_REL_PATH is: ${COMMON_DIR_REL_PATH})
$(info $$PROJ_DIR_REL_PATH is: ${PROJ_DIR_REL_PATH})

MAKE_MAC = MAKE_MAC_TSCH

MODULES+=${COMMON_DIR_REL_PATH}/inc
MODULES += os/services/orchestra

ifeq ($(SINK),1)
MODULES+=${PROJECT_PATH}/sink_receiver
endif

ifeq ($(NODE),1)
MODULES+=${COMMON_DIR_REL_PATH}/lib/uart_util
MODULES+=${COMMON_DIR_REL_PATH}/lib/sequential_procedures
MODULES+=${PROJECT_PATH}/vela_uart       #this is the only difference between basic_sender_test and integrate_sender_uart
MODULES+=${PROJECT_PATH}/vela_sender
endif

ifeq ($(OTA),1)
MODULES += ${PROJ_DIR_REL_PATH}/ota
MODULES += ${PROJ_DIR_REL_PATH}/ota-download
endif

WERROR=0

BSL_FLAGS += -b 115200

$(CONTIKI_PROJECT): $(CONTIKI_PROJECT)
    include $(CONTIKI)/Makefile.include \

