//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2025 ofravensclaw
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	WAD I/O functions.
//

#include <stdio.h>
#include <string.h>

#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"

#include <revolution/dvd.h>
#include <revolution/os.h>

void* RVLMalloc(size_t size, BOOL mem1);
void RVLFree(void* ptr, BOOL mem1);

typedef struct
{
    wad_file_t wad;
    DVDFileInfo fileInfo;
} rvl_wad_file_t;

extern wad_file_class_t rvl_wad_file;

static wad_file_t *W_RVL_OpenFile(char *path)
{
    rvl_wad_file_t *result;
    DVDFileInfo fileInfo;

    BOOL ret = DVDOpen(path, &fileInfo);

    if (!ret)
    {
        return NULL;
    }

    // Create a new rvl_wad_file_t to hold the file handle.

    result = Z_Malloc(sizeof(rvl_wad_file_t), PU_STATIC, 0);
    result->wad.file_class = &rvl_wad_file;
    result->wad.mapped = NULL;
    result->wad.length = fileInfo.length;
    memcpy(&result->fileInfo, &fileInfo, sizeof(DVDFileInfo));

    return &result->wad;
}

static void W_RVL_CloseFile(wad_file_t *wad)
{
    rvl_wad_file_t *rvl_wad;

    rvl_wad = (rvl_wad_file_t *) wad;

    DVDClose(&rvl_wad->fileInfo);
    Z_Free(rvl_wad);
}

// Read data from the specified position in the file into the 
// provided buffer.  Returns the number of bytes read.

size_t W_RVL_Read(wad_file_t *wad, unsigned int offset,
                   void *buffer, size_t buffer_len)
{
    rvl_wad_file_t *rvl_wad;
    size_t result;

    rvl_wad = (rvl_wad_file_t *) wad;

    // Reads need to be 32-byte aligned.
    void* read_buffer = RVLMalloc(OSRoundUp32B(buffer_len), TRUE);
    result = DVDRead(&rvl_wad->fileInfo, read_buffer, OSRoundUp32B(buffer_len), offset);
    if (result > 0)
    {
        if (result > buffer_len)
        {
            //OSReport("W_RVL_Read: buffer_len = %d, result = %d\n", buffer_len, result);
            // OSHalt("W_RVL_Read: read more data than requested");
        }

        // it just gets trimmed
        memcpy(buffer, read_buffer, result);
    }
    RVLFree(read_buffer, TRUE);

    return result;
}


wad_file_class_t rvl_wad_file = 
{
    W_RVL_OpenFile,
    W_RVL_CloseFile,
    W_RVL_Read,
};


