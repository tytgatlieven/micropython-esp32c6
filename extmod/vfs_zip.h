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
#ifndef MICROPY_INCLUDED_EXTMOD_VFS_ZIP_H
#define MICROPY_INCLUDED_EXTMOD_VFS_ZIP_H

#include "py/mpconfig.h"
#include "py/obj.h"
#include "extmod/vfs.h"

typedef struct _fs_zip_user_mount_t {
    mp_obj_base_t base;
    mp_vfs_blockdev_t blockdev;
    uint32_t cd_block; // address of central directory
    uint32_t cd_offset;
    uint32_t cd_size;
    char cwd[1024/*MICROPY_ALLOC_PATH_MAX*/ + 1];
} fs_zip_user_mount_t;

extern const byte mzip_e_to_errno_table[9];
extern const mp_obj_type_t mp_type_vfs_zip;
extern const mp_obj_type_t mp_type_vfs_zip_fileio;
extern const mp_obj_type_t mp_type_vfs_zip_textio;

MP_DECLARE_CONST_FUN_OBJ_3(zip_vfs_open_obj);

#endif // MICROPY_INCLUDED_EXTMOD_VFS_ZIP_H
