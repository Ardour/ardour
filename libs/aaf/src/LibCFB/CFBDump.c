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

#include <libaaf/CFBDump.h>
#include <libaaf/LibCFB.h>

#include <libaaf/utils.h>

#define debug(...)                                                             \
  _dbg(cfbd->dbg, cfbd, DEBUG_SRC_ID_LIB_CFB, VERB_DEBUG, __VA_ARGS__)

#define warning(...)                                                           \
  _dbg(cfbd->dbg, cfbd, DEBUG_SRC_ID_LIB_CFB, VERB_WARNING, __VA_ARGS__)

#define error(...)                                                             \
  _dbg(cfbd->dbg, cfbd, DEBUG_SRC_ID_LIB_CFB, VERB_ERROR, __VA_ARGS__)

void cfb_dump_node(CFB_Data *cfbd, cfbNode *node, int print_stream) {
  if (node == NULL)
    return;

  if (node->_mse == STGTY_INVALID)
    return;

  wchar_t nodeName[CFB_NODE_NAME_SZ];

  cfb_w16towchar(nodeName, node->_ab, node->_cb);

  struct dbg *dbg = cfbd->dbg;

  DBG_BUFFER_WRITE(dbg, "\n");
  DBG_BUFFER_WRITE(dbg, " _ab          : %ls\n", nodeName);
  DBG_BUFFER_WRITE(dbg, " _cb          : %u\n", node->_cb);
  DBG_BUFFER_WRITE(dbg, " _mse         : %s\n",
                   node->_mse == 0   ? "STGTY_INVALID"
                   : node->_mse == 1 ? "STGTY_STORAGE"
                   : node->_mse == 2 ? "STGTY_STREAM"
                   : node->_mse == 3 ? "STGTY_LOCKBYTES"
                   : node->_mse == 4 ? "STGTY_PROPERTY"
                   : node->_mse == 5 ? "STGTY_ROOT"
                                     : "");

  DBG_BUFFER_WRITE(dbg, " _bflags      : %s\n",
                   node->_bflags == 1 ? "BLACK" : "RED");
  DBG_BUFFER_WRITE(dbg, " _sidLeftSib  : 0x%08x\n", node->_sidLeftSib);
  DBG_BUFFER_WRITE(dbg, " _sidRightSib : 0x%08x\n", node->_sidRightSib);

  if (node->_mse == STGTY_STORAGE || node->_mse == STGTY_ROOT) {
    DBG_BUFFER_WRITE(dbg, " _sidChild    : 0x%08x\n", node->_sidChild);
    DBG_BUFFER_WRITE(dbg, " _clsid       : %ls\n",
                     cfb_CLSIDToText(&(node->_clsId)));
    DBG_BUFFER_WRITE(dbg, " _dwUserFlags : 0x%08x (%d)\n", node->_dwUserFlags,
                     node->_dwUserFlags);
  }

  if (node->_mse == STGTY_INVALID) {
    DBG_BUFFER_WRITE(dbg, " _time  (cre) : 0x%08x%08x\n",
                     node->_time[0].dwHighDateTime,
                     node->_time[0].dwLowDateTime);

    DBG_BUFFER_WRITE(dbg, " _      (mod) : 0x%08x%08x\n",
                     node->_time[1].dwHighDateTime,
                     node->_time[1].dwLowDateTime);
  }

  if (node->_mse == STGTY_STREAM || node->_mse == STGTY_ROOT) {
    DBG_BUFFER_WRITE(dbg, " _sectStart   : 0x%08x (%d)\n", node->_sectStart,
                     node->_sectStart);
    DBG_BUFFER_WRITE(dbg, " _ulSizeLow   : 0x%08x (%d)\n", node->_ulSizeLow,
                     node->_ulSizeLow);
    DBG_BUFFER_WRITE(dbg, " _ulSizeHigh  : 0x%08x (%d)\n", node->_ulSizeHigh,
                     node->_ulSizeHigh);
  }

  DBG_BUFFER_WRITE(dbg, "\n\n");

  dbg->debug_callback(dbg, (void *)cfbd, DEBUG_SRC_ID_DUMP, 0, "", "", 0,
                      dbg->_dbg_msg, dbg->user);

  if (print_stream == 1) {
    cfb_dump_nodeStream(cfbd, node);
  }
}

void cfb_dump_nodePath(CFB_Data *cfbd, const wchar_t *path, int print_stream) {
  cfbNode *node = cfb_getNodeByPath(cfbd, path, 0);

  if (node == NULL) {
    error("cfb_dump_nodePath() : Could not find node at \"%ls\"\n", path);
    return;
  }

  cfb_dump_node(cfbd, node, print_stream);
}

void cfb_dump_nodeStream(CFB_Data *cfbd, cfbNode *node) {
  struct dbg *dbg = cfbd->dbg;

  unsigned char *stream = NULL;
  uint64_t stream_sz = 0;

  cfb_getStream(cfbd, node, &stream, &stream_sz);

  if (stream == NULL) {
    return;
  }

  laaf_util_dump_hex(stream, stream_sz, &dbg->_dbg_msg, &dbg->_dbg_msg_size,
                     dbg->_dbg_msg_pos);

  dbg->debug_callback(dbg, (void *)cfbd, DEBUG_SRC_ID_DUMP, 0, "", "", 0,
                      dbg->_dbg_msg, dbg->user);

  free(stream);
}

void cfb_dump_nodePathStream(CFB_Data *cfbd, const wchar_t *path) {
  struct dbg *dbg = cfbd->dbg;

  cfbNode *node = cfb_getNodeByPath(cfbd, path, 0);

  if (node == NULL) {
    error("cfb_dump_nodePathStream() : Could not find node at \"%ls\"\n", path);
    return;
  }

  unsigned char *stream = NULL;
  uint64_t stream_sz = 0;

  cfb_getStream(cfbd, node, &stream, &stream_sz);

  laaf_util_dump_hex(stream, stream_sz, &dbg->_dbg_msg, &dbg->_dbg_msg_size,
                     dbg->_dbg_msg_pos);

  dbg->debug_callback(dbg, (void *)cfbd, DEBUG_SRC_ID_DUMP, 0, "", "", 0,
                      dbg->_dbg_msg, dbg->user);

  free(stream);
}

void cfb_dump_nodePaths(CFB_Data *cfbd, uint32_t prevPath, char *strArray[],
                        uint32_t *str_i, cfbNode *node) {
  struct dbg *dbg = cfbd->dbg;

  if (node == NULL) {
    /* the begining of the first function call. */
    node = &cfbd->nodes[0];
    strArray = calloc(cfbd->nodes_cnt, sizeof(char *));
  }

  uint32_t thisPath = (*str_i);
  wchar_t nodeName[CFB_NODE_NAME_SZ];

  cfb_w16towchar(nodeName, node->_ab, node->_cb);

  int pathlen = snprintf(NULL, 0, "%s/%ls", strArray[prevPath], nodeName);

  if (pathlen < 0) {
    // TODO error
    return;
  }

  pathlen++;

  strArray[thisPath] = malloc(pathlen);

  snprintf(strArray[thisPath], pathlen, "%s/%ls", strArray[prevPath], nodeName);

  (*str_i)++;

  if ((int32_t)node->_sidChild > 0)
    cfb_dump_nodePaths(cfbd, thisPath, strArray, str_i,
                       &cfbd->nodes[node->_sidChild]);

  if ((int32_t)node->_sidLeftSib > 0)
    cfb_dump_nodePaths(cfbd, prevPath, strArray, str_i,
                       &cfbd->nodes[node->_sidLeftSib]);

  if ((int32_t)node->_sidRightSib > 0)
    cfb_dump_nodePaths(cfbd, prevPath, strArray, str_i,
                       &cfbd->nodes[node->_sidRightSib]);

  /* the end of the first function call, recursion is over. */
  if (node == &cfbd->nodes[0]) {

    /* commented out because output is proper this way... why did we call
     * qsort() in the first place ?! */
    // qsort( strArray, *str_i, sizeof(char*), compareStrings );

    for (uint32_t i = 0; i < cfbd->nodes_cnt && strArray[i] != NULL; i++) {
      DBG_BUFFER_WRITE(dbg, "%05i : %s\n", i, strArray[i]);
      free(strArray[i]);
    }

    free(strArray);

    DBG_BUFFER_WRITE(dbg, "\n\n");

    dbg->debug_callback(dbg, (void *)cfbd, DEBUG_SRC_ID_DUMP, 0, "", "", 0,
                        dbg->_dbg_msg, dbg->user);
  }
}

void cfb_dump_header(CFB_Data *cfbd) {
  struct dbg *dbg = cfbd->dbg;

  cfbHeader *cfbh = cfbd->hdr;

  DBG_BUFFER_WRITE(dbg, "_abSig              : 0x%08" PRIx64 "\n",
                   cfbh->_abSig);
  DBG_BUFFER_WRITE(dbg, "_clsId              : %ls\n",
                   cfb_CLSIDToText(&(cfbh->_clsid)));
  DBG_BUFFER_WRITE(dbg, " version            : %u.%u ( 0x%04x 0x%04x )\n",
                   cfbh->_uMinorVersion, cfbh->_uDllVersion,
                   cfbh->_uMinorVersion, cfbh->_uDllVersion);
  DBG_BUFFER_WRITE(dbg, "_uByteOrder         : %s ( 0x%04x )\n",
                   cfbh->_uByteOrder == 0xFFFE   ? "little-endian"
                   : cfbh->_uByteOrder == 0xFEFF ? "big-endian"
                                                 : "?",
                   cfbh->_uByteOrder);
  DBG_BUFFER_WRITE(dbg, "_uSectorShift       : %u (%u bytes sectors)\n",
                   cfbh->_uSectorShift, 1 << cfbh->_uSectorShift);
  DBG_BUFFER_WRITE(dbg, "_uMiniSectorShift   : %u (%u bytes mini-sectors)\n",
                   cfbh->_uMiniSectorShift, 1 << cfbh->_uMiniSectorShift);
  DBG_BUFFER_WRITE(dbg, "_usReserved0        : 0x%02x\n", cfbh->_usReserved);
  DBG_BUFFER_WRITE(dbg, "_ulReserved1        : 0x%04x\n", cfbh->_ulReserved1);
  DBG_BUFFER_WRITE(dbg, "_csectDir           : %u\n", cfbh->_csectDir);
  DBG_BUFFER_WRITE(dbg, "_csectFat           : %u\n", cfbh->_csectFat);
  DBG_BUFFER_WRITE(dbg, "_sectDirStart       : %u\n", cfbh->_sectDirStart);
  DBG_BUFFER_WRITE(dbg, "_signature          : %u\n", cfbh->_signature);
  DBG_BUFFER_WRITE(dbg, "_ulMiniSectorCutoff : %u\n",
                   cfbh->_ulMiniSectorCutoff);
  DBG_BUFFER_WRITE(dbg, "_sectMiniFatStart   : %u\n", cfbh->_sectMiniFatStart);
  DBG_BUFFER_WRITE(dbg, "_csectMiniFat       : %u\n", cfbh->_csectMiniFat);
  DBG_BUFFER_WRITE(dbg, "_sectDifStart       : %u\n", cfbh->_sectDifStart);
  DBG_BUFFER_WRITE(dbg, "_csectDif           : %u\n", cfbh->_csectDif);

  DBG_BUFFER_WRITE(dbg, "\n");

  dbg->debug_callback(dbg, (void *)cfbd, DEBUG_SRC_ID_DUMP, 0, "", "", 0,
                      dbg->_dbg_msg, dbg->user);
}

void cfb_dump_FAT(CFB_Data *cfbd) {
  struct dbg *dbg = cfbd->dbg;

  DBG_BUFFER_WRITE(dbg, "_CFB_FAT______________________________________________"
                        "_________________________________________\n\n");

  uint32_t i = 0;

  for (i = 0; i < cfbd->fat_sz; i++) {
    DBG_BUFFER_WRITE(dbg, " SECT[%u] : 0x%08x %s\n", i, cfbd->fat[i],
                     (cfbd->fat[i] == CFB_MAX_REG_SECT)   ? "(CFB_MAX_REG_SECT)"
                     : (cfbd->fat[i] == CFB_DIFAT_SECT)   ? "(CFB_DIFAT_SECT)"
                     : (cfbd->fat[i] == CFB_FAT_SECT)     ? "(CFB_FAT_SECT)"
                     : (cfbd->fat[i] == CFB_END_OF_CHAIN) ? "(CFB_END_OF_CHAIN)"
                     : (cfbd->fat[i] == CFB_FREE_SECT)    ? "(CFB_FREE_SECT)"
                                                          : "");
  }

  DBG_BUFFER_WRITE(dbg, "\n");

  DBG_BUFFER_WRITE(dbg, " End of FAT.\n\n");

  DBG_BUFFER_WRITE(dbg, " Total FAT entries   : %u\n", cfbd->fat_sz);
  DBG_BUFFER_WRITE(dbg, " Count of FAT sector : %u\n", cfbd->hdr->_csectFat);

  DBG_BUFFER_WRITE(dbg, "\n\n");

  dbg->debug_callback(dbg, (void *)cfbd, DEBUG_SRC_ID_DUMP, 0, "", "", 0,
                      dbg->_dbg_msg, dbg->user);
}

void cfb_dump_MiniFAT(CFB_Data *cfbd) {
  struct dbg *dbg = cfbd->dbg;

  DBG_BUFFER_WRITE(dbg, "_CFB_MiniFAT__________________________________________"
                        "_________________________________________\n\n");

  uint32_t i = 0;

  for (i = 0; i < cfbd->miniFat_sz; i++) {
    DBG_BUFFER_WRITE(
        dbg, " SECT[%u] : 0x%08x %s\n", i, cfbd->miniFat[i],
        (cfbd->miniFat[i] == CFB_MAX_REG_SECT)   ? "(CFB_MAX_REG_SECT)"
        : (cfbd->miniFat[i] == CFB_DIFAT_SECT)   ? "(CFB_DIFAT_SECT)"
        : (cfbd->miniFat[i] == CFB_FAT_SECT)     ? "(CFB_FAT_SECT)"
        : (cfbd->miniFat[i] == CFB_END_OF_CHAIN) ? "(CFB_END_OF_CHAIN)"
        : (cfbd->miniFat[i] == CFB_FREE_SECT)    ? "(CFB_FREE_SECT)"
                                                 : "");
  }

  DBG_BUFFER_WRITE(dbg, "\n");

  DBG_BUFFER_WRITE(dbg, " End of MiniFAT.\n\n");

  DBG_BUFFER_WRITE(dbg, " Total MiniFAT entries   : %u\n", cfbd->miniFat_sz);
  DBG_BUFFER_WRITE(dbg, " First MiniFAT sector ID : %u\n",
                   cfbd->hdr->_sectMiniFatStart);
  DBG_BUFFER_WRITE(dbg, " Count of MiniFAT sector : %u\n",
                   cfbd->hdr->_csectMiniFat);

  DBG_BUFFER_WRITE(dbg, "\n\n");

  dbg->debug_callback(dbg, (void *)cfbd, DEBUG_SRC_ID_DUMP, 0, "", "", 0,
                      dbg->_dbg_msg, dbg->user);
}

void cfb_dump_DiFAT(CFB_Data *cfbd) {
  struct dbg *dbg = cfbd->dbg;

  DBG_BUFFER_WRITE(dbg, "_CFB_DiFAT____________________________________________"
                        "_________________________________________\n\n");

  uint32_t i = 0;

  for (i = 0; i < cfbd->DiFAT_sz; i++) {
    DBG_BUFFER_WRITE(dbg, " SECT[%u] : 0x%08x %s\n", i, cfbd->DiFAT[i],
                     (cfbd->DiFAT[i] == CFB_MAX_REG_SECT) ? "(CFB_MAX_REG_SECT)"
                     : (cfbd->DiFAT[i] == CFB_DIFAT_SECT) ? "(CFB_DIFAT_SECT)"
                     : (cfbd->DiFAT[i] == CFB_FAT_SECT)   ? "(CFB_FAT_SECT)"
                     : (cfbd->DiFAT[i] == CFB_END_OF_CHAIN)
                         ? "(CFB_END_OF_CHAIN)"
                     : (cfbd->DiFAT[i] == CFB_FREE_SECT) ? "(CFB_FREE_SECT)"
                                                         : "");
  }

  DBG_BUFFER_WRITE(dbg, "\n");

  DBG_BUFFER_WRITE(dbg, " End of DiFAT.\n\n");

  DBG_BUFFER_WRITE(dbg, " Total DiFAT entries   : %u\n", cfbd->DiFAT_sz);
  DBG_BUFFER_WRITE(dbg, " First DiFAT sector ID : %u\n",
                   cfbd->hdr->_sectDifStart);
  DBG_BUFFER_WRITE(dbg, " Count of DiFAT sector : Header + %u\n",
                   cfbd->hdr->_csectDif);

  DBG_BUFFER_WRITE(dbg, "\n\n");

  dbg->debug_callback(dbg, (void *)cfbd, DEBUG_SRC_ID_DUMP, 0, "", "", 0,
                      dbg->_dbg_msg, dbg->user);
}
