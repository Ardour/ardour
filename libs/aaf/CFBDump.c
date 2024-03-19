/*
 * Copyright (C) 2017-2024 Adrien Gesta-Fline
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aaf/CFBDump.h"
#include "aaf/LibCFB.h"

#include "aaf/utils.h"

#define debug(...) \
	AAF_LOG (cfbd->log, cfbd, LOG_SRC_ID_LIB_CFB, VERB_DEBUG, __VA_ARGS__)

#define warning(...) \
	AAF_LOG (cfbd->log, cfbd, LOG_SRC_ID_LIB_CFB, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	AAF_LOG (cfbd->log, cfbd, LOG_SRC_ID_LIB_CFB, VERB_ERROR, __VA_ARGS__)

void
cfb_dump_node (CFB_Data* cfbd, cfbNode* node, int print_stream, const char* padding)
{
	if (node == NULL)
		return;

	if (node->_mse == STGTY_INVALID)
		return;

	char* nodeName = cfb_w16toUTF8 (node->_ab, node->_cb);

	struct aafLog* log = cfbd->log;

	LOG_BUFFER_WRITE (log, "\n");
	LOG_BUFFER_WRITE (log, "%s_ab          : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), nodeName, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_cb          : %s%u%s\n", padding, ANSI_COLOR_DARKGREY (log), node->_cb, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_mse         : %s%s%s\n",
	                  padding,
	                  ANSI_COLOR_DARKGREY (log),
	                  node->_mse == 0 ? "STGTY_INVALID" : node->_mse == 1 ? "STGTY_STORAGE"
	                                                  : node->_mse == 2   ? "STGTY_STREAM"
	                                                  : node->_mse == 3   ? "STGTY_LOCKBYTES"
	                                                  : node->_mse == 4   ? "STGTY_PROPERTY"
	                                                  : node->_mse == 5   ? "STGTY_ROOT"
	                                                                      : "",
	                  ANSI_COLOR_RESET (log));

	LOG_BUFFER_WRITE (log, "%s_bflags      : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), node->_bflags == 1 ? "BLACK" : "RED", ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_sidLeftSib  : %s0x%08x%s\n", padding, ANSI_COLOR_DARKGREY (log), node->_sidLeftSib, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_sidRightSib : %s0x%08x%s\n", padding, ANSI_COLOR_DARKGREY (log), node->_sidRightSib, ANSI_COLOR_RESET (log));

	if (node->_mse == STGTY_STORAGE ||
	    node->_mse == STGTY_ROOT) {
		LOG_BUFFER_WRITE (log, "%s_sidChild    : %s0x%08x%s\n", padding, ANSI_COLOR_DARKGREY (log), node->_sidChild, ANSI_COLOR_RESET (log));
		LOG_BUFFER_WRITE (log, "%s_clsid       : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), cfb_CLSIDToText (&(node->_clsId)), ANSI_COLOR_RESET (log));
		LOG_BUFFER_WRITE (log, "%s_dwUserFlags : %s0x%08x (%d)%s\n", padding, ANSI_COLOR_DARKGREY (log), node->_dwUserFlags, node->_dwUserFlags, ANSI_COLOR_RESET (log));
	}

	if (node->_mse == STGTY_INVALID) {
		LOG_BUFFER_WRITE (log, "%s_time  (cre) : %s0x%08x%08x%s\n",
		                  padding,
		                  ANSI_COLOR_DARKGREY (log),
		                  node->_time[0].dwHighDateTime,
		                  node->_time[0].dwLowDateTime,
		                  ANSI_COLOR_RESET (log));

		LOG_BUFFER_WRITE (log, "%s_      (mod) : %s0x%08x%08x%s\n",
		                  padding,
		                  ANSI_COLOR_DARKGREY (log),
		                  node->_time[1].dwHighDateTime,
		                  node->_time[1].dwLowDateTime,
		                  ANSI_COLOR_RESET (log));
	}

	if (node->_mse == STGTY_STREAM ||
	    node->_mse == STGTY_ROOT) {
		LOG_BUFFER_WRITE (log, "%s_sectStart   : %s0x%08x (%d)%s\n", padding, ANSI_COLOR_DARKGREY (log), node->_sectStart, node->_sectStart, ANSI_COLOR_RESET (log));
		LOG_BUFFER_WRITE (log, "%s_ulSizeLow   : %s0x%08x (%d)%s\n", padding, ANSI_COLOR_DARKGREY (log), node->_ulSizeLow, node->_ulSizeLow, ANSI_COLOR_RESET (log));
		LOG_BUFFER_WRITE (log, "%s_ulSizeHigh  : %s0x%08x (%d)%s\n", padding, ANSI_COLOR_DARKGREY (log), node->_ulSizeHigh, node->_ulSizeHigh, ANSI_COLOR_RESET (log));
	}

	LOG_BUFFER_WRITE (log, "\n\n");

	log->log_callback (log, (void*)cfbd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);

	if (print_stream == 1) {
		cfb_dump_nodeStream (cfbd, node, "");
	}

	free (nodeName);
}

void
cfb_dump_nodePath (CFB_Data* cfbd, const char* path, int print_stream, const char* padding)
{
	cfbNode* node = cfb_getNodeByPath (cfbd, path, 0);

	if (node == NULL) {
		error ("cfb_dump_nodePath() : Could not find node at \"%s\"\n", path);
		return;
	}

	cfb_dump_node (cfbd, node, print_stream, padding);
}

void
cfb_dump_nodeStream (CFB_Data* cfbd, cfbNode* node, const char* padding)
{
	struct aafLog* log = cfbd->log;

	unsigned char* stream    = NULL;
	uint64_t       stream_sz = 0;

	cfb_getStream (cfbd, node, &stream, &stream_sz);

	if (stream == NULL) {
		return;
	}

	laaf_util_dump_hex (stream, stream_sz, &log->_msg, &log->_msg_size, log->_msg_pos, padding);

	log->log_callback (log, (void*)cfbd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);

	free (stream);
}

void
cfb_dump_nodePathStream (CFB_Data* cfbd, const char* path, const char* padding)
{
	struct aafLog* log = cfbd->log;

	cfbNode* node = cfb_getNodeByPath (cfbd, path, 0);

	if (node == NULL) {
		error ("Could not find node at \"%s\"\n", path);
		return;
	}

	unsigned char* stream    = NULL;
	uint64_t       stream_sz = 0;

	cfb_getStream (cfbd, node, &stream, &stream_sz);

	laaf_util_dump_hex (stream, stream_sz, &log->_msg, &log->_msg_size, log->_msg_pos, padding);

	log->log_callback (log, (void*)cfbd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);

	free (stream);
}

void
cfb_dump_nodePaths (CFB_Data* cfbd, uint32_t prevPath, char* strArray[], uint32_t* str_i, cfbNode* node, const char* padding, int firstIteration)
{
	struct aafLog* log = cfbd->log;

	/* initial function call */
	if (firstIteration) {
		node = &cfbd->nodes[0];

		if (!node) {
			return;
		}

		strArray = calloc (cfbd->nodes_cnt, sizeof (char*));

		if (!strArray) {
			error ("Out of memory");
			return;
		}
	}

	uint32_t thisPath = (*str_i);

	char* nodeName = cfb_w16toUTF8 (node->_ab, node->_cb);

	laaf_util_snprintf_realloc (&strArray[thisPath], 0, 0, "%s/%s", strArray[prevPath], nodeName);

	free (nodeName);

	(*str_i)++;

	if ((int32_t)node->_sidChild > 0)
		cfb_dump_nodePaths (cfbd, thisPath, strArray, str_i, &cfbd->nodes[node->_sidChild], padding, 0);

	if ((int32_t)node->_sidLeftSib > 0)
		cfb_dump_nodePaths (cfbd, prevPath, strArray, str_i, &cfbd->nodes[node->_sidLeftSib], padding, 0);

	if ((int32_t)node->_sidRightSib > 0)
		cfb_dump_nodePaths (cfbd, prevPath, strArray, str_i, &cfbd->nodes[node->_sidRightSib], padding, 0);

	/* the end of the first function call, recursion is over. */
	if (firstIteration) {
		/* commented out because output seems proper this way... why did we call qsort() in the first place ?! */
		// qsort( strArray, *str_i, sizeof(char*), compareStrings );

		for (uint32_t i = 0; i < cfbd->nodes_cnt && strArray[i] != NULL; i++) {
			LOG_BUFFER_WRITE (log, "%s%0*i : %s%s%s\n",
			                  padding,
			                  (cfbd->nodes_cnt > 1000000) ? 7 : (cfbd->nodes_cnt > 100000) ? 6
			                                                : (cfbd->nodes_cnt > 10000)    ? 5
			                                                : (cfbd->nodes_cnt > 1000)     ? 4
			                                                : (cfbd->nodes_cnt > 100)      ? 3
			                                                : (cfbd->nodes_cnt > 10)       ? 2
			                                                                               : 1,
			                  i,
			                  ANSI_COLOR_DARKGREY (log),
			                  strArray[i],
			                  ANSI_COLOR_RESET (log));
			free (strArray[i]);
		}

		free (strArray);

		LOG_BUFFER_WRITE (log, "\n\n");

		log->log_callback (log, (void*)cfbd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);
	}
}

void
cfb_dump_header (CFB_Data* cfbd, const char* padding)
{
	struct aafLog* log = cfbd->log;

	cfbHeader* cfbh = cfbd->hdr;

	LOG_BUFFER_WRITE (log, "%s_abSig              : %s0x%08" PRIx64 "%s\n", padding, ANSI_COLOR_DARKGREY (log), cfbh->_abSig, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_clsId              : %s%s%s\n", padding, ANSI_COLOR_DARKGREY (log), cfb_CLSIDToText (&(cfbh->_clsid)), ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_version            : %s%u.%u ( 0x%04x 0x%04x )%s\n",
	                  padding,
	                  ANSI_COLOR_DARKGREY (log),
	                  cfbh->_uMinorVersion, cfbh->_uDllVersion,
	                  cfbh->_uMinorVersion, cfbh->_uDllVersion,
	                  ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_uByteOrder         : %s%s ( 0x%04x )%s\n",
	                  padding,
	                  ANSI_COLOR_DARKGREY (log),
	                  cfbh->_uByteOrder == 0xFFFE ? "little-endian" : cfbh->_uByteOrder == 0xFEFF ? "big-endian"
	                                                                                              : "?",
	                  cfbh->_uByteOrder,
	                  ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_uSectorShift       : %s%u (%u bytes sectors)%s\n",
	                  padding,
	                  ANSI_COLOR_DARKGREY (log),
	                  cfbh->_uSectorShift,
	                  1 << cfbh->_uSectorShift,
	                  ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_uMiniSectorShift   : %s%u (%u bytes mini-sectors)%s\n",
	                  padding,
	                  ANSI_COLOR_DARKGREY (log),
	                  cfbh->_uMiniSectorShift,
	                  1 << cfbh->_uMiniSectorShift,
	                  ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_usReserved0        : %s0x%02x%s\n", padding, ANSI_COLOR_DARKGREY (log), cfbh->_usReserved, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_ulReserved1        : %s0x%04x%s\n", padding, ANSI_COLOR_DARKGREY (log), cfbh->_ulReserved1, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_csectDir           : %s%u%s\n", padding, ANSI_COLOR_DARKGREY (log), cfbh->_csectDir, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_csectFat           : %s%u%s\n", padding, ANSI_COLOR_DARKGREY (log), cfbh->_csectFat, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_sectDirStart       : %s%u%s\n", padding, ANSI_COLOR_DARKGREY (log), cfbh->_sectDirStart, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_signature          : %s%u%s\n", padding, ANSI_COLOR_DARKGREY (log), cfbh->_signature, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_ulMiniSectorCutoff : %s%u%s\n", padding, ANSI_COLOR_DARKGREY (log), cfbh->_ulMiniSectorCutoff, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_sectMiniFatStart   : %s%u%s\n", padding, ANSI_COLOR_DARKGREY (log), cfbh->_sectMiniFatStart, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_csectMiniFat       : %s%u%s\n", padding, ANSI_COLOR_DARKGREY (log), cfbh->_csectMiniFat, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_sectDifStart       : %s%u%s\n", padding, ANSI_COLOR_DARKGREY (log), cfbh->_sectDifStart, ANSI_COLOR_RESET (log));
	LOG_BUFFER_WRITE (log, "%s_csectDif           : %s%u%s\n", padding, ANSI_COLOR_DARKGREY (log), cfbh->_csectDif, ANSI_COLOR_RESET (log));

	LOG_BUFFER_WRITE (log, "\n");

	log->log_callback (log, (void*)cfbd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);
}

void
cfb_dump_FAT (CFB_Data* cfbd, const char* padding)
{
	struct aafLog* log = cfbd->log;

	LOG_BUFFER_WRITE (log, "_CFB_FAT_______________________________________________________________________________________\n\n");

	uint32_t i = 0;

	for (i = 0; i < cfbd->fat_sz; i++) {
		LOG_BUFFER_WRITE (log, "%sSECT[%s%0*u%s] : %s0x%08x %s%s\n",
		                  padding,
		                  ANSI_COLOR_DARKGREY (log),
		                  (cfbd->fat_sz > 1000000) ? 7 : (cfbd->fat_sz > 100000) ? 6
		                                             : (cfbd->fat_sz > 10000)    ? 5
		                                             : (cfbd->fat_sz > 1000)     ? 4
		                                             : (cfbd->fat_sz > 100)      ? 3
		                                             : (cfbd->fat_sz > 10)       ? 2
		                                                                         : 1,
		                  i,
		                  ANSI_COLOR_RESET (log),

		                  ANSI_COLOR_DARKGREY (log),
		                  cfbd->fat[i],
		                  (cfbd->fat[i] == CFB_MAX_REG_SECT) ? "(CFB_MAX_REG_SECT)" : (cfbd->fat[i] == CFB_DIFAT_SECT) ? "(CFB_DIFAT_SECT)"
		                                                                          : (cfbd->fat[i] == CFB_FAT_SECT)     ? "(CFB_FAT_SECT)"
		                                                                          : (cfbd->fat[i] == CFB_END_OF_CHAIN) ? "(CFB_END_OF_CHAIN)"
		                                                                          : (cfbd->fat[i] == CFB_FREE_SECT)    ? "(CFB_FREE_SECT)"
		                                                                                                               : "",
		                  ANSI_COLOR_RESET (log));
	}

	LOG_BUFFER_WRITE (log, "\n");

	LOG_BUFFER_WRITE (log, "%sEnd of FAT.\n\n", padding);

	LOG_BUFFER_WRITE (log, "%sTotal FAT entries   : %u\n", padding, cfbd->fat_sz);
	LOG_BUFFER_WRITE (log, "%sCount of FAT sector : %u\n", padding, cfbd->hdr->_csectFat);

	LOG_BUFFER_WRITE (log, "\n\n");

	log->log_callback (log, (void*)cfbd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);
}

void
cfb_dump_MiniFAT (CFB_Data* cfbd, const char* padding)
{
	struct aafLog* log = cfbd->log;

	LOG_BUFFER_WRITE (log, "_CFB_MiniFAT___________________________________________________________________________________\n\n");

	uint32_t i = 0;

	for (i = 0; i < cfbd->miniFat_sz; i++) {
		LOG_BUFFER_WRITE (log, "%sSECT[%s%0*u%s] : %s0x%08x %s%s\n",
		                  padding,
		                  ANSI_COLOR_DARKGREY (log),
		                  (cfbd->miniFat_sz > 1000000) ? 7 : (cfbd->miniFat_sz > 100000) ? 6
		                                                 : (cfbd->miniFat_sz > 10000)    ? 5
		                                                 : (cfbd->miniFat_sz > 1000)     ? 4
		                                                 : (cfbd->miniFat_sz > 100)      ? 3
		                                                 : (cfbd->miniFat_sz > 10)       ? 2
		                                                                                 : 1,
		                  i,
		                  ANSI_COLOR_RESET (log),

		                  ANSI_COLOR_DARKGREY (log),
		                  cfbd->miniFat[i],
		                  (cfbd->miniFat[i] == CFB_MAX_REG_SECT) ? "(CFB_MAX_REG_SECT)" : (cfbd->miniFat[i] == CFB_DIFAT_SECT) ? "(CFB_DIFAT_SECT)"
		                                                                              : (cfbd->miniFat[i] == CFB_FAT_SECT)     ? "(CFB_FAT_SECT)"
		                                                                              : (cfbd->miniFat[i] == CFB_END_OF_CHAIN) ? "(CFB_END_OF_CHAIN)"
		                                                                              : (cfbd->miniFat[i] == CFB_FREE_SECT)    ? "(CFB_FREE_SECT)"
		                                                                                                                       : "",
		                  ANSI_COLOR_RESET (log));
	}

	LOG_BUFFER_WRITE (log, "\n");

	LOG_BUFFER_WRITE (log, "%sEnd of MiniFAT.\n\n", padding);

	LOG_BUFFER_WRITE (log, "%sTotal MiniFAT entries   : %u\n", padding, cfbd->miniFat_sz);
	LOG_BUFFER_WRITE (log, "%sFirst MiniFAT sector ID : %u\n", padding, cfbd->hdr->_sectMiniFatStart);
	LOG_BUFFER_WRITE (log, "%sCount of MiniFAT sector : %u\n", padding, cfbd->hdr->_csectMiniFat);

	LOG_BUFFER_WRITE (log, "\n\n");

	log->log_callback (log, (void*)cfbd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);
}

void
cfb_dump_DiFAT (CFB_Data* cfbd, const char* padding)
{
	struct aafLog* log = cfbd->log;

	LOG_BUFFER_WRITE (log, "_CFB_DiFAT_____________________________________________________________________________________\n\n");

	uint32_t i = 0;

	for (i = 0; i < cfbd->DiFAT_sz; i++) {
		LOG_BUFFER_WRITE (log, "%sSECT[%s%0*u%s] : %s0x%08x %s%s\n",
		                  padding,
		                  ANSI_COLOR_DARKGREY (log),
		                  (cfbd->miniFat_sz > 1000000) ? 7 : (cfbd->miniFat_sz > 100000) ? 6
		                                                 : (cfbd->miniFat_sz > 10000)    ? 5
		                                                 : (cfbd->miniFat_sz > 1000)     ? 4
		                                                 : (cfbd->miniFat_sz > 100)      ? 3
		                                                 : (cfbd->miniFat_sz > 10)       ? 2
		                                                                                 : 1,
		                  i,
		                  ANSI_COLOR_RESET (log),

		                  ANSI_COLOR_DARKGREY (log),
		                  cfbd->DiFAT[i],
		                  (cfbd->DiFAT[i] == CFB_MAX_REG_SECT) ? "(CFB_MAX_REG_SECT)" : (cfbd->DiFAT[i] == CFB_DIFAT_SECT) ? "(CFB_DIFAT_SECT)"
		                                                                            : (cfbd->DiFAT[i] == CFB_FAT_SECT)     ? "(CFB_FAT_SECT)"
		                                                                            : (cfbd->DiFAT[i] == CFB_END_OF_CHAIN) ? "(CFB_END_OF_CHAIN)"
		                                                                            : (cfbd->DiFAT[i] == CFB_FREE_SECT)    ? "(CFB_FREE_SECT)"
		                                                                                                                   : "",
		                  ANSI_COLOR_RESET (log));
	}

	LOG_BUFFER_WRITE (log, "\n");

	LOG_BUFFER_WRITE (log, "%sEnd of DiFAT.\n\n", padding);

	LOG_BUFFER_WRITE (log, "%sTotal DiFAT entries   : %u\n", padding, cfbd->DiFAT_sz);
	LOG_BUFFER_WRITE (log, "%sFirst DiFAT sector ID : %u\n", padding, cfbd->hdr->_sectDifStart);
	LOG_BUFFER_WRITE (log, "%sCount of DiFAT sector : Header + %u\n", padding, cfbd->hdr->_csectDif);

	LOG_BUFFER_WRITE (log, "\n\n");

	log->log_callback (log, (void*)cfbd, LOG_SRC_ID_DUMP, 0, "", "", 0, log->_msg, log->user);
}
