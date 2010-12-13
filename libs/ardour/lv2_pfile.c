/* Portable file-based implementation of LV2 Persist.
 * See <http://lv2plug.in/ns/ext/persist> for details.
 * Copyright (C) 2010 David Robillard <http://drobilla.net>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this file; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv2_pfile.h"

struct _LV2PFile {
	FILE* fd;
};

LV2PFile
lv2_pfile_open(const char* path, bool write)
{
	FILE* fd = fopen(path, (write ? "w" : "r"));
	if (!fd) {
		fprintf(stderr, strerror(errno));
		return NULL;
	}

	static const char* const magic     = "LV2PFILE";
	static const size_t      magic_len = 8;
	if (write) {
		fwrite(magic, magic_len, 1, fd);
	} else {
		char file_magic[magic_len];
		if (fread(file_magic, magic_len, 1, fd) != 1
		    || strncmp(file_magic, magic, magic_len)) {
			fclose(fd);
			return NULL;
		}
	}

	LV2PFile ret = (LV2PFile)malloc(sizeof(LV2PFile));
	ret->fd = fd;
	return ret;
}

LV2PFileStatus
lv2_pfile_write(LV2PFile    file,
                const char* key,
                const void* value,
                uint64_t    size,
                const char* type)
{
#define WRITE(ptr, size, nmemb, stream) \
	if (fwrite(ptr, size, nmemb, stream) != nmemb) { \
		return LV2_PFILE_UNKNOWN_ERROR; \
	}

	const uint32_t key_len = strlen(key);
	WRITE(&key_len, sizeof(key_len), 1, file->fd);
	WRITE(key, key_len + 1, 1, file->fd);

	const uint32_t type_len = strlen(type);
	WRITE(&type_len, sizeof(type_len), 1, file->fd);
	WRITE(type, type_len + 1, 1, file->fd);

	WRITE(&size, sizeof(size), 1, file->fd);
	WRITE(value, size, 1, file->fd);

	return LV2_PFILE_OK;
}

LV2PFileStatus
lv2_pfile_read(LV2PFile  file,
               char**    key,
               uint32_t* key_len,
               char**    type,
               uint32_t* type_len,
               void**    value,
               uint64_t* size)
{
	if (feof(file->fd))
		return LV2_PFILE_EOF;

#define READ(ptr, size, nmemb, stream) \
	if (fread(ptr, size, nmemb, stream) != nmemb) { \
		assert(false); \
		return LV2_PFILE_CORRUPT; \
	}

	READ(key_len, sizeof(*key_len), 1, file->fd);
	*key = (char*)malloc(*key_len + 1);
	READ(*key, *key_len + 1, 1, file->fd);

	READ(type_len, sizeof(*type_len), 1, file->fd);
	*type = (char*)malloc(*type_len + 1);
	READ(*type, *type_len + 1, 1, file->fd);

	READ(size, sizeof(*size), 1, file->fd);
	*value = malloc(*size);
	READ(*value, *size, 1, file->fd);

	return LV2_PFILE_OK;
}

void
lv2_pfile_close(LV2PFile file)
{
	if (file)
		fclose(file->fd);

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

	char wkey[6];
	char wval[6];
	const char* wtype = "http://example.org/type";
#define NUM_RECORDS 32
	for (int i = 0; i < NUM_RECORDS; ++i) {
		snprintf(wkey, sizeof(wkey), "KEY%02d", i);
		snprintf(wval, sizeof(wval), "VAL%02d", i);
		lv2_pfile_write(file, wkey, wval, strlen(wval) + 1, wtype);
	}

	lv2_pfile_close(file);

	file = lv2_pfile_open(filename, false);
	if (!file)
		goto fail;

	char*    rkey;
	uint32_t rkey_len;
	char*    rtype;
	uint32_t rtype_len;
	uint64_t rsize;
	void*    rval;
	for (int i = 0; i < NUM_RECORDS; ++i) {
		if (lv2_pfile_read(file, &rkey, &rkey_len, &rtype, &rtype_len, &rval, &rsize))
			goto fail;

		printf("%s = %s :: %s\n", rkey, (char*)rval, rtype);
		free(rkey);
		free(rtype);
		free(rval);
	}

	lv2_pfile_close(file);
	return 0;

fail:
	lv2_pfile_close(file);
	fprintf(stderr, "Test failed\n");
	return 1;
}
#endif // STANDALONE
