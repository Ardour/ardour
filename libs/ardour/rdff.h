/*
  RDFF - RDF in RIFF
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

#ifndef RDFF_RDFF_H
#define RDFF_RDFF_H

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

typedef struct _RDFF* RDFF;

typedef enum {
	RDFF_STATUS_OK            = 0,
	RDFF_STATUS_UNKNOWN_ERROR = 1,
	RDFF_STATUS_EOF           = 2,
	RDFF_STATUS_CORRUPT       = 3
} RDFFStatus;

/**
   Generic RIFF chunk header.
*/
typedef struct {
	char     type[4];
	uint32_t size;
	char     data[];
} PACKED RDFFChunk;

/**
   Body of a URID chunk.
*/
typedef struct {
	uint32_t id;
	char     uri[];
} PACKED RDFFURIChunk;

/**
   Body of a KVAL chunk.
*/
typedef struct {
	uint32_t key;
	uint32_t type;
	uint32_t size;
	char     value[];
} PACKED RDFFValueChunk;
		
/**
   Open/Create a new RDFF file.
*/
RDFF
rdff_open(const char* path, bool write);

/**
   Write a URI ID to @a file.
*/
RDFFStatus
rdff_write_uri(RDFF        file,
               uint32_t    id,
               const char* uri,
               uint32_t    len);

/**
   Write a key/value record to @a file.
*/
RDFFStatus
rdff_write_value(RDFF        file,
                 uint32_t    key,
                 const void* value,
                 uint32_t    size,
                 uint32_t    type);

/**
   Read a chunk from @a file.

   @param buf MUST point to an RDFFChunk dynamically allocated with malloc.
   The @a size field (i.e. (*buf)->size) MUST be set to the amount of available
   memory in the chunk (not including the header). If this is insufficient,
   *buf will be resized using realloc.
*/
RDFFStatus
rdff_read_chunk(RDFF        file,
                RDFFChunk** buf);

/**
   Close @a file.
   After this call, @a file is invalid.
*/
void
rdff_close(RDFF file);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RDFF_RDFF_H */
