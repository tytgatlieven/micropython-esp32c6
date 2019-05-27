/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Glenn Ruben Bakke
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

#include <stdio.h>
#include <stdint.h>
#include "py/obj.h"
#include "py/runtime.h"
#include "mpconfigboard.h"

#if MICROPY_PY_BLE

#include "led.h"
#include "ble_drv.h"

extern const mp_obj_type_t ble_peripheral_type;
extern const mp_obj_type_t ble_service_type;
extern const mp_obj_type_t ble_uuid_type;
extern const mp_obj_type_t ble_characteristic_type;
extern const mp_obj_type_t ble_delegate_type;
extern const mp_obj_type_t ble_constants_type;
extern const mp_obj_type_t ble_scanner_type;
extern const mp_obj_type_t ble_scan_entry_type;


/// \method enable()
/// Enable BLE softdevice.
mp_obj_t ble_obj_enable(void) {
    printf("SoftDevice enabled\n");
    uint32_t err_code = ble_drv_stack_enable();
    if (err_code < 0) {
        // TODO: raise exception.
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ble_obj_enable_obj, ble_obj_enable);

/// \method disable()
/// Disable BLE softdevice.
mp_obj_t ble_obj_disable(void) {
    ble_drv_stack_disable();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ble_obj_disable_obj, ble_obj_disable);

/// \method enabled()
/// Get state of whether the softdevice is enabled or not.
mp_obj_t ble_obj_enabled(void) {
    uint8_t is_enabled = ble_drv_stack_enabled();
    mp_int_t enabled = is_enabled;
    return MP_OBJ_NEW_SMALL_INT(enabled);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ble_obj_enabled_obj, ble_obj_enabled);

/// \method address()
/// Return device address as text string.
mp_obj_t ble_obj_address(void) {
    ble_drv_addr_t local_addr;
    ble_drv_address_get(&local_addr);

    vstr_t vstr;
    vstr_init(&vstr, 17);

    vstr_printf(&vstr, ""HEX2_FMT":"HEX2_FMT":"HEX2_FMT":" \
                         HEX2_FMT":"HEX2_FMT":"HEX2_FMT"",
                local_addr.addr[5], local_addr.addr[4], local_addr.addr[3],
                local_addr.addr[2], local_addr.addr[1], local_addr.addr[0]);

    mp_obj_t mac_str = mp_obj_new_str(vstr.buf, vstr.len);

    vstr_clear(&vstr);

    return mac_str;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ble_obj_address_obj, ble_obj_address);

STATIC const mp_rom_map_elem_t ble_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ble) },
    { MP_ROM_QSTR(MP_QSTR_enable),   MP_ROM_PTR(&ble_obj_enable_obj) },
    { MP_ROM_QSTR(MP_QSTR_disable),  MP_ROM_PTR(&ble_obj_disable_obj) },
    { MP_ROM_QSTR(MP_QSTR_enabled),  MP_ROM_PTR(&ble_obj_enabled_obj) },
    { MP_ROM_QSTR(MP_QSTR_address),  MP_ROM_PTR(&ble_obj_address_obj) },
#if MICROPY_PY_BLE_PERIPHERAL
    { MP_ROM_QSTR(MP_QSTR_Peripheral),      MP_ROM_PTR(&ble_peripheral_type) },
#endif
#if 0 // MICROPY_PY_BLE_CENTRAL
    { MP_ROM_QSTR(MP_QSTR_Central),         MP_ROM_PTR(&ble_central_type) },
#endif
#if MICROPY_PY_BLE_CENTRAL
    { MP_ROM_QSTR(MP_QSTR_Scanner),         MP_ROM_PTR(&ble_scanner_type) },
    { MP_ROM_QSTR(MP_QSTR_ScanEntry),       MP_ROM_PTR(&ble_scan_entry_type) },
#endif
    { MP_ROM_QSTR(MP_QSTR_DefaultDelegate), MP_ROM_PTR(&ble_delegate_type) },
    { MP_ROM_QSTR(MP_QSTR_UUID),            MP_ROM_PTR(&ble_uuid_type) },
    { MP_ROM_QSTR(MP_QSTR_Service),         MP_ROM_PTR(&ble_service_type) },
    { MP_ROM_QSTR(MP_QSTR_Characteristic),  MP_ROM_PTR(&ble_characteristic_type) },
    { MP_ROM_QSTR(MP_QSTR_constants),       MP_ROM_PTR(&ble_constants_type) },
#if MICROPY_PY_BLE_DESCRIPTOR
    { MP_ROM_QSTR(MP_QSTR_Descriptor),      MP_ROM_PTR(&ble_descriptor_type) },
#endif
};

STATIC MP_DEFINE_CONST_DICT(ble_module_globals, ble_module_globals_table);

const mp_obj_module_t ble_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&ble_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_ble, ble_module, MICROPY_PY_BLE);


#endif // MICROPY_PY_BLE
