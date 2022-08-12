# This module is intended for use on stm32wb55 microcontrollers
# and shows how a native module can be used to extend hardware
# functionality.

# Note: `pip install pyelftools` is required to build native modules.

# Location of top-level MicroPython directory
MPY_DIR = ../../..

# Name of module
MOD = transparent_vcp

# Source files (.c or .py)
SRC = \
 _stm32wb55_transparent.c \
 stm32wb55_transparent_vcp.py \


# $(MPY_DIR)/ports/stm32/rfcore.c

# Architecture to build for (x86, x64, armv7m, xtensa, xtensawin)
ARCH = armv7m

MCU_SERIES = wb
CMSIS_MCU = STM32WB55xx
include  $(MPY_DIR)/ports/stm32/stm32.mk

CFLAGS += -D$(CMSIS_MCU) -DUSE_FULL_LL_DRIVER $(CFLAGS_MCU_$(MCU_SERIES))
# CFLAGS += -DSTM32_HAL_H='<stm32$(MCU_SERIES)xx_hal.h>'
CFLAGS += -DSTM32_HAL_H='<stm32$(MCU_SERIES)xx_hal.h>'
CFLAGS += -DMICROPY_PIN_DEFS_PORT_H='<rfcore_settings.h>'
CFLAGS += -Dpin_gpio_t=uint32_t
# CFLAGS += -Dmp_obj_new_int_from_uint=
CFLAGS += -DMICROPY_PY_BLUETOOTH=0
CFLAGS += -DMICROPY_BLUETOOTH_NIMBLE=0
CFLAGS += -DMICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS=1

CFLAGS += -I$(MPY_DIR)/ports/stm32
CFLAGS += -I$(MPY_DIR)/ports/stm32/boards/NUCLEO_WB55
CFLAGS += -I$(MPY_DIR)/lib/cmsis/inc
CFLAGS += -I$(MPY_DIR)/lib/stm32lib/STM32WBxx_HAL_Driver/Inc
CFLAGS += -I$(MPY_DIR)/lib/stm32lib/CMSIS/STM32WBxx/Include
CFLAGS += -I$(MPY_DIR)/lib/mynewt-nimble/nimble/include
CFLAGS += -I$(MPY_DIR)/extmod/nimble

include $(MPY_DIR)/py/dynruntime.mk
