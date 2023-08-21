#define MICROPY_HW_BOARD_NAME               "NUCLEO_H503RB"
#define MICROPY_HW_MCU_NAME                 "STM32H503RB"

#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_CORE_FEATURES)

#define MICROPY_PY_PYB_LEGACY               (0)

#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (0)
#define MICROPY_HW_HAS_FLASH                (0)

#define MICROPY_HW_ENABLE_RTC               (1)
#define MICROPY_HW_ENABLE_RNG               (0)
#define MICROPY_HW_ENABLE_ADC               (0)
#define MICROPY_HW_ENABLE_DAC               (0) // TODO: requires DMA, which is not yet ready
#define MICROPY_HW_HAS_SWITCH               (1)
#define MICROPY_ENABLE_FINALISER            (0)
#define MICROPY_HW_SDCARD_SDMMC             (0)
#define MICROPY_PY_STM                      (0)

#define MICROPY_KBD_EXCEPTION       (0)
#define MICROPY_ENABLE_SCHEDULER    (0)
#define MICROPY_OPT_COMPUTED_GOTO   (0)
#define MICROPY_PY_BUILTINS_COMPLEX (0)
#define MICROPY_OPT_MAP_LOOKUP_CACHE (0)
#define MICROPY_PY_GENERATOR_PEND_THROW (0)
#define MICROPY_PY_MATH             (0)
#define MICROPY_PY_FRAMEBUF         (0)
#define MICROPY_PY_SOCKET           (0)
#define MICROPY_PY_NETWORK          (0)
#define MICROPY_PY_ONEWIRE          (0)
#define MICROPY_PY_STM              (0)
#define MICROPY_PY_PYB (0)
#define MICROPY_PY_PYB_LEGACY       (0)
#define MICROPY_PY_HEAPQ            (0)
#define MICROPY_PY_OS               (0)
#define MICROPY_VFS (MICROPY_PY_OS)
#define MICROPY_ENABLE_EXTERNAL_IMPORT (MICROPY_VFS)
#define MICROPY_HW_ENABLE_HW_I2C (0)

// #define MICROPY_PY_MACHINE_SOFTI2C (0)
#define MICROPY_MODULE_FROZEN (0)

#define MICROPY_PY_TIME             (1)
#define MICROPY_EMIT_THUMB          (0)
#define MICROPY_EMIT_INLINE_THUMB   (0)
// #define MICROPY_DYNAMIC_COMPILER     (0)
// #define MICROPY_ENABLE_DYNRUNTIME (0)

#define MICROPY_PY_MACHINE_BITSTREAM (0)


// The board has a 25MHz oscillator, the following gives 250MHz CPU speed
// TODO(mst) Clock timings taken from STM32H573I_DK.
// TODO(mst) Currently only supports 25MHz oscillator, though Nucleo
#define MICROPY_HW_CLK_USE_BYPASS           (1)
#define MICROPY_HW_CLK_PLLM                 (5)
#define MICROPY_HW_CLK_PLLN                 (100)
#define MICROPY_HW_CLK_PLLP                 (2)
#define MICROPY_HW_CLK_PLLQ                 (2)
#define MICROPY_HW_CLK_PLLR                 (2)
#define MICROPY_HW_CLK_PLLVCI_LL            (LL_RCC_PLLINPUTRANGE_4_8)
#define MICROPY_HW_CLK_PLLVCO_LL            (LL_RCC_PLLVCORANGE_WIDE)
#define MICROPY_HW_CLK_PLLFRAC              (0)
#define MICROPY_HW_FLASH_LATENCY            FLASH_LATENCY_5

// PLL3 with Q output at 48MHz for USB
#define MICROPY_HW_CLK_PLL3M                (25)
#define MICROPY_HW_CLK_PLL3N                (192)
#define MICROPY_HW_CLK_PLL3P                (2)
#define MICROPY_HW_CLK_PLL3Q                (4)
#define MICROPY_HW_CLK_PLL3R                (2)
#define MICROPY_HW_CLK_PLL3FRAC             (0)
#define MICROPY_HW_CLK_PLL3VCI_LL           (LL_RCC_PLLINPUTRANGE_1_2)
#define MICROPY_HW_CLK_PLL3VCO_LL           (LL_RCC_PLLVCORANGE_MEDIUM)

// #define MICROPY_HW_CLK_USE_HSI (1)

// There is an external 32kHz oscillator
#define MICROPY_HW_RTC_USE_LSE              (1)

// UART config
#define MICROPY_HW_UART1_TX                 (pin_B14) // Arduino Connector CN10-Pin14 (D1)
#define MICROPY_HW_UART1_RX                 (pin_B15) // Arduino Connector CN10-Pin16 (D0)
#define MICROPY_HW_UART3_TX                 (pin_A4) // ST-Link
#define MICROPY_HW_UART3_RX                 (pin_A3) // ST-Link

// Connect REPL to UART3 which is provided on ST-Link USB interface
#define MICROPY_HW_UART_REPL                PYB_UART_3
#define MICROPY_HW_UART_REPL_BAUD           115200

// I2C buses
#define MICROPY_HW_I2C1_SCL                 (pin_B6)
#define MICROPY_HW_I2C1_SDA                 (pin_B7)

// SPI buses - not yet working due to missing DMA support
// PD14 according to datasheet not working as SPI1_NSS, have to use as GPIO, not as AF
// #define MICROPY_HW_SPI1_NSS                 (pin_D14) // Arduino Connector CN7-Pin16 (D10)
// #define MICROPY_HW_SPI1_SCK                 (pin_A5) // Arduino Connector CN7-Pin10 (D13)
// #define MICROPY_HW_SPI1_MISO                (pin_G9) // Arduino Connector CN7-Pin12 (D12)
// #define MICROPY_HW_SPI1_MOSI                (pin_B5) // Arduino Connector CN7-Pin14 (D11)

// USRSW is pulled low. Pressing the button makes the input go high.
#define MICROPY_HW_USRSW_PIN                (pin_C13)
#define MICROPY_HW_USRSW_PULL               (GPIO_NOPULL)
#define MICROPY_HW_USRSW_EXTI_MODE          (GPIO_MODE_IT_RISING)
#define MICROPY_HW_USRSW_PRESSED            (1)

// LEDs
#define MICROPY_HW_LED1                     (pin_A5) // Green
#define MICROPY_HW_LED_ON(pin)              (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin)             (mp_hal_pin_low(pin))

// USB config
// #define MICROPY_HW_USB_MAIN_DEV             (USB_PHY_FS_ID)
#define MICROPY_HW_ENABLE_USB               (0)
#define MICROPY_HW_USB_FS           (0)
#define MICROPY_HW_USB_MSC          (0)
#define MICROPY_HW_USB_HID          (0)

// #define MICROPY_ENABLE_COMPILER (0)
