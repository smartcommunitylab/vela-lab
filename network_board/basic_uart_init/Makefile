DEFINES+=PROJECT_CONF_H=\"project-conf.h\"
CONTIKI_PROJECT = test-uart

all: $(CONTIKI_PROJECT)

CONTIKI=${CONTIKI_ROOT}
COMMON_DIR=../../common

CONTIKI_WITH_RIME = 1
CFLAGS += -DROM_BOOTLOADER_ENABLE
CFLAGS += -DSERIAL_LINE_CONF_BUFSIZE=256

EXTERNAL_MODULE=${COMMON_DIR}/lib/uart_util
$(info $$EXTERNAL_MODULE is: ${EXTERNAL_MODULE})

EXT_MOD_CONTIKI_REL_PATH=$(shell python -c "import os.path; print os.path.relpath('${EXTERNAL_MODULE}', '${CONTIKI}')")
$(info $$EXT_MOD_CONTIKI_REL_PATH is: ${EXT_MOD_CONTIKI_REL_PATH})

# the next line is an alternative way for computing EXT_MOD_CONTIKI_REL_PATH, but in doesn't work inside make (it works from the console)
# EXT_MOD_CONTIKI_REL_PATH=$(realpath --relative-to=${CONTIKI} ${EXTERNAL_MODULE})

MODULES+=${EXT_MOD_CONTIKI_REL_PATH}



include $(CONTIKI)/Makefile.include
