USE_MBOOT ?= 0

# MCU settings
MCU_SERIES = h5
CMSIS_MCU = STM32H503xx
MICROPY_FLOAT_IMPL = none
AF_FILE = boards/stm32h50_af.csv
MICROPY_VFS_FAT = 0
LD_FILES = boards/stm32h503rb.ld boards/common_basic.ld

# LTO reduces final binary size, may be slower to build depending on gcc version and hardware
LTO ?= 1

MPY_TOOL_FLAGS = mplongint-impl=none

# Don't include default frozen modules because MCU is tight on flash space
FROZEN_MANIFEST ?=
