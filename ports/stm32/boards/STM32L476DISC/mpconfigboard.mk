MCU_SERIES = l4
CMSIS_MCU = STM32L476xx
AF_FILE = boards/stm32l476_af.csv
LD_FILES = boards/stm32l476xg.ld boards/common_basic.ld
OPENOCD_CONFIG = boards/openocd_stm32l4.cfg

MICROPY_USE_RAM_ISR_UART_FLASH_FN = 1
