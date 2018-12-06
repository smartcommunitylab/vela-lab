DEFINES+=PROJECT_CONF_H=\"project-conf.h\"
CONTIKI_PROJECT = vela_node vela_sink

all: $(CONTIKI_PROJECT)
APPS=servreg-hack

CONTIKI =${CONTIKI_ROOT}
COMMON_DIR=../../common

#CONTIKI_WITH_RIME = 1
CFLAGS += -DROM_BOOTLOADER_ENABLE=1
CFLAGS += -DSERIAL_LINE_CONF_BUFSIZE=128
CFLAGS += -DVERSION_STRING=$(VERSION_STRING)
CFLAGS += -DCC26XX_UART_CONF_BAUD_RATE=1000000

COMMON_DIR_REL_PATH=$(shell python -c "import os.path; print os.path.relpath('${COMMON_DIR}', '${CONTIKI}')")
$(info $$COMMON_DIR_REL_PATH is: ${COMMON_DIR_REL_PATH})

MODULES+=${COMMON_DIR_REL_PATH}/lib/uart_util
MODULES+=${COMMON_DIR_REL_PATH}/inc

PROJECT_SOURCEFILES += vela_uart.c
PROJECT_SOURCEFILES += vela_sender.c
PROJECT_SOURCEFILES += sink_receiver.c

TARGET ?= srf06-cc26xx
#BOARD=launchpad/cc2650
#use this one for the nodes that DO NOT HAVE the new interface board with the solar panel
#BOARD=launchpad_vela/cc2650
BOARD=launchpad_vela/cc1350

BSL_FLAGS += -b 115200

CONTIKI_WITH_RPL=1
CONTIKI_WITH_IPV6 = 1

include $(CONTIKI)/Makefile.include
