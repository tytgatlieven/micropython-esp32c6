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

#include <assert.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "systick.h"
#include "pendsv.h"

#if MICROPY_PY_NIMBLE

#include "nimble/nimble_port.h"
#include "transport/uart/ble_hci_uart.h"
#include "nimble/host/src/ble_hs_hci_priv.h" // for ble_hs_hci_cmd_tx
#include "host/ble_hs.h"

/******************************************************************************/
// Misc functions needed by Nimble

#include <stdarg.h>

int sprintf(char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(str, 65535, fmt, ap);
    va_end(ap);
    return ret;
}

// TODO deal with root pointers

#if 1
#undef malloc
#undef realloc
#undef free
void *malloc(size_t size) {
    printf("NIMBLE malloc(%u)\n", (uint)size);
    return m_malloc(size);
}
void free(void *ptr) {
    printf("NIMBLE free(%p)\n", ptr);
    return m_free(ptr);
}
void *realloc(void *ptr, size_t size) {
    printf("NIMBLE realloc(%p, %u)\n", ptr, (uint)size);
    return m_realloc(ptr, size);
}
#endif

/******************************************************************************/
// RUN LOOP

static bool run_loop_up = false;

extern void nimble_uart_process(void);
extern void os_eventq_run_all(void);
extern void os_callout_process(void);

STATIC void nimble_poll(void) {
    if (!run_loop_up) {
        return;
    }

    nimble_uart_process();
    os_callout_process();
    os_eventq_run_all();
}

// Poll nimble every 128ms
#define NIMBLE_TICK(tick) (((tick) & ~(SYSTICK_DISPATCH_NUM_SLOTS - 1) & 0x7f) == 0)

void nimble_poll_wrapper(uint32_t ticks_ms) {
    if (run_loop_up && NIMBLE_TICK(ticks_ms)) {
        pendsv_schedule_dispatch(PENDSV_DISPATCH_NIMBLE, nimble_poll);
    }
}

/******************************************************************************/
// BINDINGS

extern void ble_app_nus_init(void);
extern int ble_nus_read_char(void);
extern void ble_nus_write(size_t len, const uint8_t *buf);

STATIC mp_obj_t nimble_init(void) {
    ble_app_nus_init();

    ble_hci_uart_init();

    printf("nimble_port_init\n");
    nimble_port_init();
    ble_hs_sched_start();
    printf("nimble_port_init: done\n");

    run_loop_up = true;

    systick_enable_dispatch(SYSTICK_DISPATCH_NIMBLE, nimble_poll_wrapper);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(nimble_init_obj, nimble_init);

STATIC mp_obj_t nimble_deinit(void) {
    run_loop_up = false;
    mp_hal_pin_low(pyb_pin_BT_REG_ON);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(nimble_deinit_obj, nimble_deinit);

// hci_cmd(ogf, ocf, param[, outbuf])
STATIC mp_obj_t nimble_hci_cmd(size_t n_args, const mp_obj_t *args) {
    int ogf = mp_obj_get_int(args[0]);
    int ocf = mp_obj_get_int(args[1]);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[2], &bufinfo, MP_BUFFER_READ);

    uint8_t evt_buf[255];
    uint8_t evt_len;
    int rc = ble_hs_hci_cmd_tx(BLE_HCI_OP(ogf, ocf), bufinfo.buf, bufinfo.len, evt_buf, sizeof(evt_buf), &evt_len);

    if (rc != 0) {
        mp_raise_OSError(-rc);
    }

    if (n_args == 3) {
        return mp_obj_new_bytes(evt_buf, evt_len);
    } else {
        mp_get_buffer_raise(args[3], &bufinfo, MP_BUFFER_WRITE);
        if (bufinfo.len < evt_len) {
            mp_raise_ValueError("buf too small");
        }
        memcpy(bufinfo.buf, evt_buf, evt_len);
        return MP_OBJ_NEW_SMALL_INT(evt_len);
    }
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(nimble_hci_cmd_obj, 3, 4, nimble_hci_cmd);

STATIC mp_obj_t nimble_nus_read(void) {
    uint8_t buf[16];
    size_t i;
    for (i = 0; i < sizeof(buf); ++i) {
        int c = ble_nus_read_char();
        if (c < 0) {
            break;
        }
        buf[i] = c;
    }
    return mp_obj_new_bytes(buf, i);
}
MP_DEFINE_CONST_FUN_OBJ_0(nimble_nus_read_obj, nimble_nus_read);

STATIC mp_obj_t nimble_nus_write(mp_obj_t buf) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf, &bufinfo, MP_BUFFER_READ);
    ble_nus_write(bufinfo.len, bufinfo.buf);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(nimble_nus_write_obj, nimble_nus_write);

STATIC const mp_rom_map_elem_t nimble_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_nimble) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&nimble_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&nimble_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_hci_cmd), MP_ROM_PTR(&nimble_hci_cmd_obj) },
    { MP_ROM_QSTR(MP_QSTR_nus_read), MP_ROM_PTR(&nimble_nus_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_nus_write), MP_ROM_PTR(&nimble_nus_write_obj) },
};
STATIC MP_DEFINE_CONST_DICT(nimble_module_globals, nimble_module_globals_table);

const mp_obj_module_t nimble_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&nimble_module_globals,
};

#endif // MICROPY_PY_NIMBLE
