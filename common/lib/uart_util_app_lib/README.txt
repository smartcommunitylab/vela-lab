
add to makefile
CFLAGS += -DAPP_UART_ENABLED
CFLAGS += -DAPP_FIFO_ENABLED

$(SDK_ROOT)/components/libraries/fifo/app_fifo.c
$(SDK_ROOT)/components/libraries/uart/app_uart_fifo.c