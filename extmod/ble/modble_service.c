/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Glenn Ruben Bakke
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

#include "py/obj.h"
#include "py/runtime.h"
#include "py/objlist.h"
#include "ble_drv.h"

#if MICROPY_PY_BLE_PERIPHERAL || MICROPY_PY_BLE_CENTRAL

#include "modble.h"

STATIC void ble_service_print(const mp_print_t *print, mp_obj_t o, mp_print_kind_t kind) {
    ble_service_obj_t * self = (ble_service_obj_t *)o;

    mp_printf(print, "Service(handle: 0x" HEX2_FMT ")", self->handle);
}

STATIC mp_obj_t ble_service_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {

    enum { ARG_NEW_UUID, ARG_NEW_TYPE };

    static const mp_arg_t allowed_args[] = {
        { ARG_NEW_UUID, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { ARG_NEW_TYPE, MP_ARG_INT, {.u_int = BLE_SERVICE_PRIMARY} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    ble_service_obj_t *s = m_new_obj(ble_service_obj_t);
    s->base.type = type;

    mp_obj_t uuid_obj = args[ARG_NEW_UUID].u_obj;

    if (uuid_obj == MP_OBJ_NULL) {
        return MP_OBJ_FROM_PTR(s);
    }

    if (mp_obj_is_type(uuid_obj, &ble_uuid_type)) {
        s->p_uuid = MP_OBJ_TO_PTR(uuid_obj);

        uint8_t type = args[ARG_NEW_TYPE].u_int;
        if (type > 0 &&  type <= BLE_SERVICE_PRIMARY) {
            s->type = type;
        } else {
            mp_raise_ValueError("Invalid Service type");
        }

        (void)ble_drv_service_add(s);

    } else {
        mp_raise_ValueError("Invalid UUID parameter");
    }

    // clear reference to peripheral
    s->p_periph = NULL;
    s->char_list = mp_obj_new_list(0, NULL);

    return MP_OBJ_FROM_PTR(s);
}

/// \method __setattr__(name, val)
/// Add attribute to class instance. 
/// If it's a Characteristic, track it explicitely
///
STATIC mp_obj_t mp_builtin_setattr(mp_obj_t base, mp_obj_t attr, mp_obj_t value) {
    mp_store_attr(base, mp_obj_str_get_qstr(attr), value);
    
    if (mp_obj_is_subclass(MP_OBJ_FROM_PTR(mp_obj_get_type(value)), MP_OBJ_FROM_PTR(&ble_characteristic_type))) {
        ble_service_obj_t *self = MP_OBJ_TO_PTR(base);
        mp_obj_list_append(self->char_list, value);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_3(ble_service_setattr_obj, service_setattr);


/// \method addCharacteristic(Characteristic)
/// Add Characteristic to the Service.
///
STATIC mp_obj_t service_add_characteristic(mp_obj_t self_in, mp_obj_t characteristic) {
    ble_service_obj_t        * self   = MP_OBJ_TO_PTR(self_in);
    ble_characteristic_obj_t * p_char = MP_OBJ_TO_PTR(characteristic);

    p_char->service_handle = self->handle;

    bool retval = ble_drv_characteristic_add(p_char);

    if (retval) {
        p_char->p_service = self;
    }

    mp_obj_list_append(self->char_list, characteristic);

    // return mp_obj_new_bool(retval);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(ble_service_add_char_obj, service_add_characteristic);

/// \method getCharacteristics()
/// Return list with all characteristics registered in the Service.
///
STATIC mp_obj_t service_get_chars(mp_obj_t self_in) {
    ble_service_obj_t * self = MP_OBJ_TO_PTR(self_in);

    return self->char_list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ble_service_get_chars_obj, service_get_chars);

/// \method getCharacteristic(UUID)
/// Return Characteristic with the given UUID.
///
STATIC mp_obj_t service_get_characteristic(mp_obj_t self_in, mp_obj_t uuid) {
    ble_service_obj_t * self   = MP_OBJ_TO_PTR(self_in);
    ble_uuid_obj_t    * p_uuid = MP_OBJ_TO_PTR(uuid);

    // validate that there is an UUID object passed in as parameter
    if (!(mp_obj_is_type(uuid, &ble_uuid_type))) {
        mp_raise_ValueError("Invalid UUID parameter");
    }

    mp_obj_t * chars     = NULL;
    mp_uint_t  num_chars = 0;
    mp_obj_get_array(self->char_list, &num_chars, &chars);

    for (uint8_t i = 0; i < num_chars; i++) {
        ble_characteristic_obj_t * p_char = (ble_characteristic_obj_t *)chars[i];

        bool type_match = p_char->p_uuid->type == p_uuid->type;
        bool uuid_match = ((uint16_t)(*(uint16_t *)&p_char->p_uuid->value[0]) ==
                           (uint16_t)(*(uint16_t *)&p_uuid->value[0]));

        if (type_match && uuid_match) {
            return MP_OBJ_FROM_PTR(p_char);
        }
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(ble_service_get_char_obj, service_get_characteristic);

/// \method uuid()
/// Get UUID instance of the Service.
///
STATIC mp_obj_t service_uuid(mp_obj_t self_in) {
    ble_service_obj_t * self = MP_OBJ_TO_PTR(self_in);
    return MP_OBJ_FROM_PTR(self->p_uuid);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ble_service_get_uuid_obj, service_uuid);

STATIC const mp_rom_map_elem_t ble_service_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___setattr__),        MP_ROM_PTR(&ble_service_setattr_obj) },
    { MP_ROM_QSTR(MP_QSTR_getCharacteristic),  MP_ROM_PTR(&ble_service_get_char_obj) },
    { MP_ROM_QSTR(MP_QSTR_addCharacteristic),  MP_ROM_PTR(&ble_service_add_char_obj) },
    { MP_ROM_QSTR(MP_QSTR_getCharacteristics), MP_ROM_PTR(&ble_service_get_chars_obj) },
#if 0
	// Properties
    { MP_ROM_QSTR(MP_QSTR_peripheral), MP_ROM_PTR(&ble_service_get_peripheral_obj) },
#endif
    { MP_ROM_QSTR(MP_QSTR_uuid),       MP_ROM_PTR(&ble_service_get_uuid_obj) },
    { MP_ROM_QSTR(MP_QSTR_PRIMARY),    MP_ROM_INT(BLE_SERVICE_PRIMARY) },
    { MP_ROM_QSTR(MP_QSTR_SECONDARY),  MP_ROM_INT(BLE_SERVICE_SECONDARY) },
};

STATIC MP_DEFINE_CONST_DICT(ble_service_locals_dict, ble_service_locals_dict_table);

const mp_obj_type_t ble_service_type = {
    { &mp_type_type },
    .name = MP_QSTR_Service,
    .print = ble_service_print,
    .make_new = ble_service_make_new,
    .locals_dict = (mp_obj_dict_t*)&ble_service_locals_dict
};

#endif // MICROPY_PY_BLE_PERIPHERAL || MICROPY_PY_BLE_CENTRAL
