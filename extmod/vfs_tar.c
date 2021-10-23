/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Damien P. George
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

#include "py/mpconfig.h"
#if MICROPY_VFS_TAR

#if !MICROPY_VFS
#error "with MICROPY_VFS_TAR enabled, must also enable MICROPY_VFS"
#endif

#include <string.h>
#include "py/runtime.h"
#include "py/mperrno.h"
#include "extmod/vfs_tar.h"
#include "shared/timeutils/timeutils.h"

#define mp_obj_tar_vfs_t fs_tar_user_mount_t

STATIC mp_import_stat_t tar_vfs_import_stat(void *vfs_in, const char *path) {
    fs_tar_user_mount_t *vfs = vfs_in;
    mtar_header_t header;
    if (path[0] == '/') {
        path += 1;
    }
    assert(vfs != NULL);
    int res = mtar_find(&vfs->tar, path, &header);
    if (res == MTAR_ESUCCESS) {
        if (header.type == MTAR_TDIR) {
            return MP_IMPORT_STAT_DIR;
        } else {
            return MP_IMPORT_STAT_FILE;
        }
    }
    return MP_IMPORT_STAT_NO_EXIST;
}

static void raise_error_read_only() {
    mp_raise_NotImplementedError(MP_ERROR_TEXT("vfs_tar is read only"));
}

static void _update_position(fs_tar_user_mount_t *vfs, size_t size) {
    vfs->block += size / vfs->blockdev.block_size;
    vfs->offset += size % vfs->blockdev.block_size;
    if (vfs->offset >= vfs->blockdev.block_size) {
        vfs->offset -= vfs->blockdev.block_size;
        vfs->block += 1;
    }
}

static int block_write(mtar_t *tar, const void *data, unsigned size) {
  fs_tar_user_mount_t *vfs = (fs_tar_user_mount_t *)tar->stream;
  int res = mp_vfs_blockdev_write_ext(&vfs->blockdev, vfs->block, vfs->offset, size, data);
  if (res == 0) {
    _update_position(vfs, size);
    return MTAR_ESUCCESS;
  }
  return MTAR_EWRITEFAIL;
}

static int block_read(mtar_t *tar, void *data, unsigned size) {
  fs_tar_user_mount_t *vfs = (fs_tar_user_mount_t *)tar->stream;
  int res = mp_vfs_blockdev_read_ext(&vfs->blockdev, vfs->block, vfs->offset, size, data);
  if (res == 0) {
    _update_position(vfs, size);
    return MTAR_ESUCCESS;
  }
  return MTAR_EREADFAIL;
}

static int block_seek(mtar_t *tar, unsigned offset) {
  fs_tar_user_mount_t *vfs = (fs_tar_user_mount_t *)tar->stream;
  vfs->block = 0;
  vfs->offset = 0;
  _update_position(vfs, offset);
  return MTAR_ESUCCESS;
}

static int block_close(mtar_t *tar) {
    (void)tar;
//   fs_tar_user_mount_t *mount = (fs_tar_user_mount_t *)tar->stream;
//   fclose(tar->stream);
  return MTAR_ESUCCESS;
}

STATIC mp_obj_t tar_vfs_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    // create new object
    fs_tar_user_mount_t *vfs = m_new_obj(fs_tar_user_mount_t);
    memset(vfs, 0, sizeof(fs_tar_user_mount_t));

    vfs->base.type = type;
    vfs->tar.stream = (void *)vfs;

    // Initialise underlying block device
    vfs->blockdev.flags = MP_BLOCKDEV_FLAG_FREE_OBJ;
    // vfs->blockdev.block_size = FF_MIN_SS; // default, will be populated by call to MP_BLOCKDEV_IOCTL_BLOCK_SIZE
    mp_vfs_blockdev_init(&vfs->blockdev, args[0]);
    mp_obj_t bsize = mp_vfs_blockdev_ioctl(&vfs->blockdev, MP_BLOCKDEV_IOCTL_BLOCK_SIZE, 0);
    if (mp_obj_is_small_int(bsize)) {
        vfs->blockdev.block_size = MP_OBJ_SMALL_INT_VALUE(bsize);
    } else {
        vfs->blockdev.block_size = 512; 
    }

    // mount the block device so the VFS methods can be used
    // FRESULT res = f_mount(&vfs->tar);

    /* Init tar struct and functions */
    vfs->tar.write = block_write;
    vfs->tar.read = block_read;
    vfs->tar.seek = block_seek;
    vfs->tar.close = block_close;

    /* Read first header to check it is valid if mode is `r` */
    mtar_header_t h;
    int res = mtar_read_header(&vfs->tar, &h);
    if (res == MTAR_EOPENFAIL) {
        // don't error out if no filesystem, to let mkfs()/mount() create one if wanted
        vfs->blockdev.flags |= MP_BLOCKDEV_FLAG_NO_FILESYSTEM;
    } else if (res != MTAR_ESUCCESS) {
        mp_raise_OSError(mtar_e_to_errno_table[-res]);
    }
    return MP_OBJ_FROM_PTR(vfs);
}

#if _FS_REENTRANT
STATIC mp_obj_t tar_vfs_del(mp_obj_t self_in) {
    // mp_obj_tar_vfs_t *self = MP_OBJ_TO_PTR(self_in);
    // // f_umount only needs to be called to release the sync object
    // f_umount(&self->tar);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tar_vfs_del_obj, tar_vfs_del);
#endif

STATIC mp_obj_t tar_vfs_mkfs(mp_obj_t bdev_in) {
    (void)bdev_in;
    raise_error_read_only();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tar_vfs_mkfs_fun_obj, tar_vfs_mkfs);
STATIC MP_DEFINE_CONST_STATICMETHOD_OBJ(tar_vfs_mkfs_obj, MP_ROM_PTR(&tar_vfs_mkfs_fun_obj));

typedef struct _mp_vfs_tar_ilistdir_it_t {
    mp_obj_base_t base;
    mp_fun_1_t iternext;
    bool is_str;
    mtar_t tar;
    mtar_header_t dir;
    char path[MICROPY_ALLOC_PATH_MAX + 1];
} mp_vfs_tar_ilistdir_it_t;

STATIC mp_obj_t mp_vfs_tar_ilistdir_it_iternext(mp_obj_t self_in) {
    mp_vfs_tar_ilistdir_it_t *self = MP_OBJ_TO_PTR(self_in);

    for (;;) {
        // int res = mtar_next(&self->tar);
        // if (res != MTAR_ESUCCESS) {
        //     break;
        // }
        /* Load header */
        mtar_header_t h;
        int res = mtar_read_header(&self->tar, &h);
        if (res != MTAR_ESUCCESS) {
            break;
        }
        const char *next_path = h.name;
        size_t match_len = strlen(self->path);
        if (next_path[0] == 0 || strncmp(self->path, next_path, match_len) != 0) {
            // stop on error or end of dir
            break;
        }
        const char *name = &next_path[match_len];
        size_t name_length = strcspn(name, "/");
        if (name_length < strlen(name) - 1) {
            // file in subdirectory
            res = mtar_next(&self->tar);
            if (res != MTAR_ESUCCESS) {
                break;
            }
            continue;
        }

        // make 4-tuple with info about this entry
        mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(4, NULL));
        if (self->is_str) {
            t->items[0] = mp_obj_new_str(name, name_length);
        } else {
            t->items[0] = mp_obj_new_bytes((const byte *)name, name_length);
        }
        if (h.type == MTAR_TDIR) {
            // dir
            t->items[1] = MP_OBJ_NEW_SMALL_INT(MP_S_IFDIR);
        } else {
            // file
            t->items[1] = MP_OBJ_NEW_SMALL_INT(MP_S_IFREG);
        }
        t->items[2] = MP_OBJ_NEW_SMALL_INT(0); // no inode number
        t->items[3] = mp_obj_new_int_from_uint(h.size);

        res = mtar_next(&self->tar);
        if (res != MTAR_ESUCCESS) {
            break;
        }

        return MP_OBJ_FROM_PTR(t);
    }

    // ignore error because we may be closing a second time
    // f_closedir(&self->dir);

    return MP_OBJ_STOP_ITERATION;
}

STATIC mp_obj_t tar_vfs_ilistdir_func(size_t n_args, const mp_obj_t *args) {
    mp_obj_tar_vfs_t *self = MP_OBJ_TO_PTR(args[0]);
    bool is_str_type = true;
    const char *path;
    if (n_args == 2) {
        if (mp_obj_get_type(args[1]) == &mp_type_bytes) {
            is_str_type = false;
        }
        path = mp_obj_str_get_str(args[1]);
    } else {
        path = "";
    }
    if (path[0] == '/') {
        // tar file doesn't have leading slash in paths
        path += 1;
    }
    // Create a new iterator object to list the dir
    mp_vfs_tar_ilistdir_it_t *iter = m_new_obj(mp_vfs_tar_ilistdir_it_t);
    memset(iter, 0, sizeof(mp_vfs_tar_ilistdir_it_t));
    iter->base.type = &mp_type_polymorph_iter;
    iter->iternext = mp_vfs_tar_ilistdir_it_iternext;
    iter->is_str = is_str_type;

    if (path[0] == 0) {
        int res = mtar_rewind(&self->tar);
        if (res) {
            mp_raise_OSError(mtar_e_to_errno_table[-res]);
        }
    } else {
        int res = mtar_find(&self->tar, path, &iter->dir);
        if (res != MTAR_ESUCCESS) {
            mp_raise_OSError(mtar_e_to_errno_table[-res]);
        }
        res = mtar_read_header(&self->tar, &iter->dir);
        if (iter->dir.type != MTAR_TDIR) {
            mp_raise_OSError(MP_ENOTDIR);
        }
    }
    memcpy(&iter->tar, &self->tar, sizeof(iter->tar));

    size_t slen = strlen(path);
    if (slen != 0) {
        strncpy(iter->path, path, sizeof(iter->path)-2);
        if (iter->path[slen-1] != '/') {
            iter->path[slen] = '/';
            iter->path[slen+1] = 0;
        }
    }

    return MP_OBJ_FROM_PTR(iter);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tar_vfs_ilistdir_obj, 1, 2, tar_vfs_ilistdir_func);


STATIC mp_obj_t tar_vfs_remove(mp_obj_t vfs_in, mp_obj_t path_in) {
    (void)vfs_in;
    (void)path_in;
    raise_error_read_only();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tar_vfs_remove_obj, tar_vfs_remove);

STATIC mp_obj_t tar_vfs_rmdir(mp_obj_t vfs_in, mp_obj_t path_in) {
    (void)vfs_in;
    (void)path_in;
    raise_error_read_only();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tar_vfs_rmdir_obj, tar_vfs_rmdir);

STATIC mp_obj_t tar_vfs_rename(mp_obj_t vfs_in, mp_obj_t path_in, mp_obj_t path_out) {
    (void)vfs_in;
    (void)path_in;
    (void)path_out;
    raise_error_read_only();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(tar_vfs_rename_obj, tar_vfs_rename);

STATIC mp_obj_t tar_vfs_mkdir(mp_obj_t vfs_in, mp_obj_t path_o) {
    (void)vfs_in;
    (void)path_o;
    raise_error_read_only();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tar_vfs_mkdir_obj, tar_vfs_mkdir);

// Change current directory.
STATIC mp_obj_t tar_vfs_chdir(mp_obj_t vfs_in, mp_obj_t path_in) {
    mp_obj_tar_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    char buf[MICROPY_ALLOC_PATH_MAX + 1];
    mtar_header_t header;
    const char *fullpath;
    const char *path = mp_obj_str_get_str(path_in);

    if (path[0] == '/') {
        fullpath = &path[1];
    } else {
        int i;
        strncpy(buf, self->cwd, MICROPY_ALLOC_PATH_MAX);
        i = strlen(buf);
        if (i + strlen(path) + 2 > MICROPY_ALLOC_PATH_MAX) {
            mp_raise_OSError(MP_ENOMEM);
        }
        if (i && buf[i-1] != '/') {
            buf[i] = '/';
            i += 1;
        }
        if (path[0] == '.' && path[1] == '/') {
            path = path+2;
        }
        strncpy(&buf[i], path, MICROPY_ALLOC_PATH_MAX-i-1);
        fullpath = buf;
    }
    printf("vfs_tar: chdir(%s)\n", fullpath);
    
    int res = mtar_find(&self->tar, fullpath, &header);
    if (res == MTAR_ESUCCESS) {
        if (header.type != MTAR_TDIR) {
            mp_raise_OSError(MP_EACCES);
        }
    } else {
        mp_raise_OSError(MP_ENOENT);
    }
    strncpy(self->cwd, fullpath, MICROPY_ALLOC_PATH_MAX-1);
        
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tar_vfs_chdir_obj, tar_vfs_chdir);

// Get the current directory.
STATIC mp_obj_t tar_vfs_getcwd(mp_obj_t vfs_in) {
    mp_obj_tar_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    return mp_obj_new_str(self->cwd, strlen(self->cwd));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tar_vfs_getcwd_obj, tar_vfs_getcwd);

// Get the status of a file or directory.
STATIC mp_obj_t tar_vfs_stat(mp_obj_t vfs_in, mp_obj_t path_in) {
    mp_obj_tar_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *path = mp_obj_str_get_str(path_in);

    size_t fsize;
    uint8_t attrib;
    mp_int_t seconds = 0;

    if (path[0] == 0 || (path[0] == '/' && path[1] == 0)) {
        // stat root directory
        fsize = 0;
        attrib = MTAR_TDIR;
    } else {
        mtar_header_t header;
        if (path[0] == '/') {
            // tar file doesn't have leading slash in paths
            path += 1;
        }
        int res = mtar_find(&self->tar, path, &header);
        if (res != MTAR_ESUCCESS) {
            mp_raise_OSError(mtar_e_to_errno_table[-res]);
        }
        fsize = header.size;
        seconds = header.mtime;
        attrib = header.type;

        #ifndef MICROPY_EPOCH_IS_1970
        seconds -= TIMEUTILS_SECONDS_1970_TO_2000;
        #endif
    }

    mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));
    mp_int_t mode = 0;
    if (attrib & MTAR_TDIR) {
        mode |= MP_S_IFDIR;
    } else {
        mode |= MP_S_IFREG;
    }
    t->items[0] = MP_OBJ_NEW_SMALL_INT(mode); // st_mode
    t->items[1] = MP_OBJ_NEW_SMALL_INT(0); // st_ino
    t->items[2] = MP_OBJ_NEW_SMALL_INT(0); // st_dev
    t->items[3] = MP_OBJ_NEW_SMALL_INT(0); // st_nlink
    t->items[4] = MP_OBJ_NEW_SMALL_INT(0); // st_uid
    t->items[5] = MP_OBJ_NEW_SMALL_INT(0); // st_gid
    t->items[6] = mp_obj_new_int_from_uint(fsize); // st_size
    t->items[7] = mp_obj_new_int_from_uint(seconds); // st_atime
    t->items[8] = mp_obj_new_int_from_uint(seconds); // st_mtime
    t->items[9] = mp_obj_new_int_from_uint(seconds); // st_ctime

    return MP_OBJ_FROM_PTR(t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tar_vfs_stat_obj, tar_vfs_stat);

// Get the status of a VFS.
STATIC mp_obj_t tar_vfs_statvfs(mp_obj_t vfs_in, mp_obj_t path_in) {
    // mp_obj_tar_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    (void)vfs_in;
    (void)path_in;

    mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));

    t->items[0] = MP_OBJ_NEW_SMALL_INT(0); // f_bsize
    t->items[1] = MP_OBJ_NEW_SMALL_INT(0); // f_frsize
    t->items[2] = MP_OBJ_NEW_SMALL_INT(0); // f_blocks
    t->items[3] = MP_OBJ_NEW_SMALL_INT(0); // f_bfree
    t->items[4] = MP_OBJ_NEW_SMALL_INT(0); // f_bavail
    t->items[5] = MP_OBJ_NEW_SMALL_INT(0); // f_files
    t->items[6] = MP_OBJ_NEW_SMALL_INT(0); // f_ffree
    t->items[7] = MP_OBJ_NEW_SMALL_INT(0); // f_favail
    t->items[8] = MP_OBJ_NEW_SMALL_INT(0); // f_flags
    t->items[9] = MP_OBJ_NEW_SMALL_INT(MICROPY_ALLOC_PATH_MAX); // f_namemax

    return MP_OBJ_FROM_PTR(t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tar_vfs_statvfs_obj, tar_vfs_statvfs);

STATIC mp_obj_t vfs_tar_mount(mp_obj_t self_in, mp_obj_t readonly, mp_obj_t mkfs) {
    fs_tar_user_mount_t *self = MP_OBJ_TO_PTR(self_in);

    // Read-only device indicated by writeblocks[0] == MP_OBJ_NULL.
    // User can specify read-only device by:
    //  1. readonly=True keyword argument
    //  2. nonexistent writeblocks method (then writeblocks[0] == MP_OBJ_NULL already)
    
    // if (mp_obj_is_true(readonly)) {
        self->blockdev.writeblocks[0] = MP_OBJ_NULL;
    // }

    // check if we need to make the filesystem
    // FRESULT res = (self->blockdev.flags & MP_BLOCKDEV_FLAG_NO_FILESYSTEM) ? FR_NO_FILESYSTEM : FR_OK;
    // if (res == FR_NO_FILESYSTEM && mp_obj_is_true(mkfs)) {
    //     uint8_t working_buf[FF_MAX_SS];
    //     res = f_mkfs(&self->tar, FM_TAR | FM_SFD, 0, working_buf, sizeof(working_buf));
    // }
    if (self->blockdev.flags & MP_BLOCKDEV_FLAG_NO_FILESYSTEM) {
        mp_raise_OSError(MP_ENODEV);
    }
    // self->blockdev.flags &= ~MP_BLOCKDEV_FLAG_NO_FILESYSTEM;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(vfs_tar_mount_obj, vfs_tar_mount);

STATIC mp_obj_t vfs_tar_umount(mp_obj_t self_in) {
    (void)self_in;
    // keep the TAR filesystem mounted internally so the VFS methods can still be used
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tar_vfs_umount_obj, vfs_tar_umount);

STATIC const mp_rom_map_elem_t tar_vfs_locals_dict_table[] = {
    #if _FS_REENTRANT
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&tar_vfs_del_obj) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_mkfs), MP_ROM_PTR(&tar_vfs_mkfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&tar_vfs_open_obj) },
    { MP_ROM_QSTR(MP_QSTR_ilistdir), MP_ROM_PTR(&tar_vfs_ilistdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkdir), MP_ROM_PTR(&tar_vfs_mkdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmdir), MP_ROM_PTR(&tar_vfs_rmdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_chdir), MP_ROM_PTR(&tar_vfs_chdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_getcwd), MP_ROM_PTR(&tar_vfs_getcwd_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove), MP_ROM_PTR(&tar_vfs_remove_obj) },
    { MP_ROM_QSTR(MP_QSTR_rename), MP_ROM_PTR(&tar_vfs_rename_obj) },
    { MP_ROM_QSTR(MP_QSTR_stat), MP_ROM_PTR(&tar_vfs_stat_obj) },
    { MP_ROM_QSTR(MP_QSTR_statvfs), MP_ROM_PTR(&tar_vfs_statvfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_mount), MP_ROM_PTR(&vfs_tar_mount_obj) },
    { MP_ROM_QSTR(MP_QSTR_umount), MP_ROM_PTR(&tar_vfs_umount_obj) },
};
STATIC MP_DEFINE_CONST_DICT(tar_vfs_locals_dict, tar_vfs_locals_dict_table);

STATIC const mp_vfs_proto_t tar_vfs_proto = {
    .import_stat = tar_vfs_import_stat,
};

const mp_obj_type_t mp_tar_vfs_type = {
    { &mp_type_type },
    .name = MP_QSTR_VfsTAR,
    .make_new = tar_vfs_make_new,
    .protocol = &tar_vfs_proto,
    .locals_dict = (mp_obj_dict_t *)&tar_vfs_locals_dict,

};

#endif // MICROPY_VFS_TAR
