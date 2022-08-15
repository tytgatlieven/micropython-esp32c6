USE_MBOOT = 0
USE_PYDFU = 0
# For dual core HAL drivers.
CFLAGS += -DCORE_CM7

# Arduino bootloader PID:VID
BOOTLOADER_DFU_USB_VID = 0x2341
BOOTLOADER_DFU_USB_PID = 0x035b

# MCU settings
MCU_SERIES = h7
CMSIS_MCU = STM32H747xx
MICROPY_FLOAT_IMPL = single
AF_FILE = boards/stm32h743_af.csv
LD_FILES = boards/ARDUINO_PORTENTA_H7/stm32h747.ld
TEXT0_ADDR = 0x08040000

# MicroPython settings
MICROPY_PY_BLUETOOTH = 1
MICROPY_BLUETOOTH_NIMBLE = 1
MICROPY_BLUETOOTH_BTSTACK = 0
MICROPY_PY_LWIP = 1
MICROPY_PY_NETWORK_CYW43 = 1
MICROPY_PY_SSL = 1
MICROPY_SSL_MBEDTLS = 1
MICROPY_USE_RAM_ISR_UART_FLASH_FN = 1

FROZEN_MANIFEST = $(BOARD_DIR)/manifest.py
MBEDTLS_CONFIG_FILE = '"$(BOARD_DIR)/mbedtls_config_board.h"'
