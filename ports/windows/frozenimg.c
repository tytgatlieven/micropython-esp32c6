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

#include "py/mpconfig.h"

#if MICROPY_PY_FROZENIMG

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <extmod/vfs.h>
#include <extmod/vfs_tar.h>
#include <extmod/stream_blockdev.h>
#include <tchar.h>
#include <windows.h>

char* GetMainModulePath(void)
{
    TCHAR* buf    = NULL;
    DWORD  bufLen = 256;
    DWORD  retLen;

    while (32768 >= bufLen)
    {
        if (!(buf = (TCHAR*)malloc(sizeof(TCHAR) * (size_t)bufLen)))
        {
            /* Insufficient memory */
            return NULL;
        }

        if (!(retLen = GetModuleFileName(NULL, buf, bufLen)))
        {
            /* GetModuleFileName failed */
            free(buf);
            return NULL;
        }
        else if (bufLen > retLen)
        {
            /* Success */
            char * result = _tcsdup(buf); /* Caller should free returned pointer */
            free(buf);
            return result;
        }

        free(buf);
        bufLen <<= 1;
    }

    /* Path too long */
    return NULL;
}

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


int ReadFromExeFile(void)
{
  DWORD read; 
  // BYTE* data;

  // Open exe file
  char* filename;

  if (!(filename = GetMainModulePath()))
  {
        /* Insufficient memory or path too long */
        // printf("Insufficient memory");
        return -1;
  }

  
  // DWORD pathlen = GetModuleFileName(NULL, NULL, 0);
  // printf("pathlen %ld\n", pathlen);

  // char *filename = malloc(pathlen);
  // GetModuleFileName(NULL, filename, pathlen);

  // printf("module %s\n", filename);

  HANDLE hFile = CreateFile((CHAR*)filename, GENERIC_READ, FILE_SHARE_READ,
    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if(INVALID_HANDLE_VALUE == hFile)
      return -1;

  IMAGE_DOS_HEADER dosheader;
  ReadFile(hFile, &dosheader, sizeof(dosheader), &read, NULL);

  // Locate PE header
  IMAGE_NT_HEADERS32 header;
  SetFilePointer(hFile, dosheader.e_lfanew, NULL, FILE_BEGIN);
  ReadFile(hFile, &header, sizeof(header), &read, NULL);
    //  (IMAGE_NT_HEADERS32*)(dosheader + dosheader->e_lfanew);
  
  if(dosheader.e_magic != IMAGE_DOS_SIGNATURE ||
    header.Signature != IMAGE_NT_SIGNATURE) {
    CloseHandle(hFile);
    free(filename);
    return -1;
  }

  // For each section
  // size_t numSections = header.FileHeader.NumberOfSections;
    // (IMAGE_SECTION_HEAZDER*)((BYTE*)header + sizeof(IMAGE_NT_HEADERS32));
  DWORD maxpointer = 0, exesize = 0;
  // printf("IMAGE_SECTION_HEADER sections %d\n", header.FileHeader.NumberOfSections);
  for(int i = 0; i < header.FileHeader.NumberOfSections; i++) {
    // printf("IMAGE_SECTION_HEADER %d\n", i);
    IMAGE_SECTION_HEADER sectiontable;
    ReadFile(hFile, &sectiontable, sizeof(sectiontable), &read, NULL);

    if(sectiontable.PointerToRawData > maxpointer) {
      maxpointer = sectiontable.PointerToRawData;
      exesize = sectiontable.PointerToRawData + sectiontable.SizeOfRawData;
    }
    // sectiontable++;
  }

  // Seek to the overlay
  DWORD filesize = GetFileSize(hFile, NULL);
  SetFilePointer(hFile, exesize, NULL, FILE_BEGIN);
  DWORD datasize = filesize - exesize;
  char *data = (char*)malloc(filesize - exesize + 1);
  ReadFile(hFile, data, datasize, &read, NULL);
  CloseHandle(hFile);
  char *result = memmem(data, datasize, "main.py", 7);
  if (result == NULL) {
    free(data);
    free(filename);
    return -1;
  }
  int offset = result - data;
  exesize += offset;
  datasize -= offset;

  if (datasize <= 0) {
    free(filename);
    return -1;
  }

  mp_obj_t open_args[2] = {
        mp_obj_new_str(filename, strlen(filename)),
        // mp_obj_new_str("frozen.tar", 11),
        MP_OBJ_NEW_QSTR(MP_QSTR_rb)
  };
  mp_obj_t exe_file = mp_builtin_open(2, open_args, (mp_map_t *)&mp_const_empty_map);

  mpy_stream_bdev_obj_t *sbd = m_new_obj(mpy_stream_bdev_obj_t);
  sbd->base.type = &mpy_stream_bdev_type;
  sbd->stream = exe_file;
  sbd->block_size = 512;
  sbd->start = exesize;
  sbd->len = datasize;
  // sbd->start = 0;
  // sbd->len = 10240;
  mp_obj_t sbd_obj = MP_OBJ_FROM_PTR(sbd);

  // Mount the tar at the a hardcoded path of our internal VFS
  mp_obj_t args[2] = {
      mp_type_vfs_tar.make_new(&mp_type_vfs_tar, 1, 0, &sbd_obj),
      mp_obj_new_str_via_qstr("/:", 2),
  };
  mp_vfs_mount(2, args, (mp_map_t *)&mp_const_empty_map);
  free(filename);

  // Process the data
  // *(data + datasize) = '\0';
  // MessageBox(0, (CHAR*)data, "AppName", MB_ICONINFORMATION);
  // free(data);
  return 0;
}


#endif