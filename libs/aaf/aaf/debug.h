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

#ifndef __debug_h__
#define __debug_h__

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aaf/utils.h"

#define __FILENAME__                                                           \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

enum debug_source_id {
  DEBUG_SRC_ID_LIB_CFB,
  DEBUG_SRC_ID_AAF_CORE,
  DEBUG_SRC_ID_AAF_IFACE,
  DEBUG_SRC_ID_TRACE,
  DEBUG_SRC_ID_DUMP
};

typedef enum verbosityLevel_e {
  VERB_QUIET = 0,
  VERB_ERROR,
  VERB_WARNING,
  VERB_DEBUG,
  MAX_VERB
} verbosityLevel_e;

struct dbg {

  void (*debug_callback)(struct dbg *dbg, void *ctxdata, int lib, int type,
                         const char *srcfile, const char *srcfunc, int lineno,
                         const char *msg, void *user);

  FILE *fp;
  verbosityLevel_e verb;
  int ansicolor;

  char *_dbg_msg;
  int _dbg_msg_size;
  int _dbg_msg_pos;

  char *_dbg_msg_tmp;
  int _dbg_msg_pos_tmp;

  void *user;
};

#define _dbg(dbg, ctxdata, lib, type, ...)                                     \
  {                                                                            \
    const char *dbgfile = __FILENAME__;                                        \
    const char *dbgfunc = __func__;                                            \
    int dbgline = __LINE__;                                                    \
    if (dbg && dbg->verb >= type && dbg->debug_callback) {                     \
      if (dbg->_dbg_msg_pos) {                                                 \
        dbg->_dbg_msg_pos_tmp = dbg->_dbg_msg_pos;                             \
        dbg->_dbg_msg_tmp = laaf_util_c99strdup(dbg->_dbg_msg);                \
      }                                                                        \
      int msgsize = snprintf(NULL, 0, __VA_ARGS__) + 1;                        \
      if (msgsize >= dbg->_dbg_msg_size) {                                     \
        char *msgtmp = realloc(dbg->_dbg_msg, msgsize);                        \
        if (msgtmp) {                                                          \
          dbg->_dbg_msg = msgtmp;                                              \
          dbg->_dbg_msg_size = msgsize;                                        \
          snprintf(dbg->_dbg_msg, dbg->_dbg_msg_size, __VA_ARGS__);            \
          dbg->debug_callback(dbg, (void *)ctxdata, lib, type, dbgfile,        \
                              dbgfunc, dbgline, dbg->_dbg_msg, dbg->user);     \
        } else {                                                               \
          /* realloc() error */                                                \
        }                                                                      \
      } else {                                                                 \
        snprintf(dbg->_dbg_msg, dbg->_dbg_msg_size, __VA_ARGS__);              \
        dbg->debug_callback(dbg, (void *)ctxdata, lib, type, dbgfile, dbgfunc, \
                            dbgline, dbg->_dbg_msg, dbg->user);                \
      }                                                                        \
      if (dbg->_dbg_msg_pos_tmp) {                                             \
        dbg->_dbg_msg_pos = dbg->_dbg_msg_pos_tmp;                             \
        strcpy(dbg->_dbg_msg, dbg->_dbg_msg_tmp);                              \
        free(dbg->_dbg_msg_tmp);                                               \
        dbg->_dbg_msg_tmp = NULL;                                              \
        dbg->_dbg_msg_pos_tmp = 0;                                             \
      }                                                                        \
    }                                                                          \
  }

#define DBG_BUFFER_WRITE(dbg, ...)                                             \
  dbg->_dbg_msg_pos += laaf_util_snprintf_realloc(                             \
      &dbg->_dbg_msg, &dbg->_dbg_msg_size, dbg->_dbg_msg_pos, __VA_ARGS__);

#define DBG_BUFFER_RESET(dbg) dbg->_dbg_msg_pos = 0;

struct dbg *laaf_new_debug(void);

void laaf_free_debug(struct dbg *dbg);

void laaf_debug_callback(struct dbg *dbg, void *ctxdata, int lib, int type,
                         const char *srcfile, const char *srcfunc, int lineno,
                         const char *msg, void *user);

#endif // !__debug_h__
