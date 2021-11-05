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
#include <stdlib.h>
#include "py/runtime.h"
#include "py/mperrno.h"
#include "extmod/vfs_zip.h"
#include "shared/timeutils/timeutils.h"

// NOTE: zip with file comment are not currently supported, 
// would need variable length footer size search
typedef struct __attribute__ ((packed)) _end_of_central_directory {
    uint32_t sig;
    uint16_t disk_number;
    uint16_t disk_number_cd;
    uint16_t disk_entries;
    uint16_t total_number;
    uint32_t cd_size;
    uint32_t offset_cd;
    uint16_t comment_len;
    // uint8_t comment[]; 
} end_cent_dir_t;


typedef struct __attribute__ ((packed)) _cd_file_header {
    uint32_t sig;                   // central file header signature   4 bytes  (0x02014b50)
    uint16_t version;               // version made by                 2 bytes
    uint16_t vers_needed;           // version needed to extract       2 bytes
    uint16_t flags;                 // general purpose bit flag        2 bytes
    uint16_t compression;           // compression method              2 bytes
    uint16_t mod_time;              // last mod file time              2 bytes
    uint16_t mod_date;              // last mod file date              2 bytes
    uint32_t crc32;                 // crc-32                          4 bytes
    uint32_t size_compressed;       // compressed size                 4 bytes
    uint32_t size_uncompressed;     // uncompressed size               4 bytes
    uint16_t file_name_len;         // file name length                2 bytes
    uint16_t extra_field_len;       // extra field length              2 bytes
    uint16_t comment_len;           // file comment length             2 bytes
    uint16_t disk_num;              // disk number start               2 bytes
    uint16_t int_attr;              // internal file attributes        2 bytes
    uint32_t ext_attr;              // external file attributes        4 bytes
    uint32_t offset_local_header;   // relative offset of local header 4 bytes
    char buffer[];
    // char extra_field[];
    // char comment[];
} cd_file_header_t;

#define CD_SIG  0x02014b50
#define EOCD_SIG {0x50, 0x4b, 0x05, 0x06}
#define CRC_SEED 0xdebb20e3 


#define PATH_SEP_CHAR '/'

typedef enum {
    ENTRY_TYPE_NONE = 0,
    ENTRY_TYPE_FILE,
    ENTRY_TYPE_DIR,
} entry_type_t;

typedef struct _zip_addr {
    uint32_t block;
    uint32_t offset;
} zip_addr_t;

#define mp_obj_zip_vfs_t fs_zip_user_mount_t

/*
 * The memmem() function finds the start of the first occurrence of the
 * substring 'needle' of length 'nlen' in the memory area 'haystack' of
 * length 'hlen'.
 *
 * The return value is a pointer to the beginning of the sub-string, or
 * NULL if the substring is not found.
 */
void *memmem(const char *haystack, size_t hlen, const char *needle, size_t nlen)
{
    int needle_first;
    const char *p = haystack;
    size_t plen = hlen;

    if (!nlen)
        return NULL;

    needle_first = *(unsigned char *)needle;

    while (plen >= nlen && (p = memchr(p, needle_first, plen - nlen + 1)))
    {
        if (!memcmp(p, needle, nlen))
            return (void *)p;

        p++;
        plen = hlen - (p - haystack);
    }

    return NULL;
}

STATIC void zip_addr_inc(zip_addr_t *addr, size_t block_size, size_t seek) {
    addr->offset += seek;
    if (addr->offset >= block_size) {
        addr->block += 1;
        addr->offset -= block_size;
    }
}


STATIC int zip_read_data(fs_zip_user_mount_t *vfs, zip_addr_t addr, uint8_t *buffer, size_t length) {
    uint32_t block_size = vfs->blockdev.block_size;
    uint32_t first_read = length;
    uint32_t second_read = 0;
    if (addr.offset + length > block_size) {
        second_read = block_size - addr.offset + length;
        first_read = second_read - length;
    }
    
    int res = mp_vfs_blockdev_read_ext(&vfs->blockdev, addr.block, addr.offset, first_read, buffer);
    if (res != 0) {
        return MP_EIO;
    }
    if (second_read) {
        addr.block += 1;
        addr.offset = 0;
        res = mp_vfs_blockdev_read_ext(&vfs->blockdev, addr.block, addr.offset, second_read, buffer + first_read);
        if (res != 0) {
            return MP_EIO;
        }
    }
    return 0;
}

STATIC int zip_read_header(fs_zip_user_mount_t *vfs, zip_addr_t addr, cd_file_header_t *cd_file_header, size_t header_len) {
    uint32_t block_size = vfs->blockdev.block_size;
    uint32_t read_size = header_len ? header_len : sizeof(cd_file_header_t);
    // if (cd_file_header == NULL) {

    // }

    int res = zip_read_data(vfs, addr, (uint8_t *)cd_file_header, read_size);
    if (res != 0) {
        return res;
    }

    if (cd_file_header->sig != CD_SIG) {
        return MP_EINVAL;
    }

    if (!header_len) {
        zip_addr_inc(&addr, block_size, read_size);

        read_size = cd_file_header->file_name_len + cd_file_header->extra_field_len + cd_file_header->comment_len;

        res = zip_read_data(vfs, addr, (uint8_t*)cd_file_header->buffer, read_size);
        if (res != 0) {
            return res;
        }
    }
    return 0;
}

STATIC void zip_next_header_loc(fs_zip_user_mount_t *vfs, zip_addr_t *addr, cd_file_header_t *cd_file_header) {
    uint32_t cd_header_size = sizeof(cd_file_header_t) + cd_file_header->file_name_len + cd_file_header->extra_field_len + cd_file_header->comment_len;
    uint32_t block_size = vfs->blockdev.block_size;
    zip_addr_inc(addr, block_size, cd_header_size);
}

STATIC int zip_find_file(
        fs_zip_user_mount_t *vfs, const char * filename, 
        cd_file_header_t *cd_file_header, size_t header_len, entry_type_t *type) {
    int res;
    zip_addr_t start_addr = {vfs->cd_block, vfs->cd_offset};
    // uint32_t block_size = vfs->blockdev.block_size;
    // uint32_t read_size = sizeof(cd_file_header_t);
    uint16_t fn_len = strlen(filename);
    *type = ENTRY_TYPE_NONE;

    do {
        res = zip_read_header(vfs, start_addr, cd_file_header, header_len);
        zip_next_header_loc(vfs, &start_addr, cd_file_header); 
        if (res != 0) {
            return MP_ENOENT;
        }

        printf("%s\n", cd_file_header->buffer);
        if (strncmp(filename, cd_file_header->buffer, fn_len-1) == 0) {
            // zipfile starts with search pattern
            if (filename[fn_len-1] == '/') {
                // definitely searching for dir, zip contains file in that dir, return as found
                *type = ENTRY_TYPE_DIR;
                return 0;
            }
            if (cd_file_header->buffer[fn_len] == '/') {
                // Found a directory with the target name
                *type = ENTRY_TYPE_DIR;
                return 0;
            }
            if (cd_file_header->buffer[fn_len] == 0) {
                // Found a file match
                *type = ENTRY_TYPE_FILE;
                return 0;
            }
        }
    } while (1);

    return MP_EIO;
}

STATIC mp_import_stat_t zip_vfs_import_stat(void *vfs_in, const char *path) {
    fs_zip_user_mount_t *vfs = vfs_in;
    // Zip files don't have leading / in the filenames
    if (path[0] == PATH_SEP_CHAR) {
        path += 1;
    }
    assert(vfs != NULL);
    size_t header_len = sizeof(cd_file_header_t) + strlen(path) + 2;
    cd_file_header_t *header = calloc(1, header_len);   
    entry_type_t type = ENTRY_TYPE_NONE;
    int res = zip_find_file(vfs, path, header, header_len, &type);
    free(header);
    if (res == 0) {
        if (type == ENTRY_TYPE_DIR) {
            return MP_IMPORT_STAT_DIR;
        } else if (type == ENTRY_TYPE_FILE) {
            return MP_IMPORT_STAT_FILE;
        }
    }
    return MP_IMPORT_STAT_NO_EXIST;
}

static void raise_error_read_only() {
    mp_raise_NotImplementedError(MP_ERROR_TEXT("vfs_zip is read only"));
}

// static int block_write(mzip_t *zip, const void *data, unsigned size) {
//   fs_zip_user_mount_t *vfs = (fs_zip_user_mount_t *)zip->stream;
//   size_t block = zip->pos / vfs->blockdev.block_size;
//   size_t offset = zip->pos % vfs->blockdev.block_size;
//   int res = mp_vfs_blockdev_write_ext(&vfs->blockdev, block, offset, size, data);
//   if (res == 0) {
//     return 0;
//   }
//   return MTAR_EWRITEFAIL;
// }

// static int block_read(mzip_t *zip, void *data, unsigned size) {
//   fs_zip_user_mount_t *vfs = (fs_zip_user_mount_t *)zip->stream;
//   size_t block = zip->pos / vfs->blockdev.block_size;
//   size_t offset = zip->pos % vfs->blockdev.block_size;
//   int res = mp_vfs_blockdev_read_ext(&vfs->blockdev, block, offset, size, data);
//   if (res == 0) {
//     return 0;
//   }
//   return MTAR_EREADFAIL;
// }

// static int block_seek(mzip_t *zip, unsigned offset) {
//   (void)offset;
//   (void)zip;
//   return 0;
// }

// static int block_close(mzip_t *zip) {
//     (void)zip;
//   return 0;
// }

STATIC mp_obj_t zip_vfs_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    // create new object
    fs_zip_user_mount_t *vfs = m_new_obj(fs_zip_user_mount_t);
    memset(vfs, 0, sizeof(fs_zip_user_mount_t));

    vfs->base.type = type;
    // vfs->zip.stream = (void *)vfs;

    // Initialise underlying block device
    vfs->blockdev.flags = MP_BLOCKDEV_FLAG_FREE_OBJ;
    mp_vfs_blockdev_init(&vfs->blockdev, args[0]);
    mp_obj_t bsize = mp_vfs_blockdev_ioctl(&vfs->blockdev, MP_BLOCKDEV_IOCTL_BLOCK_SIZE, 0);
    uint32_t block_size;
    if (mp_obj_is_small_int(bsize)) {
        block_size = MP_OBJ_SMALL_INT_VALUE(bsize);
    } else {
        block_size = 512; 
    }

    if (block_size < sizeof(end_cent_dir_t)) {
        mp_raise_ValueError(MP_ERROR_TEXT("block_size too small"));
    }
    vfs->blockdev.block_size = block_size;

    /* Init zip struct and functions */
    // vfs->zip.write = block_write;
    // vfs->zip.read = block_read;
    // vfs->zip.seek = block_seek;
    // vfs->zip.close = block_close;

    /* Verify end of central directory (zip footer) */

    size_t search_size = block_size + sizeof(end_cent_dir_t) - 1;
    uint8_t *search = calloc(1, search_size);

    size_t num_blocks;
    mp_obj_t bcount = mp_vfs_blockdev_ioctl(&vfs->blockdev, MP_BLOCKDEV_IOCTL_BLOCK_COUNT, 0);
    if (mp_obj_is_small_int(bcount)) {
        num_blocks = MP_OBJ_SMALL_INT_VALUE(bcount);
    } else {
        free(search);
        mp_raise_OSError(MP_EIO);
    }
    size_t offset = 1 + block_size - sizeof(end_cent_dir_t);
    int res = mp_vfs_blockdev_read_ext(&vfs->blockdev, num_blocks-2, offset, sizeof(end_cent_dir_t)-1, search);
    res |= mp_vfs_blockdev_read_ext(&vfs->blockdev, num_blocks-1, 0, block_size, &search[sizeof(end_cent_dir_t)-1]);
    if (res != 0) {
        free(search);
        mp_raise_OSError(MP_EIO);
    }
    
    uint8_t sig[] = EOCD_SIG;
    end_cent_dir_t* footer = memmem((char *)search, search_size, (char *)&sig, 4);
    if (footer == NULL) {
        vfs->blockdev.flags |= MP_BLOCKDEV_FLAG_NO_FILESYSTEM;
    } else {
        if (footer->disk_number != 0 || footer->disk_number_cd != 0) {
            free(search);
            mp_raise_ValueError(MP_ERROR_TEXT("only single-file zip supported"));
        }
        // uint32_t cd_end_offset = sizeof(footer) + footer->cd_size;
        
        vfs->cd_block = footer->offset_cd / block_size;
        vfs->cd_offset = footer->offset_cd % block_size;
        vfs->cd_size = footer->cd_size;
    }

    free(search);
    return MP_OBJ_FROM_PTR(vfs);
}

#if _FS_REENTRANT
STATIC mp_obj_t zip_vfs_del(mp_obj_t self_in) {
    // mp_obj_zip_vfs_t *self = MP_OBJ_TO_PTR(self_in);
    // // f_umount only needs to be called to release the sync object
    // f_umount(&self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(zip_vfs_del_obj, zip_vfs_del);
#endif

STATIC mp_obj_t zip_vfs_mkfs(mp_obj_t bdev_in) {
    (void)bdev_in;
    raise_error_read_only();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(zip_vfs_mkfs_fun_obj, zip_vfs_mkfs);
STATIC MP_DEFINE_CONST_STATICMETHOD_OBJ(zip_vfs_mkfs_obj, MP_ROM_PTR(&zip_vfs_mkfs_fun_obj));

typedef struct _mp_vfs_zip_ilistdir_it_t {
    mp_obj_base_t base;
    mp_fun_1_t iternext;
    bool is_str;
    fs_zip_user_mount_t *vfs;
    zip_addr_t next_header_addr;
    char path[MICROPY_ALLOC_PATH_MAX + 1];
} mp_vfs_zip_ilistdir_it_t;

STATIC mp_obj_t mp_vfs_zip_ilistdir_it_iternext(mp_obj_t self_in) {
    int res;
    mp_vfs_zip_ilistdir_it_t *self = MP_OBJ_TO_PTR(self_in);

    uint32_t block_size = self->vfs->blockdev.block_size;

    for (;;) {
        /* Load header */
        cd_file_header_t header;

        res = zip_read_header(self->vfs, self->next_header_addr, &header, sizeof(header));
        // int res = zip_read_header(&self, &h);
        if (res != 0) {
            break;
        }

        char *next_path = calloc(1, header.file_name_len);
        zip_addr_t filename_addr = self->next_header_addr;
        zip_addr_inc(&filename_addr, block_size, sizeof(header));
        int res = zip_read_data(self->vfs, filename_addr, (uint8_t *)next_path, header.file_name_len);
        if (res != 0) {
            // stop on error or end of dir
            free(next_path);
            break;
        }

        
        size_t match_len = strnlen(self->path, header.file_name_len);
        if (next_path[0] == 0 || strncmp(self->path, next_path, match_len) != 0) {
            // stop on error or end of dir
            free(next_path);
            break;
        }
        const char *name = &next_path[match_len];
        size_t name_length = strcspn(name, "/");
        if (name_length < strlen(name) - 1) {
            // file in subdirectory
            free(next_path);
            zip_next_header_loc(self->vfs, &self->next_header_addr, &header); 
            continue;
        }

        // make 4-tuple with info about this entry
        mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(4, NULL));
        if (self->is_str) {
            t->items[0] = mp_obj_new_str(name, name_length);
        } else {
            t->items[0] = mp_obj_new_bytes((const byte *)name, name_length);
        }
        free(next_path);
        // if (type == ENTRY_TYPE_DIR) {
        //     // dir
        //     t->items[1] = MP_OBJ_NEW_SMALL_INT(MP_S_IFDIR);
        // } else {
        //     // file
            t->items[1] = MP_OBJ_NEW_SMALL_INT(MP_S_IFREG);
        // }
        t->items[2] = MP_OBJ_NEW_SMALL_INT(0); // no inode number
        t->items[3] = mp_obj_new_int_from_uint(header.size_uncompressed);

        zip_next_header_loc(self->vfs, &self->next_header_addr, &header); 
        if (res != 0) {
            break;
        }

        return MP_OBJ_FROM_PTR(t);
    }
    return MP_OBJ_STOP_ITERATION;
}

STATIC mp_obj_t zip_vfs_ilistdir_func(size_t n_args, const mp_obj_t *args) {
    mp_obj_zip_vfs_t *vfs = MP_OBJ_TO_PTR(args[0]);
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
    if (path[0] == PATH_SEP_CHAR) {
        // zip file doesn't have leading slash in paths
        path += 1;
    }
    // Create a new iterator object to list the dir
    mp_vfs_zip_ilistdir_it_t *iter = m_new_obj(mp_vfs_zip_ilistdir_it_t);
    memset(iter, 0, sizeof(mp_vfs_zip_ilistdir_it_t));
    iter->base.type = &mp_type_polymorph_iter;
    iter->iternext = mp_vfs_zip_ilistdir_it_iternext;
    iter->is_str = is_str_type;
    iter->vfs = vfs;

    if (path[0] == 0) {
        iter->next_header_addr.block = vfs->cd_block;
        iter->next_header_addr.offset = vfs->cd_offset;
    } else {
        size_t header_len = sizeof(cd_file_header_t) + strlen(path) + 2;
        cd_file_header_t *header = calloc(1, header_len);
        entry_type_t type = ENTRY_TYPE_NONE;
        int res = zip_find_file(vfs, path, header, header_len, &type);
        //todo pass start details to iter
        free(header);
        if (res != 0) {
            mp_raise_OSError(res);
        }
        // res = zip_read_header(&vfs, &iter->dir);
        if (type != ENTRY_TYPE_DIR) {
            mp_raise_OSError(MP_ENOTDIR);
        }
    }
    // memcpy(&iter->zip, &vfs, sizeof(iter->zip));

    size_t slen = strlen(path);
    if (slen != 0) {
        strncpy(iter->path, path, sizeof(iter->path)-2);
        if (iter->path[slen-1] != PATH_SEP_CHAR) {
            iter->path[slen] = PATH_SEP_CHAR;
            iter->path[slen+1] = 0;
        }
    }

    return MP_OBJ_FROM_PTR(iter);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(zip_vfs_ilistdir_obj, 1, 2, zip_vfs_ilistdir_func);


STATIC mp_obj_t zip_vfs_remove(mp_obj_t vfs_in, mp_obj_t path_in) {
    (void)vfs_in;
    (void)path_in;
    raise_error_read_only();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(zip_vfs_remove_obj, zip_vfs_remove);

STATIC mp_obj_t zip_vfs_rmdir(mp_obj_t vfs_in, mp_obj_t path_in) {
    (void)vfs_in;
    (void)path_in;
    raise_error_read_only();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(zip_vfs_rmdir_obj, zip_vfs_rmdir);

STATIC mp_obj_t zip_vfs_rename(mp_obj_t vfs_in, mp_obj_t path_in, mp_obj_t path_out) {
    (void)vfs_in;
    (void)path_in;
    (void)path_out;
    raise_error_read_only();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(zip_vfs_rename_obj, zip_vfs_rename);

STATIC mp_obj_t zip_vfs_mkdir(mp_obj_t vfs_in, mp_obj_t path_o) {
    (void)vfs_in;
    (void)path_o;
    raise_error_read_only();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(zip_vfs_mkdir_obj, zip_vfs_mkdir);

// Change current directory.
STATIC mp_obj_t zip_vfs_chdir(mp_obj_t vfs_in, mp_obj_t path_in) {
    mp_obj_zip_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    VSTR_FIXED(path, MICROPY_ALLOC_PATH_MAX);
    const char *cpath = mp_obj_str_get_str(path_in);
    size_t cpath_len = strlen(cpath);

    if (cpath[0] == PATH_SEP_CHAR) {
        vstr_add_strn(&path, &cpath[1], cpath_len-1);
    } else {
        size_t cwd_len = strlen(self->cwd);
        if (cwd_len) {
            vstr_add_strn(&path, self->cwd, cwd_len);
            if (self->cwd[cwd_len-1] != PATH_SEP_CHAR) {
                vstr_add_char(&path, PATH_SEP_CHAR);
            }
        }
        if (path.len && path.buf[path.len-1] != PATH_SEP_CHAR) {
            vstr_add_char(&path, PATH_SEP_CHAR);
        }
        if (cpath_len >= 2 && cpath[0] == '.' && cpath[1] == PATH_SEP_CHAR) {
            cpath = &cpath[2];
            cpath_len -= 2;
        }
        vstr_add_strn(&path, cpath, cpath_len);
    }
    entry_type_t type = ENTRY_TYPE_NONE;
    size_t header_len = sizeof(cd_file_header_t) + cpath_len + 2;
    cd_file_header_t *header = calloc(1, header_len);    
    int res = zip_find_file(self, vstr_str(&path), header, header_len, &type);
    free(header);
    if (res == 0) {
        if (type != ENTRY_TYPE_DIR) {
            mp_raise_OSError(MP_EACCES);
        }
    } else {
        mp_raise_OSError(MP_ENOENT);
    }
    strncpy(self->cwd, vstr_str(&path), vstr_len(&path));
        
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(zip_vfs_chdir_obj, zip_vfs_chdir);

// Get the current directory.
STATIC mp_obj_t zip_vfs_getcwd(mp_obj_t vfs_in) {
    mp_obj_zip_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    return mp_obj_new_str(self->cwd, strlen(self->cwd));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(zip_vfs_getcwd_obj, zip_vfs_getcwd);

// Get the status of a file or directory.
STATIC mp_obj_t zip_vfs_stat(mp_obj_t vfs_in, mp_obj_t path_in) {
    mp_obj_zip_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *path = mp_obj_str_get_str(path_in);

    size_t fsize;
    // uint8_t attrib;
    mp_int_t seconds = 0;

    if (path[0] == 0 || (path[0] == PATH_SEP_CHAR && path[1] == 0)) {
        // stat root directory
        fsize = 0;
        // attrib = ENTRY_TYPE_DIR;
    } else {
        if (path[0] == PATH_SEP_CHAR) {
            // zip file doesn't have leading slash in paths
            path += 1;
        }
        entry_type_t type = ENTRY_TYPE_NONE;
        size_t header_len = sizeof(cd_file_header_t) + strlen(path) + 2;
        cd_file_header_t *header = calloc(1, header_len);            
        int res = zip_find_file(self, path, header, header_len, &type);
        if (res != 0) {
            mp_raise_OSError(res);
        }
        fsize = header->size_uncompressed;
        seconds = header->mod_date;
        // attrib = header->type;
        free(header);
        #ifndef MICROPY_EPOCH_IS_1970
        seconds -= TIMEUTILS_SECONDS_1970_TO_2000;
        #endif
    }

    mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));
    mp_int_t mode = 0;
    // if (attrib & ENTRY_TYPE_DIR) {
    //     mode |= MP_S_IFDIR;
    // } else {
        mode |= MP_S_IFREG;
    // }
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
STATIC MP_DEFINE_CONST_FUN_OBJ_2(zip_vfs_stat_obj, zip_vfs_stat);

// Get the status of a VFS.
STATIC mp_obj_t zip_vfs_statvfs(mp_obj_t vfs_in, mp_obj_t path_in) {
    // mp_obj_zip_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
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
STATIC MP_DEFINE_CONST_FUN_OBJ_2(zip_vfs_statvfs_obj, zip_vfs_statvfs);

STATIC mp_obj_t vfs_zip_mount(mp_obj_t self_in, mp_obj_t readonly, mp_obj_t mkfs) {
    fs_zip_user_mount_t *self = MP_OBJ_TO_PTR(self_in);

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
    //     res = f_mkfs(&self, FM_TAR | FM_SFD, 0, working_buf, sizeof(working_buf));
    // }
    if (self->blockdev.flags & MP_BLOCKDEV_FLAG_NO_FILESYSTEM) {
        mp_raise_OSError(MP_ENODEV);
    }
    // self->blockdev.flags &= ~MP_BLOCKDEV_FLAG_NO_FILESYSTEM;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(vfs_zip_mount_obj, vfs_zip_mount);

STATIC mp_obj_t vfs_zip_umount(mp_obj_t self_in) {
    (void)self_in;
    // keep the TAR filesystem mounted internally so the VFS methods can still be used
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(zip_vfs_umount_obj, vfs_zip_umount);

STATIC const mp_rom_map_elem_t zip_vfs_locals_dict_table[] = {
    #if _FS_REENTRANT
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&zip_vfs_del_obj) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_mkfs), MP_ROM_PTR(&zip_vfs_mkfs_obj) },
    // { MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&zip_vfs_open_obj) },
    { MP_ROM_QSTR(MP_QSTR_ilistdir), MP_ROM_PTR(&zip_vfs_ilistdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkdir), MP_ROM_PTR(&zip_vfs_mkdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmdir), MP_ROM_PTR(&zip_vfs_rmdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_chdir), MP_ROM_PTR(&zip_vfs_chdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_getcwd), MP_ROM_PTR(&zip_vfs_getcwd_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove), MP_ROM_PTR(&zip_vfs_remove_obj) },
    { MP_ROM_QSTR(MP_QSTR_rename), MP_ROM_PTR(&zip_vfs_rename_obj) },
    { MP_ROM_QSTR(MP_QSTR_stat), MP_ROM_PTR(&zip_vfs_stat_obj) },
    { MP_ROM_QSTR(MP_QSTR_statvfs), MP_ROM_PTR(&zip_vfs_statvfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_mount), MP_ROM_PTR(&vfs_zip_mount_obj) },
    { MP_ROM_QSTR(MP_QSTR_umount), MP_ROM_PTR(&zip_vfs_umount_obj) },
};
STATIC MP_DEFINE_CONST_DICT(zip_vfs_locals_dict, zip_vfs_locals_dict_table);

STATIC const mp_vfs_proto_t zip_vfs_proto = {
    .import_stat = zip_vfs_import_stat,
};

const mp_obj_type_t mp_type_vfs_zip = {
    { &mp_type_type },
    .name = MP_QSTR_VfsTAR,
    .make_new = zip_vfs_make_new,
    .protocol = &zip_vfs_proto,
    .locals_dict = (mp_obj_dict_t *)&zip_vfs_locals_dict,

};

#endif // MICROPY_VFS_TAR
