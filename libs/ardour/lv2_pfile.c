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
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this sofware; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  02110-1301 USA.
*/

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv2_pfile.h"

#define CHUNK_ID_LEN 4

static const char FILE_TYPE[CHUNK_ID_LEN]  = "LV2F";  /* LV2 RIFF File */ 
static const char CHUNK_KVAL[CHUNK_ID_LEN] = "KVAL";  /* Key/Value Chunk */
static const char CHUNK_URID[CHUNK_ID_LEN] = "URID";  /* URI ID Chunk */

struct _LV2PFile {
	FILE*    fd;
	uint32_t size;
	bool     write;
};

LV2PFile
lv2_pfile_open(const char* path, bool write)
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
		fwrite(FILE_TYPE, CHUNK_ID_LEN, 1, fd); /* LV2 RIFF file type */
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
			fprintf(stderr, "%s: error: not an LV2 RIFF file\n", path);
			return NULL;
		}
	}

	LV2PFile ret = (LV2PFile)malloc(sizeof(struct _LV2PFile));
	ret->fd    = fd;
	ret->size  = size;
	ret->write = write;
	return ret;
}

#define WRITE(ptr, size, nmemb, stream) \
	if (fwrite(ptr, size, nmemb, stream) != nmemb) { \
		return LV2_PFILE_UNKNOWN_ERROR; \
	}

LV2PFileStatus
lv2_pfile_write_uri(LV2PFile    file,
                    uint32_t    id,
                    const char* uri,
                    uint32_t    len)
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
	return LV2_PFILE_OK;
}

LV2PFileStatus
lv2_pfile_write_value(LV2PFile    file,
                      uint32_t    key,
                      const void* value,
                      uint32_t    size,
                      uint32_t    type)
{
	const uint32_t chunk_size = sizeof(key) + sizeof(type) + sizeof(size) + size;
	WRITE(CHUNK_KVAL,  CHUNK_ID_LEN,       1, file->fd);
	WRITE(&chunk_size, sizeof(chunk_size), 1, file->fd);
	WRITE(&key,        sizeof(key),        1, file->fd);
	WRITE(&type,       sizeof(type),       1, file->fd);
	WRITE(&size,       sizeof(size),       1, file->fd);
	WRITE(value,       size,               1, file->fd);
	if ((size % 2)) {
		WRITE("", 1, 1, file->fd);  /* write pad */
	}
	file->size += 8 + chunk_size;
	return LV2_PFILE_OK;
}

LV2PFileStatus
lv2_pfile_read_chunk(LV2PFile              file,
                     LV2PFileChunkHeader** buf)
{
	if (feof(file->fd))
		return LV2_PFILE_EOF;

#define READ(ptr, size, nmemb, stream) \
	if (fread(ptr, size, nmemb, stream) != nmemb) { \
		return LV2_PFILE_CORRUPT; \
	}

	const uint32_t alloc_size = (*buf)->size;

	READ((*buf)->type,  sizeof((*buf)->type), 1, file->fd);
	READ(&(*buf)->size, sizeof((*buf)->size), 1, file->fd);
	if ((*buf)->size > alloc_size) {
		*buf = realloc(*buf, sizeof(LV2PFileChunkHeader) + (*buf)->size);
	}
	READ((*buf)->data, (*buf)->size, 1, file->fd);
	if (((*buf)->size % 2)) {
		char pad;
		READ(&pad, 1, 1, file->fd);  /* skip pad */
	}
	return LV2_PFILE_OK;
}

void
lv2_pfile_close(LV2PFile file)
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

	LV2PFile file = lv2_pfile_open(filename, true);
	if (!file)
		goto fail;

	static const int N_URIS    = 16;
	static const int N_RECORDS = 16;

	char uri[64];
	for (int i = 0; i < N_URIS; ++i) {
		snprintf(uri, sizeof(uri), "http://example.org/uri%02d", i + 1);
		lv2_pfile_write_uri(file, i + 1, uri, strlen(uri) + 1);
	}
	
	char val[6];
	for (int i = 0; i < N_RECORDS; ++i) {
		snprintf(val, sizeof(val), "VAL%02d", i);
		lv2_pfile_write_value(file,
		                      rand() % N_URIS,
		                      val,
		                      sizeof(val),
		                      0);
	}

	lv2_pfile_close(file);

	file = lv2_pfile_open(filename, false);
	if (!file)
		goto fail;

	LV2PFileChunkHeader* chunk = malloc(sizeof(LV2PFileChunkHeader));
	chunk->size = 0;
	for (int i = 0; i < N_URIS; ++i) {
		if (lv2_pfile_read_chunk(file, &chunk)
		    || strncmp(chunk->type, "URID", 4)) {
			fprintf(stderr, "error: expected URID chunk\n");
			goto fail;
		}
		LV2PFileURIChunk* body = (LV2PFileURIChunk*)chunk->data;
		printf("URI: %s\n", body->uri);
	}

	for (int i = 0; i < N_RECORDS; ++i) {
		if (lv2_pfile_read_chunk(file, &chunk)
		    || strncmp(chunk->type, "KVAL", 4)) {
			fprintf(stderr, "error: expected KVAL chunk\n");
			goto fail;
		}
		LV2PFileValueChunk* body = (LV2PFileValueChunk*)chunk->data;
		printf("KEY %d = %s\n", body->key, body->value);
	}

	free(chunk);
	lv2_pfile_close(file);

	return 0;

fail:
	lv2_pfile_close(file);
	fprintf(stderr, "Test failed\n");
	return 1;
}
#endif // STANDALONE
