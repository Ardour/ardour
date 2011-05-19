/*
  RDFF - RDF in RIFF
  Copyright 2011 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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

/**
   RDFF file or stream.
 */
typedef struct _RDFF* RDFF;

/**
   Status codes for function returns.
*/
typedef enum {
	RDFF_STATUS_OK            = 0,  /**< Success. */
	RDFF_STATUS_UNKNOWN_ERROR = 1,  /**< Unknown error. */
	RDFF_STATUS_EOF           = 2,  /**< End of file. */
	RDFF_STATUS_CORRUPT       = 3   /**< Corrupt data. */
} RDFFStatus;

/**
   Generic RIFF chunk header.
*/
typedef struct {
	char     type[4];  /**< Chunk type ID. */
	uint32_t size;     /**< Size of chunk body (not including header). */
	char     data[];   /**< Chunk body. */
} PACKED RDFFChunk;

/**
   Body of a RDFF "urid" chunk.
*/
typedef struct {
	uint32_t id;     /**< Numeric ID of URI in this RDFF. */
	char     uri[];  /**< URI string. */
} PACKED RDFFURIChunk;

/**
   Body of a RDFF "trip" chunk.
*/
typedef struct {
	uint32_t subject;      /**< Subject URI ID. */
	uint32_t predicate;    /**< Predicate URI ID. */
	uint32_t object_type;  /**< Object type URI ID. */
	uint32_t object_size;  /**< Size of object data. */
	char     object[];     /**< Object data. */
} PACKED RDFFTripleChunk;

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
               uint32_t    len,
               const char* uri);

/**
   Write a key/value record to @a file.
*/
RDFFStatus
rdff_write_triple(RDFF        file,
                  uint32_t    subject,
                  uint32_t    predicate,
                  uint32_t    object_type,
                  uint32_t    object_size,
                  const void* object);

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
   Return true iff @a chunk is a URI chunk.
*/
bool
rdff_chunk_is_uri(RDFFChunk* chunk);

/**
   Return true iff @a chunk is a Triple chunk.
*/
bool
rdff_chunk_is_triple(RDFFChunk* chunk);

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
