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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdff.h"

#define CHUNK_ID_LEN 4

static const char FILE_TYPE[CHUNK_ID_LEN]  = "RDFF";  /* RDFF File ID */
static const char CHUNK_TRIP[CHUNK_ID_LEN] = "trip";  /* Triple Chunk ID */
static const char CHUNK_URID[CHUNK_ID_LEN] = "urid";  /* URI-ID Chunk ID*/

struct _RDFF {
	FILE*    fd;
	uint32_t size;
	bool     write;
};

RDFF
rdff_open(const char* path, bool write)
{
	FILE* fd = fopen(path, (write ? "w" : "r"));
	if (!fd) {
		fprintf(stderr, "%s\n", strerror(errno));
		return NULL;
	}

	uint32_t size = 0;

	if (write) {
		fwrite("RIFF", CHUNK_ID_LEN, 1, fd);    /* RIFF chunk ID */
		fwrite(&size, sizeof(size), 1, fd);     /* RIFF chunk size */
		fwrite(FILE_TYPE, CHUNK_ID_LEN, 1, fd); /* File type */
	} else {
		char magic[CHUNK_ID_LEN];
		if (fread(magic, CHUNK_ID_LEN, 1, fd) != 1
		    || strncmp(magic, "RIFF", CHUNK_ID_LEN)) {
			fclose(fd);
			fprintf(stderr, "%s: error: not a RIFF file\n", path);
			return NULL;
		}

		if (fread(&size, sizeof(size), 1, fd) != 1) {
			fclose(fd);
			fprintf(stderr, "%s: error: missing RIFF chunk size\n", path);
			return NULL;
		}

		if (fread(magic, CHUNK_ID_LEN, 1, fd) != 1
		    || strncmp(magic, FILE_TYPE, CHUNK_ID_LEN)) {
			fclose(fd);
			fprintf(stderr, "%s: error: not an %s RIFF file\n",
			        FILE_TYPE, path);
			return NULL;
		}
	}

	RDFF ret = (RDFF)malloc(sizeof(struct _RDFF));
	ret->fd    = fd;
	ret->size  = size;
	ret->write = write;
	return ret;
}

#define WRITE(ptr, size, nmemb, stream) \
	if (fwrite(ptr, size, nmemb, stream) != nmemb) { \
		return RDFF_STATUS_UNKNOWN_ERROR; \
	}

RDFFStatus
rdff_write_uri(RDFF        file,
               uint32_t    id,
               uint32_t    len,
               const char* uri)
{
	const uint32_t chunk_size = sizeof(id) + len + 1;
	WRITE(CHUNK_URID,  CHUNK_ID_LEN,       1, file->fd);
	WRITE(&chunk_size, sizeof(chunk_size), 1, file->fd);
	WRITE(&id,         sizeof(id),         1, file->fd);
	WRITE(uri,         len + 1,            1, file->fd);
	if ((chunk_size % 2)) {
		WRITE("", 1, 1, file->fd);  /* pad */
	}
	file->size += 8 + chunk_size;
	return RDFF_STATUS_OK;
}

RDFFStatus
rdff_write_triple(RDFF        file,
                  uint32_t    subject,
                  uint32_t    predicate,
                  uint32_t    object_type,
                  uint32_t    object_size,
                  const void* object)
{
	const uint32_t chunk_size = sizeof(RDFFTripleChunk) + object_size;
	WRITE(CHUNK_TRIP,   CHUNK_ID_LEN,        1, file->fd);
	WRITE(&chunk_size,  sizeof(chunk_size),  1, file->fd);
	WRITE(&subject,     sizeof(subject),     1, file->fd);
	WRITE(&predicate,   sizeof(predicate),   1, file->fd);
	WRITE(&object_type, sizeof(object_type), 1, file->fd);
	WRITE(&object_size, sizeof(object_size), 1, file->fd);
	WRITE(object,       object_size,         1, file->fd);
	if ((object_size % 2)) {
		WRITE("", 1, 1, file->fd);  /* write pad */
	}
	file->size += 8 + chunk_size;
	return RDFF_STATUS_OK;
}

RDFFStatus
rdff_read_chunk(RDFF        file,
                RDFFChunk** buf)
{
	if (feof(file->fd))
		return RDFF_STATUS_EOF;

#define READ(ptr, size, nmemb, stream) \
	if (fread(ptr, size, nmemb, stream) != nmemb) { \
		return RDFF_STATUS_CORRUPT; \
	}

	const uint32_t alloc_size = (*buf)->size;

	READ((*buf)->type,  sizeof((*buf)->type), 1, file->fd);
	READ(&(*buf)->size, sizeof((*buf)->size), 1, file->fd);
	if ((*buf)->size > alloc_size) {
		*buf = realloc(*buf, sizeof(RDFFChunk) + (*buf)->size);
	}
	READ((*buf)->data, (*buf)->size, 1, file->fd);
	if (((*buf)->size % 2)) {
		char pad;
		READ(&pad, 1, 1, file->fd);  /* skip pad */
	}
	return RDFF_STATUS_OK;
}

bool
rdff_chunk_is_uri(RDFFChunk* chunk)

{
	return !strncmp(chunk->type, CHUNK_URID, CHUNK_ID_LEN);
}

bool
rdff_chunk_is_triple(RDFFChunk* chunk)
{
	return !strncmp(chunk->type, CHUNK_TRIP, CHUNK_ID_LEN);
}

void
rdff_close(RDFF file)
{
	if (file) {
		if (file->write) {
			fseek(file->fd, 4, SEEK_SET);
			if (fwrite(&file->size, sizeof(file->size), 1, file->fd) != 1) {
				fprintf(stderr, "failed to write RIFF header size\n");
			}
		}
		fclose(file->fd);
	}

	free(file);
}

#ifdef STANDALONE
// Test program
int
main(int argc, char** argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s FILENAME\n", argv[0]);
		return 1;
	}

	const char* const filename = argv[1];

	RDFF file = rdff_open(filename, true);
	if (!file)
		goto fail;

	static const int N_URIS    = 16;
	static const int N_RECORDS = 16;

	char uri[64];
	for (int i = 0; i < N_URIS; ++i) {
		snprintf(uri, sizeof(uri), "http://example.org/uri%02d", i + 1);
		rdff_write_uri(file, i + 1, strlen(uri), uri);
	}

	char val[6];
	for (int i = 0; i < N_RECORDS; ++i) {
		snprintf(val, sizeof(val), "VAL%02d", i);
		rdff_write_triple(file,
		                  0,
		                  rand() % N_URIS,
		                  0,
		                  sizeof(val),
		                  val);
	}

	rdff_close(file);

	file = rdff_open(filename, false);
	if (!file)
		goto fail;

	RDFFChunk* chunk = malloc(sizeof(RDFFChunk));
	chunk->size = 0;
	for (int i = 0; i < N_URIS; ++i) {
		if (rdff_read_chunk(file, &chunk)
		    || strncmp(chunk->type, CHUNK_URID, 4)) {
			fprintf(stderr, "error: expected %s chunk\n", CHUNK_URID);
			goto fail;
		}
		RDFFURIChunk* body = (RDFFURIChunk*)chunk->data;
		printf("URI: %s\n", body->uri);
	}

	for (int i = 0; i < N_RECORDS; ++i) {
		if (rdff_read_chunk(file, &chunk)
		    || strncmp(chunk->type, CHUNK_TRIP, 4)) {
			fprintf(stderr, "error: expected %s chunk\n", CHUNK_TRIP);
			goto fail;
		}
		RDFFTripleChunk* body = (RDFFTripleChunk*)chunk->data;
		printf("KEY %d = %s\n", body->predicate, body->object);
	}

	free(chunk);
	rdff_close(file);

	return 0;

fail:
	rdff_close(file);
	fprintf(stderr, "Test failed\n");
	return 1;
}
#endif // STANDALONE
