/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
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

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "py/runtime.h"

#if MICROPY_STREAM_BLOCKDEV

#include "py/mperrno.h"
#include "py/stream.h"
#include "extmod/stream_blockdev.h"
#include "extmod/vfs.h"

/******************************************************************************/
// MicroPython bindings
//
// Expose the stream_bdev as an object with the block protocol.

STATIC mp_obj_t stream_flush(mp_obj_t stream) {
    const mp_stream_p_t *stream_p = mp_get_stream(stream);
    int error;
    mp_uint_t res = stream_p->ioctl(stream, MP_STREAM_FLUSH, 0, &error);
    if (res == MP_STREAM_ERROR) {
        mp_raise_OSError(error);
    }
    return mp_const_none;
}

STATIC mp_uint_t stream_seek(mp_obj_t stream, int whence, int offset) {
    const mp_stream_p_t *stream_p = mp_get_stream(stream);
    struct mp_stream_seek_t seek_s;
    seek_s.offset = offset;
    seek_s.whence = whence;
    int error;
    mp_uint_t res = stream_p->ioctl(stream, MP_STREAM_SEEK, (mp_uint_t)(uintptr_t)&seek_s, &error);
    if (res == 0) {
        return (mp_uint_t)seek_s.offset;
    }
    mp_raise_OSError(error);
}

STATIC void mpy_stream_bdev_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mpy_stream_bdev_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "StreamBlockDevice(stream=<>, start=%u, len=%u)", /*(uint32_t)&self->stream,*/ self->start, self->len);
}

STATIC mp_obj_t mpy_stream_bdev_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    // Parse arguments
    enum { ARG_stream, ARG_block_size, ARG_start, ARG_len };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_stream, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_block_size, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 512} },
        { MP_QSTR_start, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_len,   MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // if (args[ARG_start].u_int == -1 && args[ARG_len].u_int == -1) {
    //     // Default singleton object that accesses entire stream_bdev, including virtual partition table
    //     return MP_OBJ_FROM_PTR(&mpy_stream_bdev_obj);
    // }

    // Make sure we got a stream object
    mp_obj_t stream = args[ARG_stream].u_obj;
    // const mp_stream_p_t *stream_p = mp_get_stream_raise(stream, MP_STREAM_OP_IOCTL);

    mpy_stream_bdev_obj_t *self = m_new_obj(mpy_stream_bdev_obj_t);
    self->base.type = &mpy_stream_bdev_type;
    self->stream = stream;

    size_t bl_len = args[ARG_len].u_int;
    if (bl_len == 0) {
        bl_len = (uint32_t)stream_seek(stream, MP_SEEK_END, 0);
    }

    self->block_size = args[ARG_block_size].u_int;

    mp_int_t start = args[ARG_start].u_int;
    if (start == -1) {
        start = 0;
    } else if (!(0 <= start && (size_t)start < bl_len && start % self->block_size == 0)) {
        mp_raise_ValueError(NULL);
    }

    self->start = start;
    self->len = bl_len - start;

    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t mpy_stream_bdev_readblocks(size_t n_args, const mp_obj_t *args) {
    mpy_stream_bdev_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint32_t block_num = mp_obj_get_int(args[1]);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[2], &bufinfo, MP_BUFFER_WRITE);
    
    uint32_t offset = 0;
    if (n_args == 4) {
        offset = mp_obj_get_int(args[3]);
    }
    uint32_t start = self->start + (block_num * self->block_size) + offset;
    stream_seek(self->stream, SEEK_SET, start);

    // mp_obj_array_t ar = {{&mp_type_bytearray}, BYTEARRAY_TYPECODE, 0, num_blocks *self->block_size, buf};
    // self->readblocks[2] = MP_OBJ_NEW_SMALL_INT(block_num);
    // self->readblocks[3] = MP_OBJ_FROM_PTR(&ar);
    
    // mp_call_method_n_kw(2, 0, self->readblocks);

    mp_obj_t read_args[3] = {
        MP_OBJ_FROM_PTR(&mp_stream_readinto_obj),
        self->stream,
        args[2]
    };
    mp_call_method_n_kw(1, 0, read_args);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mpy_stream_bdev_readblocks_obj, 3, 4, mpy_stream_bdev_readblocks);

STATIC mp_obj_t mpy_stream_bdev_writeblocks(size_t n_args, const mp_obj_t *args) {
    mpy_stream_bdev_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    uint32_t block_num = mp_obj_get_int(args[1]);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[2], &bufinfo, MP_BUFFER_READ);

    uint32_t offset = 0;
    if (n_args == 4) {
        offset = mp_obj_get_int(args[3]);
    }
    uint32_t start = self->start + (block_num * self->block_size) + offset;
    stream_seek(self->stream, MP_SEEK_SET, start);

    mp_obj_t write_args[3] = {
        MP_OBJ_FROM_PTR(&mp_stream_write_obj),
        self->stream,
        args[2]
    };
    mp_call_method_n_kw(1, 0, write_args);

    return mp_const_none;


    // if (n_args == 3) {
    //     // Cast self->start to signed in case it's mpy_stream_bdev_obj with negative start
    //     block_num += FLASH_PART1_START_BLOCK + (int32_t)self->start / self->block_size;
    //     ret = storage_write_blocks(bufinfo.buf, block_num, bufinfo.len / self->block_size);
    // }
    // else {
    //     // Extended block write on a sub-section of the stream_bdev storage
    //     uint32_t offset = mp_obj_get_int(args[3]);
    //     if ((block_num * self->block_size) >= self->len) {
    //         ret = -MP_EFAULT; // Bad address
    //     } else {
    //         block_num += self->start / self->block_size;
    //         ret = MICROPY_HW_BDEV_WRITEBLOCKS_EXT(bufinfo.buf, block_num, offset, bufinfo.len);
    //     }
    // }
    // return MP_OBJ_NEW_SMALL_INT(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mpy_stream_bdev_writeblocks_obj, 3, 4, mpy_stream_bdev_writeblocks);

STATIC mp_obj_t mpy_stream_bdev_ioctl(mp_obj_t self_in, mp_obj_t cmd_in, mp_obj_t arg_in) {
    mpy_stream_bdev_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_int_t cmd = mp_obj_get_int(cmd_in);
    switch (cmd) {
        case MP_BLOCKDEV_IOCTL_INIT: {
            mp_int_t ret = 0;
            // storage_init();
            // if (mp_obj_get_int(arg_in) == 1) {
            //     // Will be using extended block protocol
            //     // Switch to use native block size of the underlying storage.
            //         // self->use_native_block_size = true;
            //         //TODO
            //     }
            // }
            return MP_OBJ_NEW_SMALL_INT(ret);
        }
        case MP_BLOCKDEV_IOCTL_DEINIT:
            stream_flush(self->stream);
            return MP_OBJ_NEW_SMALL_INT(0);                                             // TODO properly
        case MP_BLOCKDEV_IOCTL_SYNC:
            stream_flush(self->stream);
            return MP_OBJ_NEW_SMALL_INT(0);

        case MP_BLOCKDEV_IOCTL_BLOCK_COUNT: {
            size_t num = self->len / self->block_size;
            if (self->len % self->block_size) {
                num += 1;
            }
            return MP_OBJ_NEW_SMALL_INT(num);
        }

        case MP_BLOCKDEV_IOCTL_BLOCK_SIZE: {
            return MP_OBJ_NEW_SMALL_INT(self->block_size);
        }

        case MP_BLOCKDEV_IOCTL_BLOCK_ERASE: {
            int ret = 0;
            // #if defined(MICROPY_HW_BDEV_ERASEBLOCKS_EXT)
            // if (self->use_native_block_size) {
            //     mp_int_t block_num = self->start / self->block_size + mp_obj_get_int(arg_in);

            //     ret = MICROPY_HW_BDEV_ERASEBLOCKS_EXT(block_num, self->block_size);
            // }
            // #endif
            return MP_OBJ_NEW_SMALL_INT(ret);
        }

        case MP_BLOCKDEV_IOCTL_MEMMAP: {
            // create mempap instance of stream and return as bytearray
            // const mp_stream_p_t *stream_p = mp_get_stream(self->stream);
            // int error;
            // mp_uint_t res = stream_p->ioctl(self->stream, MP_STREAM_FLUSH, 0, &error);
            // if (res == MP_STREAM_ERROR) {
            //     mp_raise_OSError(error);
            // }
                        
        }

        default:
            return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mpy_stream_bdev_ioctl_obj, mpy_stream_bdev_ioctl);


STATIC const mp_rom_map_elem_t mpy_stream_bdev_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_readblocks), MP_ROM_PTR(&mpy_stream_bdev_readblocks_obj) },
    { MP_ROM_QSTR(MP_QSTR_writeblocks), MP_ROM_PTR(&mpy_stream_bdev_writeblocks_obj) },
    { MP_ROM_QSTR(MP_QSTR_ioctl), MP_ROM_PTR(&mpy_stream_bdev_ioctl_obj) },
};

STATIC MP_DEFINE_CONST_DICT(mpy_stream_bdev_locals_dict, mpy_stream_bdev_locals_dict_table);


const mp_obj_type_t mpy_stream_bdev_type = {
    { &mp_type_type },
    .name = MP_QSTR_StreamBlockDevice,
    .print = mpy_stream_bdev_print,
    .make_new = mpy_stream_bdev_make_new,
    .locals_dict = (mp_obj_dict_t *)&mpy_stream_bdev_locals_dict,
};

#endif