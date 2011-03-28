/*
  Portable file-based LV2 Persist implementation.
  See <http://lv2plug.in/ns/ext/persist> for details.

  Copyright 2011 David Robillard <http://drobilla.net>
 
  This is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
 
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this sofware; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  02110-1301 USA.
*/

#ifndef LV2PFILE_H
#define LV2PFILE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __GNUC__
#    define PACKED __attribute__((__packed__))
#else
#    define PACKED
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _LV2PFile*          LV2PFile;
typedef struct _LV2PFile_Iterator* LV2PFile_Iterator;

typedef enum {
	LV2_PFILE_OK            = 0,
	LV2_PFILE_UNKNOWN_ERROR = 1,
	LV2_PFILE_EOF           = 2,
	LV2_PFILE_CORRUPT       = 3
} LV2PFileStatus;

typedef struct {
	char     type[4];
	uint32_t size;
	char     data[];
} PACKED LV2PFileChunkHeader;

typedef struct {
	uint32_t id;
	char     uri[];
} PACKED LV2PFileURIChunk;

typedef struct {
	uint32_t key;
	uint32_t type;
	uint32_t size;
	char     value[];
} PACKED LV2PFileValueChunk;
		
/**
   Open/Create a new persist file.
*/
LV2PFile
lv2_pfile_open(const char* path, bool write);

/**
   Write a URI ID to @a file.
*/
LV2PFileStatus
lv2_pfile_write_uri(LV2PFile    file,
                    uint32_t    id,
                    const char* uri,
                    uint32_t    size);

/**
   Write a key/value record to @a file.
*/
LV2PFileStatus
lv2_pfile_write_value(LV2PFile    file,
                      uint32_t    key,
                      const void* value,
                      uint32_t    size,
                      uint32_t    type);
LV2PFileStatus
lv2_pfile_read_chunk(LV2PFile              file,
                     LV2PFileChunkHeader** buf);

/**
   Read a record from a persist file.
   @a key and @a value are allocated with malloc and must be freed by caller.
*/
#if 0
LV2PFileStatus
lv2_pfile_read(LV2PFile  file,
               char**    key,
               uint32_t* key_len,
               char**    type,
               uint32_t* type_len,
               void**    value,
               uint64_t* size);
#endif

/**
   Close @a file.
   After this call, @a file is invalid.
*/
void
lv2_pfile_close(LV2PFile file);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV2PFILE_H */
