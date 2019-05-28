/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Damien P. George
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

#include <string.h>

#include "py/mperrno.h"
#include "py/mphal.h"
#include "qspi.h"
#include "pin_static_af.h"

#if defined(MICROPY_HW_QSPIFLASH_SIZE_BYTES) || defined(MICROPY_HW_QSPIFLASH_SIZE_BITS_LOG2)

#ifndef MICROPY_HW_QSPI_PRESCALER
#define MICROPY_HW_QSPI_PRESCALER            3  // F_CLK = F_AHB/3 (72MHz when CPU is 216MHz)
#endif

#ifndef MICROPY_HW_QSPI_SAMPLE_SHIFT
#define MICROPY_HW_QSPI_SAMPLE_SHIFT         1  // sample shift enabled
#endif

#ifndef MICROPY_HW_QSPI_TIMEOUT_COUNTER
#define MICROPY_HW_QSPI_TIMEOUT_COUNTER      0  // timeout counter disabled (see F7 errata)
#endif

#ifndef MICROPY_HW_QSPI_CS_HIGH_CYCLES
#define MICROPY_HW_QSPI_CS_HIGH_CYCLES       2  // nCS stays high for 2 cycles
#endif

#ifndef MICROPY_HW_QSPI_ENABLE_MPU_CACHING
#define MICROPY_HW_QSPI_ENABLE_MPU_CACHING   0 // MPU deny access by default
#endif

#ifndef MICROPY_HW_QSPI_ENABLE_MEMORY_MAPPED
#define MICROPY_HW_QSPI_ENABLE_MEMORY_MAPPED 1 // Memory mapped mode enabled in idle
#endif

// Provides the MPU_REGION_SIZE_X value when passed the size of region in bytes
// "m" must be a power of 2 between 32 and 4G (2**5 and 2**32) and this formula
// computes the log2 of "m", minus 1
#define MPU_REGION_SIZE(m) (((m) - 1) / (((m) - 1) % 255 + 1) / 255 % 255 * 8 + 7 - 86 / (((m) - 1) % 255 + 12) - 1)

#define QSPI_MPU_REGION_SIZE (MPU_REGION_SIZE(MICROPY_HW_QSPIFLASH_SIZE_BYTES))

#if MICROPY_HW_QSPIFLASH_SIZE_BYTES
#define QSPI_DCR_FSIZE (MPU_REGION_SIZE(MICROPY_HW_QSPIFLASH_SIZE_BYTES))
#else
#define QSPI_DCR_FSIZE (MICROPY_HW_QSPIFLASH_SIZE_BITS_LOG2 -3 -1)
#endif


void qspi_init(void) {
    // Configure pins
    mp_hal_pin_config_alt_static_speed(MICROPY_HW_QSPIFLASH_CS, MP_HAL_PIN_MODE_ALT, MP_HAL_PIN_PULL_NONE, MP_HAL_PIN_SPEED_VERY_HIGH, STATIC_AF_QUADSPI_BK1_NCS);
    mp_hal_pin_config_alt_static_speed(MICROPY_HW_QSPIFLASH_SCK, MP_HAL_PIN_MODE_ALT, MP_HAL_PIN_PULL_NONE, MP_HAL_PIN_SPEED_VERY_HIGH, STATIC_AF_QUADSPI_CLK);
    mp_hal_pin_config_alt_static_speed(MICROPY_HW_QSPIFLASH_IO0, MP_HAL_PIN_MODE_ALT, MP_HAL_PIN_PULL_NONE, MP_HAL_PIN_SPEED_VERY_HIGH, STATIC_AF_QUADSPI_BK1_IO0);
    mp_hal_pin_config_alt_static_speed(MICROPY_HW_QSPIFLASH_IO1, MP_HAL_PIN_MODE_ALT, MP_HAL_PIN_PULL_NONE, MP_HAL_PIN_SPEED_VERY_HIGH, STATIC_AF_QUADSPI_BK1_IO1);
    mp_hal_pin_config_alt_static_speed(MICROPY_HW_QSPIFLASH_IO2, MP_HAL_PIN_MODE_ALT, MP_HAL_PIN_PULL_NONE, MP_HAL_PIN_SPEED_VERY_HIGH, STATIC_AF_QUADSPI_BK1_IO2);
    mp_hal_pin_config_alt_static_speed(MICROPY_HW_QSPIFLASH_IO3, MP_HAL_PIN_MODE_ALT, MP_HAL_PIN_PULL_NONE, MP_HAL_PIN_SPEED_VERY_HIGH, STATIC_AF_QUADSPI_BK1_IO3);

    // Bring up the QSPI peripheral

    __HAL_RCC_QSPI_CLK_ENABLE();
    __HAL_RCC_QSPI_FORCE_RESET();
    __HAL_RCC_QSPI_RELEASE_RESET();

    QUADSPI->CR =
        (MICROPY_HW_QSPI_PRESCALER - 1) << QUADSPI_CR_PRESCALER_Pos
        | 3 << QUADSPI_CR_FTHRES_Pos // 4 bytes must be available to read/write
        #if defined(QUADSPI_CR_FSEL_Pos)
        | 0 << QUADSPI_CR_FSEL_Pos // FLASH 1 selected
        #endif
        #if defined(QUADSPI_CR_DFM_Pos)
        | 0 << QUADSPI_CR_DFM_Pos // dual-flash mode disabled
        #endif
        | MICROPY_HW_QSPI_SAMPLE_SHIFT << QUADSPI_CR_SSHIFT_Pos
        | MICROPY_HW_QSPI_TIMEOUT_COUNTER << QUADSPI_CR_TCEN_Pos
        | 1 << QUADSPI_CR_EN_Pos // enable the peripheral
        ;

    QUADSPI->DCR =
        QSPI_DCR_FSIZE << QUADSPI_DCR_FSIZE_Pos
        | (MICROPY_HW_QSPI_CS_HIGH_CYCLES - 1) << QUADSPI_DCR_CSHT_Pos
        | 0 << QUADSPI_DCR_CKMODE_Pos // CLK idles at low state
        ;

    // Configure explicit MPU Access to QSPI memory region

    // Disable MPU
    __DMB();
    SCB->SHCSR &= ~SCB_SHCSR_MEMFAULTENA_Msk;
    MPU->CTRL = 0;

    // Config MPU to disable speculative acces to entire qspi region
    MPU->RNR = MPU_REGION_NUMBER1;
    MPU->RBAR = 0x90000000;
    MPU->RASR = MPU_INSTRUCTION_ACCESS_DISABLE  << MPU_RASR_XN_Pos   |
                MPU_REGION_NO_ACCESS            << MPU_RASR_AP_Pos   |
                MPU_TEX_LEVEL0                  << MPU_RASR_TEX_Pos  |
                MPU_ACCESS_NOT_SHAREABLE        << MPU_RASR_S_Pos    |
                MPU_ACCESS_NOT_CACHEABLE        << MPU_RASR_C_Pos    |
                MPU_ACCESS_NOT_BUFFERABLE       << MPU_RASR_B_Pos    |
                0x00                            << MPU_RASR_SRD_Pos  |
                MPU_REGION_SIZE_256MB           << MPU_RASR_SIZE_Pos |
                MPU_REGION_ENABLE               << MPU_RASR_ENABLE_Pos;
    __ISB();
    __DSB();
    __DMB();

#if MICROPY_HW_QSPI_ENABLE_MPU_CACHING
    // Config MPU to allow normal access to active qspi region
    MPU->RNR = MPU_REGION_NUMBER2;
    MPU->RBAR = 0x90000000;
    MPU->RASR = MPU_INSTRUCTION_ACCESS_DISABLE  << MPU_RASR_XN_Pos   |
                MPU_REGION_PRIV_RO              << MPU_RASR_AP_Pos   |
                MPU_TEX_LEVEL0                  << MPU_RASR_TEX_Pos  |
                MPU_ACCESS_NOT_SHAREABLE        << MPU_RASR_S_Pos    |
                MPU_ACCESS_CACHEABLE            << MPU_RASR_C_Pos    |
                MPU_ACCESS_NOT_BUFFERABLE       << MPU_RASR_B_Pos    |
                0x00                            << MPU_RASR_SRD_Pos  |
                QSPI_MPU_REGION_SIZE            << MPU_RASR_SIZE_Pos |
                MPU_REGION_ENABLE               << MPU_RASR_ENABLE_Pos;
    __ISB();
    __DSB();
    __DMB();
#endif

    // Enable MPU
    MPU->CTRL = MPU_PRIVILEGED_DEFAULT | MPU_CTRL_ENABLE_Msk;
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
    __DSB();
    __ISB();
}

void qspi_memory_map(void) {
#if MICROPY_HW_QSPI_ENABLE_MEMORY_MAPPED

    // Enable memory-mapped mode

    QUADSPI->ABR = 0; // disable continuous read mode

    // Clear fifo
    QUADSPI->CR |= QUADSPI_CR_ABORT;
    while (QUADSPI->CR & QUADSPI_CR_ABORT) {
    }

    // For chip addresses over 16MB, 32 bit addressing is required instead of 24 bit
    uint8_t cmd = 0xeb; // quad read opcode
    uint8_t adsize = 2; // 24-bit address size
    if (QSPI_DCR_FSIZE > 24) {
        cmd = 0xeb; // quad read with 32bit address opcode
        adsize = 3; // 32-bit address size
    }

    QUADSPI->CCR =
        0 << QUADSPI_CCR_DDRM_Pos // DDR mode disabled
        | 0 << QUADSPI_CCR_SIOO_Pos // send instruction every transaction
        | 3 << QUADSPI_CCR_FMODE_Pos // memory-mapped mode
        | 3 << QUADSPI_CCR_DMODE_Pos // data on 4 lines
        | 0 << QUADSPI_CCR_DCYC_Pos // 0 dummy cycles
        | 1 << QUADSPI_CCR_ABSIZE_Pos // 16-bit alternate byte
        | 1 << QUADSPI_CCR_ABMODE_Pos // alternate byte on 1 lines
        | adsize << QUADSPI_CCR_ADSIZE_Pos
        | 3 << QUADSPI_CCR_ADMODE_Pos // address on 4 lines
        | 1 << QUADSPI_CCR_IMODE_Pos // instruction on 1 line
        | cmd << QUADSPI_CCR_INSTRUCTION_Pos
        ;

#else // ! MICROPY_HW_QSPI_ENABLE_MEMORY_MAPPED

    // Disable memory mapped mode entirely
    return;
#endif
}

STATIC int qspi_ioctl(void *self_in, uint32_t cmd) {
    (void)self_in;
    switch (cmd) {
        case MP_QSPI_IOCTL_INIT:
            qspi_init();
            break;
        case MP_QSPI_IOCTL_BUS_RELEASE:
            // Switch to memory-map mode when bus is idle
            qspi_memory_map();
            break;
    }
    return 0; // success
}

STATIC void qspi_write_cmd_data(void *self_in, uint8_t cmd, size_t len, uint32_t data) {
    (void)self_in;

    if (QUADSPI->SR & QUADSPI_SR_BUSY) {
        QUADSPI->CR |= QUADSPI_CR_ABORT;
        while (QUADSPI->CR & QUADSPI_CR_ABORT) {
        }
    }

    QUADSPI->FCR = QUADSPI_FCR_CTCF; // clear TC flag

    if (len == 0) {
        QUADSPI->CCR =
            0 << QUADSPI_CCR_DDRM_Pos // DDR mode disabled
            | 0 << QUADSPI_CCR_SIOO_Pos // send instruction every transaction
            | 0 << QUADSPI_CCR_FMODE_Pos // indirect write mode
            | 0 << QUADSPI_CCR_DMODE_Pos // no data
            | 0 << QUADSPI_CCR_DCYC_Pos // 0 dummy cycles
            | 0 << QUADSPI_CCR_ABMODE_Pos // no alternate byte
            | 0 << QUADSPI_CCR_ADMODE_Pos // no address
            | 1 << QUADSPI_CCR_IMODE_Pos // instruction on 1 line
            | cmd << QUADSPI_CCR_INSTRUCTION_Pos // write opcode
            ;
    } else {
        QUADSPI->DLR = len - 1;

        QUADSPI->CCR =
            0 << QUADSPI_CCR_DDRM_Pos // DDR mode disabled
            | 0 << QUADSPI_CCR_SIOO_Pos // send instruction every transaction
            | 0 << QUADSPI_CCR_FMODE_Pos // indirect write mode
            | 1 << QUADSPI_CCR_DMODE_Pos // data on 1 line
            | 0 << QUADSPI_CCR_DCYC_Pos // 0 dummy cycles
            | 0 << QUADSPI_CCR_ABMODE_Pos // no alternate byte
            | 0 << QUADSPI_CCR_ADMODE_Pos // no address
            | 1 << QUADSPI_CCR_IMODE_Pos // instruction on 1 line
            | cmd << QUADSPI_CCR_INSTRUCTION_Pos // write opcode
            ;

        // This assumes len==2
        *(uint16_t*)&QUADSPI->DR = data;
    }

    // Wait for write to finish
    while (!(QUADSPI->SR & QUADSPI_SR_TCF)) {
    }

    QUADSPI->FCR = QUADSPI_FCR_CTCF; // clear TC flag
}

STATIC void qspi_write_cmd_addr_data(void *self_in, uint8_t cmd, uint32_t addr, uint8_t addr_bytes, size_t len, const uint8_t *src) {
    (void)self_in;

    if (QUADSPI->SR & QUADSPI_SR_BUSY) {
        QUADSPI->CR |= QUADSPI_CR_ABORT;
        while (QUADSPI->CR & QUADSPI_CR_ABORT) {
        }
    }

    QUADSPI->FCR = QUADSPI_FCR_CTCF; // clear TC flag

    if (len == 0) {
        QUADSPI->CCR =
            0 << QUADSPI_CCR_DDRM_Pos // DDR mode disabled
            | 0 << QUADSPI_CCR_SIOO_Pos // send instruction every transaction
            | 0 << QUADSPI_CCR_FMODE_Pos // indirect write mode
            | 0 << QUADSPI_CCR_DMODE_Pos // no data
            | 0 << QUADSPI_CCR_DCYC_Pos // 0 dummy cycles
            | 0 << QUADSPI_CCR_ABMODE_Pos // no alternate byte
            | (addr_bytes-1) << QUADSPI_CCR_ADSIZE_Pos
            | 1 << QUADSPI_CCR_ADMODE_Pos // address on 1 line
            | 1 << QUADSPI_CCR_IMODE_Pos // instruction on 1 line
            | cmd << QUADSPI_CCR_INSTRUCTION_Pos // write opcode
            ;

        QUADSPI->AR = addr;
    } else {
        QUADSPI->DLR = len - 1;

        QUADSPI->CCR =
            0 << QUADSPI_CCR_DDRM_Pos // DDR mode disabled
            | 0 << QUADSPI_CCR_SIOO_Pos // send instruction every transaction
            | 0 << QUADSPI_CCR_FMODE_Pos // indirect write mode
            | 1 << QUADSPI_CCR_DMODE_Pos // data on 1 line
            | 0 << QUADSPI_CCR_DCYC_Pos // 0 dummy cycles
            | 0 << QUADSPI_CCR_ABMODE_Pos // no alternate byte
            | (addr_bytes-1) << QUADSPI_CCR_ADSIZE_Pos
            | 1 << QUADSPI_CCR_ADMODE_Pos // address on 1 line
            | 1 << QUADSPI_CCR_IMODE_Pos // instruction on 1 line
            | cmd << QUADSPI_CCR_INSTRUCTION_Pos // write opcode
            ;

        QUADSPI->AR = addr;

        // Write out the data 1 byte at a time
        while (len) {
            while (!(QUADSPI->SR & QUADSPI_SR_FTF)) {
            }
            *(volatile uint8_t*)&QUADSPI->DR = *src++;
            --len;
        }
    }

    // Wait for write to finish
    while (!(QUADSPI->SR & QUADSPI_SR_TCF)) {
    }

    QUADSPI->FCR = QUADSPI_FCR_CTCF; // clear TC flag
}

STATIC uint32_t qspi_read_cmd(void *self_in, uint8_t cmd, size_t len) {
    (void)self_in;

    if (QUADSPI->SR & QUADSPI_SR_BUSY) {
        QUADSPI->CR |= QUADSPI_CR_ABORT;
        while (QUADSPI->CR & QUADSPI_CR_ABORT) {
        }
    }

    QUADSPI->FCR = QUADSPI_FCR_CTCF; // clear TC flag

    QUADSPI->DLR = len - 1; // number of bytes to read

    QUADSPI->CCR =
        0 << QUADSPI_CCR_DDRM_Pos // DDR mode disabled
        | 0 << QUADSPI_CCR_SIOO_Pos // send instruction every transaction
        | 1 << QUADSPI_CCR_FMODE_Pos // indirect read mode
        | 1 << QUADSPI_CCR_DMODE_Pos // data on 1 line
        | 0 << QUADSPI_CCR_DCYC_Pos // 0 dummy cycles
        | 0 << QUADSPI_CCR_ABMODE_Pos // no alternate byte
        | 0 << QUADSPI_CCR_ADMODE_Pos // no address
        | 1 << QUADSPI_CCR_IMODE_Pos // instruction on 1 line
        | cmd << QUADSPI_CCR_INSTRUCTION_Pos // read opcode
        ;

    // Wait for read to finish
    while (!(QUADSPI->SR & QUADSPI_SR_TCF)) {
    }

    QUADSPI->FCR = QUADSPI_FCR_CTCF; // clear TC flag

    // Read result
    return QUADSPI->DR;
}

STATIC void qspi_read_cmd_qaddr_qdata(void *self_in, uint8_t cmd, uint32_t addr, uint8_t addr_bytes, size_t len, uint8_t *dest) {
    (void)self_in;

    if (QUADSPI->SR & QUADSPI_SR_BUSY) {
        QUADSPI->CR |= QUADSPI_CR_ABORT;
        while (QUADSPI->CR & QUADSPI_CR_ABORT) {
        }
    }

    QUADSPI->FCR = QUADSPI_FCR_CTCF; // clear TC flag

    QUADSPI->DLR = len - 1; // number of bytes to read

    QUADSPI->CCR =
        0 << QUADSPI_CCR_DDRM_Pos // DDR mode disabled
        | 0 << QUADSPI_CCR_SIOO_Pos // send instruction every transaction
        | 1 << QUADSPI_CCR_FMODE_Pos // indirect read mode
        | 3 << QUADSPI_CCR_DMODE_Pos // data on 4 lines
        | 4 << QUADSPI_CCR_DCYC_Pos // 4 dummy cycles
        | 0 << QUADSPI_CCR_ABSIZE_Pos // 8-bit alternate byte
        | 3 << QUADSPI_CCR_ABMODE_Pos // alternate byte on 4 lines
        | (addr_bytes-1) << QUADSPI_CCR_ADSIZE_Pos
        | 3 << QUADSPI_CCR_ADMODE_Pos // address on 4 lines
        | 1 << QUADSPI_CCR_IMODE_Pos // instruction on 1 line
        | cmd << QUADSPI_CCR_INSTRUCTION_Pos // quad read opcode
        ;

    QUADSPI->ABR = 0; // alternate byte: disable continuous read mode
    QUADSPI->AR = addr; // addres to read from

    // Read in the data 4 bytes at a time if dest is aligned
    if (((uintptr_t)dest & 3) == 0) {
        while (len >= 4) {
            while (!(QUADSPI->SR & QUADSPI_SR_FTF)) {
            }
            *(uint32_t*)dest = QUADSPI->DR;
            dest += 4;
            len -= 4;
        }
    }

    // Read in remaining data 1 byte at a time
    while (len) {
        while (!((QUADSPI->SR >> QUADSPI_SR_FLEVEL_Pos) & 0x3f)) {
        }
        *dest++ = *(volatile uint8_t*)&QUADSPI->DR;
        --len;
    }

    QUADSPI->FCR = QUADSPI_FCR_CTCF; // clear TC flag
}

const mp_qspi_proto_t qspi_proto = {
    .ioctl = qspi_ioctl,
    .write_cmd_data = qspi_write_cmd_data,
    .write_cmd_addr_data = qspi_write_cmd_addr_data,
    .read_cmd = qspi_read_cmd,
    .read_cmd_qaddr_qdata = qspi_read_cmd_qaddr_qdata,
};

#endif // defined(MICROPY_HW_QSPIFLASH_SIZE_BITS_LOG2)
