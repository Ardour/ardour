/*
  RDFF - RDF in RIFF
  Copyright 2011 David Robillard <http://drobilla.net>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
  AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
  OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  THE POSSIBILITY OF SUCH DAMAGE.
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
