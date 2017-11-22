CONTIKI =/Users/murphy/Projects/VelaLab/contiki

# CONTIKI =/Users/rajeev/git/wur_sim/contiki_orig



# all: main_stub vela_receiver
all: vela_node vela_sink

APPS=servreg-hack

TARGET ?= srf06-cc26xx

#BOARD  ?= sensortag/cc2650
BOARD  ?= launchpad/cc2650

# TARGET ?= sky

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

PROJECT_SOURCEFILES += vela_uart.c
PROJECT_SOURCEFILES += vela_sender.c
PROJECT_SOURCEFILES += sink_receiver.c

CONTIKI_WITH_RPL=1
CONTIKI_WITH_IPV6 = 1
include $(CONTIKI)/Makefile.include
