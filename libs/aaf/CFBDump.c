/*
 * Copyright (C) 2017-2023 Adrien Gesta-Fline
 *
 * This file is part of libAAF.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "aaf/CFBDump.h"
#include "aaf/LibCFB.h"

#include "aaf/utils.h"

#define debug(...) \
	_dbg (cfbd->dbg, cfbd, DEBUG_SRC_ID_LIB_CFB, VERB_DEBUG, __VA_ARGS__)

#define warning(...) \
	_dbg (cfbd->dbg, cfbd, DEBUG_SRC_ID_LIB_CFB, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	_dbg (cfbd->dbg, cfbd, DEBUG_SRC_ID_LIB_CFB, VERB_ERROR, __VA_ARGS__)

void
cfb_dump_node (CFB_Data* cfbd, cfbNode* node, int print_stream)
{
	if (node == NULL)
		return;

	if (node->_mse == STGTY_INVALID)
		return;

	wchar_t nodeName[CFB_NODE_NAME_SZ];

	cfb_w16towchar (nodeName, node->_ab, node->_cb);

	int         offset = 0;
	struct dbg* dbg    = cfbd->dbg;

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "\n");
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _ab          : %ls\n", nodeName);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _cb          : %u\n", node->_cb);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _mse         : %s\n",
	                                      node->_mse == 0 ? "STGTY_INVALID" : node->_mse == 1 ? "STGTY_STORAGE"
	                                                                      : node->_mse == 2   ? "STGTY_STREAM"
	                                                                      : node->_mse == 3   ? "STGTY_LOCKBYTES"
	                                                                      : node->_mse == 4   ? "STGTY_PROPERTY"
	                                                                      : node->_mse == 5   ? "STGTY_ROOT"
	                                                                                          : "");

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _bflags      : %s\n", node->_bflags == 1 ? "BLACK" : "RED");
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _sidLeftSib  : 0x%08x\n", node->_sidLeftSib);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _sidRightSib : 0x%08x\n", node->_sidRightSib);

	if (node->_mse == STGTY_STORAGE ||
	    node->_mse == STGTY_ROOT) {
		offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _sidChild    : 0x%08x\n", node->_sidChild);
		offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _clsid       : %ls\n", cfb_CLSIDToText (&(node->_clsId)));
		offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _dwUserFlags : 0x%08x (%d)\n", node->_dwUserFlags, node->_dwUserFlags);
	}

	if (node->_mse == STGTY_INVALID) {
		offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _time  (cre) : 0x%08x%08x\n",
		                                      node->_time[0].dwHighDateTime,
		                                      node->_time[0].dwLowDateTime);

		offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _      (mod) : 0x%08x%08x\n",
		                                      node->_time[1].dwHighDateTime,
		                                      node->_time[1].dwLowDateTime);
	}

	if (node->_mse == STGTY_STREAM ||
	    node->_mse == STGTY_ROOT) {
		offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _sectStart   : 0x%08x (%d)\n", node->_sectStart, node->_sectStart);
		offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _ulSizeLow   : 0x%08x (%d)\n", node->_ulSizeLow, node->_ulSizeLow);
		offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " _ulSizeHigh  : 0x%08x (%d)\n", node->_ulSizeHigh, node->_ulSizeHigh);
	}

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "\n\n");

	if (print_stream == 1) {
		cfb_dump_nodeStream (cfbd, node);
	}
}

void
cfb_dump_nodePath (CFB_Data* cfbd, const wchar_t* path, int print_stream)
{
	cfbNode* node = cfb_getNodeByPath (cfbd, path, 0);

	if (node == NULL) {
		error ("cfb_dump_nodePath() : Could not find node at \"%ls\"\n", path);
		return;
	}

	int         offset = 0;
	struct dbg* dbg    = cfbd->dbg;

	cfb_dump_node (cfbd, node, print_stream);

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "\n\n");
}

void
cfb_dump_nodeStream (CFB_Data* cfbd, cfbNode* node)
{
	unsigned char* stream    = NULL;
	uint64_t       stream_sz = 0;

	cfb_getStream (cfbd, node, &stream, &stream_sz);

	if (stream == NULL) {
		return;
	}

	laaf_util_dump_hex (stream, stream_sz, &cfbd->dbg->_dbg_msg, &cfbd->dbg->_dbg_msg_size, 0);

	free (stream);
}

void
cfb_dump_nodePathStream (CFB_Data* cfbd, const wchar_t* path)
{
	cfbNode* node = cfb_getNodeByPath (cfbd, path, 0);

	if (node == NULL) {
		error ("cfb_dump_nodePathStream() : Could not find node at \"%ls\"\n", path);
		return;
	}

	unsigned char* stream    = NULL;
	uint64_t       stream_sz = 0;

	cfb_getStream (cfbd, node, &stream, &stream_sz);

	laaf_util_dump_hex (stream, stream_sz, &cfbd->dbg->_dbg_msg, &cfbd->dbg->_dbg_msg_size, 0);

	free (stream);
}

void
cfb_dump_nodePaths (CFB_Data* cfbd, uint32_t prevPath, char* strArray[], uint32_t* str_i, cfbNode* node)
{
	if (node == NULL) {
		/* the begining of the first function call. */
		node     = &cfbd->nodes[0];
		strArray = calloc (cfbd->nodes_cnt, sizeof (char*));
	}

	uint32_t thisPath = (*str_i);
	wchar_t  nodeName[CFB_NODE_NAME_SZ];

	cfb_w16towchar (nodeName, node->_ab, node->_cb);

	int pathlen = snprintf (NULL, 0, "%s/%ls", strArray[prevPath], nodeName);

	if (pathlen < 0) {
		// TODO error
		return;
	}

	pathlen++;

	strArray[thisPath] = malloc (pathlen);

	snprintf (strArray[thisPath], pathlen, "%s/%ls", strArray[prevPath], nodeName);

	(*str_i)++;

	if ((int32_t)node->_sidChild > 0)
		cfb_dump_nodePaths (cfbd, thisPath, strArray, str_i, &cfbd->nodes[node->_sidChild]);

	if ((int32_t)node->_sidLeftSib > 0)
		cfb_dump_nodePaths (cfbd, prevPath, strArray, str_i, &cfbd->nodes[node->_sidLeftSib]);

	if ((int32_t)node->_sidRightSib > 0)
		cfb_dump_nodePaths (cfbd, prevPath, strArray, str_i, &cfbd->nodes[node->_sidRightSib]);

	/* the end of the first function call, recursion is over. */
	if (node == &cfbd->nodes[0]) {
		int         offset = 0;
		struct dbg* dbg    = cfbd->dbg;

		/* commented out because output is proper this way... why did we call qsort() in the first place ?! */
		// qsort( strArray, *str_i, sizeof(char*), compareStrings );

		for (uint32_t i = 0; i < cfbd->nodes_cnt && strArray[i] != NULL; i++) {
			offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "%05i : %s\n", i, strArray[i]);
			free (strArray[i]);
		}

		free (strArray);

		offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "\n\n");
	}
}

void
cfb_dump_header (CFB_Data* cfbd)
{
	cfbHeader* cfbh = cfbd->hdr;

	int         offset = 0;
	struct dbg* dbg    = cfbd->dbg;

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_abSig              : 0x%08" PRIx64 "\n", cfbh->_abSig);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_clsId              : %ls\n", cfb_CLSIDToText (&(cfbh->_clsid)));
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " version            : %u.%u ( 0x%04x 0x%04x )\n",
	                                      cfbh->_uMinorVersion, cfbh->_uDllVersion,
	                                      cfbh->_uMinorVersion, cfbh->_uDllVersion);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_uByteOrder         : %s ( 0x%04x )\n",
	                                      cfbh->_uByteOrder == 0xFFFE ? "little-endian" : cfbh->_uByteOrder == 0xFEFF ? "big-endian"
	                                                                                                                  : "?",
	                                      cfbh->_uByteOrder);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_uSectorShift       : %u (%u bytes sectors)\n",
	                                      cfbh->_uSectorShift,
	                                      1 << cfbh->_uSectorShift);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_uMiniSectorShift   : %u (%u bytes mini-sectors)\n",
	                                      cfbh->_uMiniSectorShift,
	                                      1 << cfbh->_uMiniSectorShift);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_usReserved0        : 0x%02x\n", cfbh->_usReserved);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_ulReserved1        : 0x%04x\n", cfbh->_ulReserved1);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_csectDir           : %u\n", cfbh->_csectDir);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_csectFat           : %u\n", cfbh->_csectFat);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_sectDirStart       : %u\n", cfbh->_sectDirStart);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_signature          : %u\n", cfbh->_signature);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_ulMiniSectorCutoff : %u\n", cfbh->_ulMiniSectorCutoff);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_sectMiniFatStart   : %u\n", cfbh->_sectMiniFatStart);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_csectMiniFat       : %u\n", cfbh->_csectMiniFat);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_sectDifStart       : %u\n", cfbh->_sectDifStart);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_csectDif           : %u\n", cfbh->_csectDif);

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "\n");
}

void
cfb_dump_FAT (CFB_Data* cfbd)
{
	int         offset = 0;
	struct dbg* dbg    = cfbd->dbg;

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_CFB_FAT_______________________________________________________________________________________\n\n");

	uint32_t i = 0;

	for (i = 0; i < cfbd->fat_sz; i++) {
		offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " SECT[%u] : 0x%08x %s\n",
		                                      i,
		                                      cfbd->fat[i],
		                                      (cfbd->fat[i] == CFB_MAX_REG_SECT) ? "(CFB_MAX_REG_SECT)" : (cfbd->fat[i] == CFB_DIFAT_SECT) ? "(CFB_DIFAT_SECT)"
		                                                                                              : (cfbd->fat[i] == CFB_FAT_SECT)     ? "(CFB_FAT_SECT)"
		                                                                                              : (cfbd->fat[i] == CFB_END_OF_CHAIN) ? "(CFB_END_OF_CHAIN)"
		                                                                                              : (cfbd->fat[i] == CFB_FREE_SECT)    ? "(CFB_FREE_SECT)"
		                                                                                                                                   : "");
	}

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "\n");

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " End of FAT.\n\n");

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " Total FAT entries   : %u\n", cfbd->fat_sz);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " Count of FAT sector : %u\n", cfbd->hdr->_csectFat);

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "\n\n");
}

void
cfb_dump_MiniFAT (CFB_Data* cfbd)
{
	int         offset = 0;
	struct dbg* dbg    = cfbd->dbg;

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_CFB_MiniFAT___________________________________________________________________________________\n\n");

	uint32_t i = 0;

	for (i = 0; i < cfbd->miniFat_sz; i++) {
		offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " SECT[%u] : 0x%08x %s\n",
		                                      i,
		                                      cfbd->miniFat[i],
		                                      (cfbd->miniFat[i] == CFB_MAX_REG_SECT) ? "(CFB_MAX_REG_SECT)" : (cfbd->miniFat[i] == CFB_DIFAT_SECT) ? "(CFB_DIFAT_SECT)"
		                                                                                                  : (cfbd->miniFat[i] == CFB_FAT_SECT)     ? "(CFB_FAT_SECT)"
		                                                                                                  : (cfbd->miniFat[i] == CFB_END_OF_CHAIN) ? "(CFB_END_OF_CHAIN)"
		                                                                                                  : (cfbd->miniFat[i] == CFB_FREE_SECT)    ? "(CFB_FREE_SECT)"
		                                                                                                                                           : "");
	}

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "\n");

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " End of MiniFAT.\n\n");

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " Total MiniFAT entries   : %u\n", cfbd->miniFat_sz);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " First MiniFAT sector ID : %u\n", cfbd->hdr->_sectMiniFatStart);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " Count of MiniFAT sector : %u\n", cfbd->hdr->_csectMiniFat);

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "\n\n");
}

void
cfb_dump_DiFAT (CFB_Data* cfbd)
{
	int         offset = 0;
	struct dbg* dbg    = cfbd->dbg;

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "_CFB_DiFAT_____________________________________________________________________________________\n\n");

	uint32_t i = 0;

	for (i = 0; i < cfbd->DiFAT_sz; i++) {
		offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " SECT[%u] : 0x%08x %s\n",
		                                      i,
		                                      cfbd->DiFAT[i],
		                                      (cfbd->DiFAT[i] == CFB_MAX_REG_SECT) ? "(CFB_MAX_REG_SECT)" : (cfbd->DiFAT[i] == CFB_DIFAT_SECT) ? "(CFB_DIFAT_SECT)"
		                                                                                                : (cfbd->DiFAT[i] == CFB_FAT_SECT)     ? "(CFB_FAT_SECT)"
		                                                                                                : (cfbd->DiFAT[i] == CFB_END_OF_CHAIN) ? "(CFB_END_OF_CHAIN)"
		                                                                                                : (cfbd->DiFAT[i] == CFB_FREE_SECT)    ? "(CFB_FREE_SECT)"
		                                                                                                                                       : "");
	}

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "\n");

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " End of DiFAT.\n\n");

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " Total DiFAT entries   : %u\n", cfbd->DiFAT_sz);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " First DiFAT sector ID : %u\n", cfbd->hdr->_sectDifStart);
	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, " Count of DiFAT sector : Header + %u\n", cfbd->hdr->_csectDif);

	offset += laaf_util_snprintf_realloc (&dbg->_dbg_msg, &dbg->_dbg_msg_size, offset, "\n\n");
}
