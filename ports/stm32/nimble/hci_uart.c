/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Damien P. George
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

#include "py/runtime.h"
#include "py/mphal.h"
#include "pin_static_af.h"
#include "uart.h"
#include "pendsv.h"
#include "nimble/ble.h"
#include "hal/hal_uart.h"
#include "cywbt.h"

#if MICROPY_BLUETOOTH_NIMBLE


/******************************************************************************/
// UART
pyb_uart_obj_t bt_hci_uart_obj;
static uint8_t hci_uart_rxbuf[512];

extern void nimble_poll(void);

mp_obj_t mp_uart_interrupt(mp_obj_t self_in) {
    pendsv_schedule_dispatch(PENDSV_DISPATCH_NIMBLE, nimble_poll);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_uart_interrupt_obj, mp_uart_interrupt);

int uart_init_baudrate(uint32_t baudrate) {
    uart_init(&bt_hci_uart_obj, baudrate, UART_WORDLENGTH_8B, UART_PARITY_NONE, UART_STOPBITS_1, UART_HWCONTROL_RTS | UART_HWCONTROL_CTS);
    uart_set_rxbuf(&bt_hci_uart_obj, sizeof(hci_uart_rxbuf), hci_uart_rxbuf);
    return 0;
}

static int uart_init_0(int uart_id, int baud) {
    bt_hci_uart_obj.base.type = &pyb_uart_type;
    bt_hci_uart_obj.uart_id = uart_id;
    bt_hci_uart_obj.is_static = true;
    bt_hci_uart_obj.timeout = 2;
    bt_hci_uart_obj.timeout_char = 2;
    MP_STATE_PORT(pyb_uart_obj_all)[bt_hci_uart_obj.uart_id - 1] = &bt_hci_uart_obj;
    uart_init_baudrate(baud);

    // Interrupt on RX chunk received (idle)
    // Trigger nimble poll when this happens
    mp_obj_t uart_irq_fn = mp_load_attr(&bt_hci_uart_obj, MP_QSTR_irq);
    mp_obj_t uargs[] = {
        MP_OBJ_FROM_PTR(&mp_uart_interrupt_obj),
        MP_OBJ_NEW_SMALL_INT(UART_FLAG_IDLE),
        mp_const_true,
    };
    mp_call_function_n_kw(uart_irq_fn, 3, 0, uargs);
    return 0;
}

/******************************************************************************/
// Bindings Uart to Nimble
uint8_t bt_hci_cmd_buf[4 + 256];

static hal_uart_tx_cb_t hal_uart_tx_cb;
static void *hal_uart_tx_arg;
static hal_uart_rx_cb_t hal_uart_rx_cb;
static void *hal_uart_rx_arg;

static uint32_t bt_sleep_ticks;

int hal_uart_init_cbs(uint32_t port, hal_uart_tx_cb_t tx_cb, void *tx_arg, hal_uart_rx_cb_t rx_cb, void *rx_arg) {
    hal_uart_tx_cb = tx_cb;
    hal_uart_tx_arg = tx_arg;
    hal_uart_rx_cb = rx_cb;
    hal_uart_rx_arg = rx_arg;
    return 0; // success
}

int hal_uart_config(uint32_t port, uint32_t baud, uint32_t bits, uint32_t stop, uint32_t parity, uint32_t flow) {
    uart_init_0(port, baud);

    #if MICROPY_PY_NETWORK_CYW43
        cywbt_init();
        cywbt_activate();
    #endif

    return 0; // success
}

void hal_uart_start_tx(uint32_t port) {
    size_t len = 0;
    for (;;) {
        int data = hal_uart_tx_cb(hal_uart_tx_arg);
        if (data == -1) {
            break;
        }
        bt_hci_cmd_buf[len++] = data;
    }

    #if 0
    printf("[% 8d] BTUTX: %02x", mp_hal_ticks_ms(), hci_cmd_buf[0]);
    for (int i = 1; i < len; ++i) {
        printf(":%02x", hci_cmd_buf[i]);
    }
    printf("\n");
    #endif

    bt_sleep_ticks = mp_hal_ticks_ms();

    #ifdef pyb_pin_BT_DEV_WAKE
        if (mp_hal_pin_read(pyb_pin_BT_DEV_WAKE) == 1) {
            //printf("BT WAKE for TX\n");
            mp_hal_pin_low(pyb_pin_BT_DEV_WAKE); // wake up
            mp_hal_delay_ms(5); // can't go lower than this
        }
    #endif

    uart_tx_strn(&bt_hci_uart_obj, (void*)bt_hci_cmd_buf, len);
}

int hal_uart_close(uint32_t port) {
    return 0; // success
}

void nimble_uart_process(void) {
    int host_wake = 0;
    #ifdef pyb_pin_BT_HOST_WAKE
        host_wake = mp_hal_pin_read(pyb_pin_BT_HOST_WAKE);
    #endif
    /*
    // this is just for info/tracing purposes
    static int last_host_wake = 0;
    if (host_wake != last_host_wake) {
        printf("HOST_WAKE change %d -> %d\n", last_host_wake, host_wake);
        last_host_wake = host_wake;
    }
    */
    while (uart_rx_any(&bt_hci_uart_obj)) {
        uint8_t data = uart_rx_char(&bt_hci_uart_obj);
        //printf("UART RX: %02x\n", data);
        hal_uart_rx_cb(hal_uart_rx_arg, data);
    }
    if (host_wake == 1 && mp_hal_pin_read(pyb_pin_BT_DEV_WAKE) == 0) {
        if (mp_hal_ticks_ms() - bt_sleep_ticks > 500) {
            //printf("BT SLEEP\n");
            mp_hal_pin_high(pyb_pin_BT_DEV_WAKE); // let sleep
        }
    }
}

#endif // MICROPY_BLUETOOTH_NIMBLE
