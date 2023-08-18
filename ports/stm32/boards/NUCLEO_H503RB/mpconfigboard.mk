USE_MBOOT ?= 0

# MCU settings
MCU_SERIES = h5
CMSIS_MCU = STM32H503xx
MICROPY_FLOAT_IMPL = single
AF_FILE = boards/stm32h573_af.csv

ifeq ($(USE_MBOOT),1)
# When using Mboot everything goes after the bootloader
# TODO(mst) MBOOT will not work (no FLASH_APP defined - awfully tight for flash!)
LD_FILES = boards/stm32h503rb.ld boards/common_bl.ld
TEXT0_ADDR = 0x08008000
else
# When not using Mboot everything goes at the start of flash
LD_FILES = boards/stm32h503rb.ld boards/common_basic.ld
TEXT0_ADDR = 0x08000000
endif
