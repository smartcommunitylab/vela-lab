DEFINES+=PROJECT_CONF_H=\"project-conf.h\"
CONTIKI_PROJECT = vela_node vela_sink

all: $(CONTIKI_PROJECT)
#APPS=servreg-hack

CONTIKI =${CONTIKI_ROOT}
COMMON_DIR=../../common

#CONTIKI_WITH_RIME = 1
CFLAGS += -DROM_BOOTLOADER_ENABLE=1
CFLAGS += -DSERIAL_LINE_CONF_BUFSIZE=128
CFLAGS += -DVERSION_STRING=$(VERSION_STRING)
CFLAGS += -DCC26XX_UART_CONF_BAUD_RATE=1000000 #921600 #1000000
CFLAGS += -DSMALL=1
#CFLAGS += -DDEBUG=0


COMMON_DIR_REL_PATH=../vela-lab/common
#COMMON_DIR_REL_PATH=$(shell python -c "import os.path; print os.path.relpath('${COMMON_DIR}', '${CONTIKI}')")
$(info $$COMMON_DIR_REL_PATH is: ${COMMON_DIR_REL_PATH})

# force Orchestra from command line
MAKE_WITH_ORCHESTRA ?= 1
ifeq ($(MAKE_WITH_ORCHESTRA),1)
MODULES += os/services/orchestra
endif
 # print #routes periodically, used for regression tests
#MAKE_WITH_PERIODIC_ROUTES_PRINT ?= 0

MAKE_MAC = MAKE_MAC_TSCH
MAKE_ROUTING = MAKE_ROUTING_RPL_CLASSIC


MODULES+=${COMMON_DIR_REL_PATH}/lib/uart_util
MODULES+=${COMMON_DIR_REL_PATH}/inc
MODULES+=${COMMON_DIR_REL_PATH}/lib/sequential_procedures

PROJECT_SOURCEFILES += vela_uart.c
PROJECT_SOURCEFILES += vela_sender.c
PROJECT_SOURCEFILES += sink_receiver.c

WERROR=0

TARGET ?= cc26x0-cc13x0

#BOARD=launchpad/cc2650
#use this one for the nodes that DO NOT HAVE the new interface board with the solar panel
#BOARD=sensortag/cc2650
#BOARD=launchpad_vela/cc2650
#BOARD=sensortag/cc1350
BOARD=launchpad_vela/cc1350

BSL_FLAGS += -b 115200

CONTIKI_WITH_RPL=1
CONTIKI_WITH_IPV6 = 1

include $(CONTIKI)/Makefile.include

