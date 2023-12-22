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

#ifndef __AAFIParser_h__
#define __AAFIParser_h__

/**
 * @file LibAAF/AAFIface/AAFIParser.h
 * @brief AAF processing
 * @author Adrien Gesta-Fline
 * @version 0.1
 * @date 27 june 2018
 *
 * @ingroup AAFIface
 * @addtogroup AAFIface
 * @{
 */

#include <libaaf/AAFCore.h>
#include <libaaf/AAFIface.h>

enum trace_dump_state {
  TD_OK = 0,
  TD_INFO,
  TD_WARNING,
  TD_ERROR,
  TD_NOT_SUPPORTED
};

typedef struct trace_dump {
  int fn;  // line number of current __td
  int pfn; // line number of previous __td
  int lv;  // current level
  int *ll; // level loop : each entry correspond to a level and tell if there is
           // more to print
  int eob; // end of branch
  int hc;  // have children
  int sub;
} td;

#define __td_set(__td, __ptd, offset)                                          \
  __td.fn = __LINE__;                                                          \
  __td.pfn = __ptd->fn;                                                        \
  __td.lv = __ptd->lv + offset;                                                \
  __td.ll = __ptd->ll;                                                         \
  __td.ll[__td.lv] = (offset > 0) ? 0 : __td.ll[__td.lv];                      \
  __td.eob = 0;                                                                \
  __td.hc = 0;                                                                 \
  __td.sub = 0;

#define DUMP_OBJ(aafi, Obj, __td)                                              \
  aafi_dump_obj(aafi, Obj, __td, TD_OK, __LINE__, "");

#define DUMP_OBJ_INFO(aafi, Obj, __td, ...)                                    \
  aafi_dump_obj(aafi, Obj, __td, TD_OK, __LINE__, __VA_ARGS__);

#define DUMP_OBJ_WARNING(aafi, Obj, __td, ...)                                 \
  aafi_dump_obj(aafi, Obj, __td, TD_WARNING, __LINE__, __VA_ARGS__);

#define DUMP_OBJ_ERROR(aafi, Obj, __td, ...)                                   \
  (__td)->eob = 1;                                                             \
  aafi_dump_obj(aafi, Obj, __td, TD_ERROR, __LINE__, __VA_ARGS__);

#define DUMP_OBJ_NO_SUPPORT(aafi, Obj, __td)                                   \
  (__td)->eob = 1;                                                             \
  aafi_dump_obj_no_support(aafi, Obj, __td, __LINE__);

int aafi_retrieveData(AAF_Iface *aafi);

/*
 * The following functions are declared beyond AAFIparser.c scope,
 * so they are accessible to vendor-specific files (Resolve.c, ProTools.c, etc.)
 */

void aafi_dump_obj(AAF_Iface *aafi, aafObject *Obj, struct trace_dump *__td,
                   int state, int line, const char *fmt, ...);

void aafi_dump_obj_no_support(AAF_Iface *aafi, aafObject *Obj,
                              struct trace_dump *__td, int line);

void aafi_trace_obj(AAF_Iface *aafi, aafObject *Obj, const char *color);

int aafi_parse_Segment(AAF_Iface *aafi, aafObject *Segment, td *__ptd);

#endif // !__AAFIParser_h__
