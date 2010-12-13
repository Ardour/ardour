/* Portable file-based implementation of LV2 Persist.
 * See <http://lv2plug.in/ns/ext/persist> for details.
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

#include <stdbool.h>
#include <stdint.h>

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

/** Open/Create a new persist file. */
LV2PFile
lv2_pfile_open(const char* path, bool write);

/** Write a record to a persist file. */
LV2PFileStatus
lv2_pfile_write(LV2PFile    file,
                const char* key,
                const void* value,
                uint64_t    size,
                const char* type);

/** Read a record from a persist file.
 * @a key and @a value are allocated with malloc and must be freed by caller.
 */
LV2PFileStatus
lv2_pfile_read(LV2PFile  file,
               char**    key,
               uint32_t* key_len,
               char**    type,
               uint32_t* type_len,
               void**    value,
               uint64_t* size);

/** Close a persist file.
 * After this call, @a file is invalid.
 */
void
lv2_pfile_close(LV2PFile file);

#ifdef __cplusplus
} /* extern "C" */
#endif
