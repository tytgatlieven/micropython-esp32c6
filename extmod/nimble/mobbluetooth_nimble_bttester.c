/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Damien P. George
 * Copyright (c) 2021 Andrew Leech
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
#include "py/mperrno.h"
#include "py/mphal.h"

#if MICROPY_BLUETOOTH_NIMBLE_BTTESTER

#include "host/ble_uuid.h"
#include "host/ble_hs.h"

#include "bttester.h"

STATIC mp_obj_t mod_nimble_bttester_run(mp_obj_t obj, mp_obj_t stream) {
    tester_init();

    mp_stream_p_t *ctrl_stream = mp_get_stream_raise(stream, MP_STREAM_OP_WRITE | MP_STREAM_OP_READ);


}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_nimble_bttester_run_obj, mod_nimble_bttester_run);

STATIC const mp_rom_map_elem_t mp_module_nimble_bttester_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_nimble_bttester) },
    { MP_ROM_QSTR(MP_QSTR_run), MP_ROM_PTR(&mod_nimble_bttester_run_obj) },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_nimble_bttester_globals, mp_module_nimble_bttester_globals_table);

const mp_obj_module_t mp_module_nimble_bttester = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_nimble_bttester_globals,
};

#endif