/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Glenn Ruben Bakke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <mpconfigboard.h>

#if defined(NRF51822)
  #include "mpconfigdevice_nrf51822.h"
  #ifndef MICROPY_CONFIG_ROM_LEVEL
  #define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_MINIMUM)
  #endif
#elif defined(NRF52832)
  #include "mpconfigdevice_nrf52832.h"
#elif defined(NRF52840)
  #include "mpconfigdevice_nrf52840.h"
#elif defined(NRF9160)
  #include "mpconfigdevice_nrf9160.h"
#else
  #pragma error "Device not defined"
#endif

// Default feature level for processor
#ifndef MICROPY_CONFIG_ROM_LEVEL
#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES)
#endif

#define CORE_FEAT MICROPY_CONFIG_ROM_LEVEL_AT_LEAST_CORE_FEATURES
#define EXTRA_FEAT MICROPY_CONFIG_ROM_LEVEL_AT_LEAST_EXTRA_FEATURES

// options to control how MicroPython is built
#ifndef MICROPY_VFS
#define MICROPY_VFS                 (0)
#endif
#define MICROPY_ALLOC_PATH_MAX      (512)
#define MICROPY_PERSISTENT_CODE_LOAD (1)
#define MICROPY_COMP_MODULE_CONST   (EXTRA_FEAT)
#define MICROPY_COMP_TRIPLE_TUPLE_ASSIGN (EXTRA_FEAT)
#define MICROPY_READER_VFS          (MICROPY_VFS)
#define MICROPY_ENABLE_GC           (1)
#define MICROPY_ENABLE_FINALISER    (EXTRA_FEAT)
#define MICROPY_STACK_CHECK         (EXTRA_FEAT)
#define MICROPY_HELPER_REPL         (EXTRA_FEAT)
#define MICROPY_REPL_INFO           (1)
#define MICROPY_REPL_EMACS_KEYS     (EXTRA_FEAT)
#define MICROPY_REPL_AUTO_INDENT    (EXTRA_FEAT)
#define MICROPY_KBD_EXCEPTION       (EXTRA_FEAT)
#define MICROPY_LONGINT_IMPL        (MICROPY_LONGINT_IMPL_MPZ)
#if NRF51
#define MICROPY_FLOAT_IMPL          (MICROPY_FLOAT_IMPL_NONE)
#else
#define MICROPY_FLOAT_IMPL          (MICROPY_FLOAT_IMPL_FLOAT)
#endif
#if NRF51
#define MICROPY_ALLOC_GC_STACK_SIZE (32)
#endif

#define MICROPY_OPT_COMPUTED_GOTO   (0)
#define MICROPY_OPT_MPZ_BITWISE     (EXTRA_FEAT)

// fatfs configuration used in ffconf.h
#define MICROPY_FATFS_ENABLE_LFN       (1)
#define MICROPY_FATFS_LFN_CODE_PAGE    437 /* 1=SFN/ANSI 437=LFN/U.S.(OEM) */
#define MICROPY_FATFS_USE_LABEL        (1)
#define MICROPY_FATFS_RPATH            (2)
#define MICROPY_FATFS_MULTI_PARTITION  (0)

#if NRF51
    #define MICROPY_FATFS_MAX_SS       (1024)
#else
    #define MICROPY_FATFS_MAX_SS       (4096)
#endif

// use vfs's functions for import stat and builtin open
#if MICROPY_VFS
#define mp_import_stat mp_vfs_import_stat
#define mp_builtin_open mp_vfs_open
#define mp_builtin_open_obj mp_vfs_open_obj

#if MICROPY_VFS_FAT
#define mp_type_fileio mp_type_vfs_fat_fileio
#define mp_type_textio mp_type_vfs_fat_textio
#elif MICROPY_VFS_LFS1
#define mp_type_fileio mp_type_vfs_lfs1_fileio
#define mp_type_textio mp_type_vfs_lfs1_textio
#elif MICROPY_VFS_LFS2
#define mp_type_fileio mp_type_vfs_lfs2_fileio
#define mp_type_textio mp_type_vfs_lfs2_textio
#endif

#else // !MICROPY_VFS_FAT
#define mp_type_fileio fatfs_type_fileio
#define mp_type_textio fatfs_type_textio
#endif

// Enable micro:bit filesystem by default.
#ifndef MICROPY_MBFS
#define MICROPY_MBFS (1)
#endif

#define MICROPY_STREAMS_NON_BLOCK   (EXTRA_FEAT)
#define MICROPY_MODULE_WEAK_LINKS   (EXTRA_FEAT)
#define MICROPY_CAN_OVERRIDE_BUILTINS (EXTRA_FEAT)
#define MICROPY_USE_INTERNAL_ERRNO  (1)
#define MICROPY_PY_FUNCTION_ATTRS   (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_STR_UNICODE (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_STR_CENTER (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_STR_PARTITION (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_STR_SPLITLINES (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_MEMORYVIEW (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_FROZENSET (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_EXECFILE (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_COMPILE (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_HELP    (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_HELP_TEXT nrf5_help_text
#define MICROPY_PY_BUILTINS_HELP_MODULES (EXTRA_FEAT)
#define MICROPY_MODULE_BUILTIN_INIT (EXTRA_FEAT)
#define MICROPY_PY_ALL_SPECIAL_METHODS (EXTRA_FEAT)
#define MICROPY_PY_MICROPYTHON_MEM_INFO (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_SLICE_ATTRS (EXTRA_FEAT)
#define MICROPY_PY_SYS_EXIT         (1)
#define MICROPY_PY_SYS_MAXSIZE      (EXTRA_FEAT)
#define MICROPY_PY_SYS_STDIO_BUFFER (EXTRA_FEAT)
#define MICROPY_PY_COLLECTIONS_ORDEREDDICT (EXTRA_FEAT)
#define MICROPY_PY_MATH_SPECIAL_FUNCTIONS (EXTRA_FEAT)
#define MICROPY_PY_CMATH            (EXTRA_FEAT)
#define MICROPY_PY_IO               (CORE_FEAT)
#define MICROPY_PY_IO_FILEIO        (MICROPY_VFS_FAT || MICROPY_VFS_LFS1 || MICROPY_VFS_LFS2)
#define MICROPY_PY_URANDOM          (EXTRA_FEAT)
#define MICROPY_PY_URANDOM_EXTRA_FUNCS (EXTRA_FEAT)
#define MICROPY_PY_UCTYPES          (EXTRA_FEAT)
#define MICROPY_PY_UZLIB            (EXTRA_FEAT)
#define MICROPY_PY_UJSON            (EXTRA_FEAT)
#define MICROPY_PY_URE              (EXTRA_FEAT)
#define MICROPY_PY_UHEAPQ           (EXTRA_FEAT)
#define MICROPY_PY_UTIME_MP_HAL     (1)
#define MICROPY_PY_MACHINE          (1)
#define MICROPY_PY_MACHINE_PULSE    (0)
#define MICROPY_PY_MACHINE_SOFTI2C  (MICROPY_PY_MACHINE_I2C)
#define MICROPY_PY_MACHINE_SPI      (0)
#define MICROPY_PY_MACHINE_SPI_MIN_DELAY (0)
#define MICROPY_PY_FRAMEBUF         (EXTRA_FEAT)

#define MICROPY_COMP_RETURN_IF_EXPR (EXTRA_FEAT)
#define MICROPY_OPT_LOAD_ATTR_FAST_PATH (EXTRA_FEAT)
#define MICROPY_OPT_MAP_LOOKUP_CACHE (EXTRA_FEAT)
#define MICROPY_OPT_MATH_FACTORIAL (EXTRA_FEAT)
#define MICROPY_ENABLE_SOURCE_LINE (EXTRA_FEAT)
#define MICROPY_ENABLE_SCHEDULER (EXTRA_FEAT)
#define MICROPY_PY_DESCRIPTORS (EXTRA_FEAT)
#define MICROPY_PY_DELATTR_SETATTR (EXTRA_FEAT)
#define MICROPY_PY_FSTRINGS (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_SLICE_INDICES (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_ROUND_INT (EXTRA_FEAT)
#define MICROPY_PY_REVERSE_SPECIAL_METHODS (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_NOTIMPLEMENTED (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_INPUT (EXTRA_FEAT)
#define MICROPY_PY_BUILTINS_POW3 (EXTRA_FEAT)
#define MICROPY_PY_ARRAY_SLICE_ASSIGN (EXTRA_FEAT)
#define MICROPY_PY_COLLECTIONS_DEQUE (EXTRA_FEAT)
#define MICROPY_PY_MATH_CONSTANTS (EXTRA_FEAT)
#define MICROPY_PY_MATH_FACTORIAL (EXTRA_FEAT)
#define MICROPY_PY_MATH_ISCLOSE (EXTRA_FEAT)
#define MICROPY_PY_IO_IOBASE (EXTRA_FEAT)
#define MICROPY_PY_SYS_STDFILES (EXTRA_FEAT)
#define MICROPY_PY_UERRNO (EXTRA_FEAT)
#define MICROPY_PY_USELECT (EXTRA_FEAT)
#define MICROPY_PY_UASYNCIO (EXTRA_FEAT)
#define MICROPY_PY_URE_SUB (EXTRA_FEAT)
#define MICROPY_PY_UHASHLIB (EXTRA_FEAT)
#define MICROPY_PY_UBINASCII (EXTRA_FEAT)
#define MICROPY_PY_UBINASCII_CRC32 (EXTRA_FEAT)

#ifndef MICROPY_HW_LED_COUNT
#define MICROPY_HW_LED_COUNT        (0)
#endif

#ifndef MICROPY_HW_LED_PULLUP
#define MICROPY_HW_LED_PULLUP       (0)
#endif

#ifndef MICROPY_PY_MUSIC
#define MICROPY_PY_MUSIC            (0)
#endif

#ifndef MICROPY_PY_MACHINE_ADC
#define MICROPY_PY_MACHINE_ADC      (0)
#endif

#ifndef MICROPY_PY_MACHINE_I2C
#define MICROPY_PY_MACHINE_I2C      (0)
#endif

#ifndef MICROPY_PY_MACHINE_HW_SPI
#define MICROPY_PY_MACHINE_HW_SPI   (1)
#endif

#ifndef MICROPY_PY_MACHINE_HW_PWM
#define MICROPY_PY_MACHINE_HW_PWM   (0)
#endif

#ifndef MICROPY_PY_MACHINE_SOFT_PWM
#define MICROPY_PY_MACHINE_SOFT_PWM (0)
#endif

#ifndef MICROPY_PY_MACHINE_TIMER
#define MICROPY_PY_MACHINE_TIMER    (0)
#endif

#ifndef MICROPY_PY_MACHINE_RTCOUNTER
#define MICROPY_PY_MACHINE_RTCOUNTER (0)
#endif

#ifndef MICROPY_PY_TIME_TICKS
#define MICROPY_PY_TIME_TICKS       (1)
#endif

#ifndef MICROPY_PY_NRF
#define MICROPY_PY_NRF              (0)
#endif

#define MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF   (1)
#define MICROPY_EMERGENCY_EXCEPTION_BUF_SIZE  (0)

// if sdk is in use, import configuration
#if BLUETOOTH_SD
#include "bluetooth_conf.h"
#endif

#ifndef MICROPY_PY_UBLUEPY
#define MICROPY_PY_UBLUEPY                       (0)
#endif

#ifndef MICROPY_PY_BLE_NUS
#define MICROPY_PY_BLE_NUS                       (0)
#endif

// type definitions for the specific machine

#define MICROPY_MAKE_POINTER_CALLABLE(p) ((void *)((mp_uint_t)(p) | 1))

#define MP_SSIZE_MAX (0x7fffffff)

#define UINT_FMT "%u"
#define INT_FMT "%d"
#define HEX2_FMT "%02x"

typedef int mp_int_t; // must be pointer size
typedef unsigned int mp_uint_t; // must be pointer size
typedef long mp_off_t;

// extra built in modules to add to the list of known ones
extern const struct _mp_obj_module_t board_module;
extern const struct _mp_obj_module_t nrf_module;
extern const struct _mp_obj_module_t mp_module_utime;
extern const struct _mp_obj_module_t mp_module_uos;
extern const struct _mp_obj_module_t mp_module_ubluepy;
extern const struct _mp_obj_module_t music_module;

#if MICROPY_PY_NRF
#define NRF_MODULE                          { MP_ROM_QSTR(MP_QSTR_nrf), MP_ROM_PTR(&nrf_module) },
#else
#define NRF_MODULE
#endif

#if MICROPY_PY_UBLUEPY
#define UBLUEPY_MODULE                      { MP_ROM_QSTR(MP_QSTR_ubluepy), MP_ROM_PTR(&mp_module_ubluepy) },
#else
#define UBLUEPY_MODULE
#endif

#if MICROPY_PY_MUSIC
#define MUSIC_MODULE                        { MP_ROM_QSTR(MP_QSTR_music), MP_ROM_PTR(&music_module) },
#else
#define MUSIC_MODULE
#endif

#if BOARD_SPECIFIC_MODULES
#include "boardmodules.h"
#define MICROPY_BOARD_BUILTINS BOARD_MODULES
#else
#define MICROPY_BOARD_BUILTINS
#endif // BOARD_SPECIFIC_MODULES

#if BLUETOOTH_SD

#if MICROPY_PY_BLE
extern const struct _mp_obj_module_t ble_module;
#define BLE_MODULE                        { MP_ROM_QSTR(MP_QSTR_ble), MP_ROM_PTR(&ble_module) },
#else
#define BLE_MODULE
#endif

#define MICROPY_PORT_BUILTIN_MODULES \
    { MP_ROM_QSTR(MP_QSTR_board), MP_ROM_PTR(&board_module) }, \
    { MP_ROM_QSTR(MP_QSTR_utime), MP_ROM_PTR(&mp_module_utime) }, \
    { MP_ROM_QSTR(MP_QSTR_time), MP_ROM_PTR(&mp_module_utime) }, \
    { MP_ROM_QSTR(MP_QSTR_uos), MP_ROM_PTR(&mp_module_uos) }, \
    BLE_MODULE \
    MUSIC_MODULE \
    UBLUEPY_MODULE \
    MICROPY_BOARD_BUILTINS \
    NRF_MODULE \


#else
extern const struct _mp_obj_module_t ble_module;
#define MICROPY_PORT_BUILTIN_MODULES \
    { MP_ROM_QSTR(MP_QSTR_board), MP_ROM_PTR(&board_module) }, \
    { MP_ROM_QSTR(MP_QSTR_utime), MP_ROM_PTR(&mp_module_utime) }, \
    { MP_ROM_QSTR(MP_QSTR_uos), MP_ROM_PTR(&mp_module_uos) }, \
    MUSIC_MODULE \
    MICROPY_BOARD_BUILTINS \
    NRF_MODULE \


#define BLE_MODULE
#endif // BLUETOOTH_SD

// extra built in names to add to the global namespace
#define MICROPY_PORT_BUILTINS \
    { MP_ROM_QSTR(MP_QSTR_help), MP_ROM_PTR(&mp_builtin_help_obj) }, \
    { MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&mp_builtin_open_obj) }, \

// extra constants
#define MICROPY_PORT_CONSTANTS \
    { MP_ROM_QSTR(MP_QSTR_board), MP_ROM_PTR(&board_module) }, \
    { MP_ROM_QSTR(MP_QSTR_machine), MP_ROM_PTR(&mp_module_machine) }, \
    BLE_MODULE \

#define MP_STATE_PORT MP_STATE_VM

#if MICROPY_PY_MUSIC
#define ROOT_POINTERS_MUSIC \
    struct _music_data_t *music_data;
#else
#define ROOT_POINTERS_MUSIC
#endif

#if MICROPY_PY_MACHINE_SOFT_PWM
#define ROOT_POINTERS_SOFTPWM \
    const struct _pwm_events *pwm_active_events; \
    const struct _pwm_events *pwm_pending_events;
#else
#define ROOT_POINTERS_SOFTPWM
#endif

#if defined(NRF52840_XXAA)
#define NUM_OF_PINS 48
#else
#define NUM_OF_PINS 32
#endif

#define MICROPY_PORT_ROOT_POINTERS \
    const char *readline_hist[8]; \
    mp_obj_t pin_class_mapper; \
    mp_obj_t pin_class_map_dict; \
    mp_obj_t pin_irq_handlers[NUM_OF_PINS]; \
    \
    /* stdio is repeated on this UART object if it's not null */ \
    struct _machine_hard_uart_obj_t *board_stdio_uart; \
    \
    ROOT_POINTERS_MUSIC \
    ROOT_POINTERS_SOFTPWM \
    \
    /* micro:bit root pointers */ \
    void *async_data[2]; \

#define MICROPY_EVENT_POLL_HOOK \
    do { \
        extern void mp_handle_pending(bool); \
        mp_handle_pending(true); \
        __WFI(); \
    } while (0);

// We need to provide a declaration/definition of alloca()
#include <alloca.h>

#define MICROPY_PIN_DEFS_PORT_H "pin_defs_nrf5.h"

#ifndef MP_NEED_LOG2
#define MP_NEED_LOG2                (1)
#endif
