// -*- c++ -*-
#ifndef _GLIBMM_DEBUG_H
#define _GLIBMM_DEBUG_H

/* $Id: debug.h 785 2009-02-17 19:03:06Z daniel $ */

/* Copyright 2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glib.h>
#include <glibmmconfig.h>

// Some stuff that's useful when debugging gtkmm internals:

#ifdef GLIBMM_DEBUG_REFCOUNTING

/* We can't use the equivalent GLib macro because it's always disabled in C++,
 * even though __PRETTY_FUNCTION__ works fine in C++ as well if you use it
 * right (i.e. concatenation with string literals isn't allowed).
 */
#ifdef __GNUC__
#define GLIBMM_GNUC_PRETTY_FUNCTION __PRETTY_FUNCTION__
#else
#define GLIBMM_GNUC_PRETTY_FUNCTION ""
#endif

#define GLIBMM_DEBUG_REFERENCE(cppInstance, cInstance)                               \
    G_STMT_START{                                                                   \
      void *const cppInstance__ = (void*) (cppInstance);                            \
      void *const cInstance__   = (void*) (cInstance);                              \
      g_log(G_LOG_DOMAIN,                                                           \
            G_LOG_LEVEL_DEBUG,                                                      \
            "file %s: line %d (%s):\n"                                              \
            "ref: C++ instance: %p; C instance: %p, ref_count = %u, type = %s\n",   \
            __FILE__,                                                               \
            __LINE__,                                                               \
            GLIBMM_GNUC_PRETTY_FUNCTION,                                             \
            cppInstance__,                                                          \
            cInstance__,                                                            \
            G_OBJECT(cInstance__)->ref_count,                                       \
            G_OBJECT_TYPE_NAME(cInstance__));                                       \
    }G_STMT_END

#define GLIBMM_DEBUG_UNREFERENCE(cppInstance, cInstance)                             \
    G_STMT_START{                                                                   \
      void *const cppInstance__ = (void*) (cppInstance);                            \
      void *const cInstance__   = (void*) (cInstance);                              \
      g_log(G_LOG_DOMAIN,                                                           \
            G_LOG_LEVEL_DEBUG,                                                      \
            "file %s: line %d (%s):\n"                                              \
            "unref: C++ instance: %p; C instance: %p, ref_count = %u, type = %s\n", \
            __FILE__,                                                               \
            __LINE__,                                                               \
            GLIBMM_GNUC_PRETTY_FUNCTION,                                             \
            cppInstance__,                                                          \
            cInstance__,                                                            \
            G_OBJECT(cInstance__)->ref_count,                                       \
            G_OBJECT_TYPE_NAME(cInstance__));                                       \
    }G_STMT_END

#else

#define GLIBMM_DEBUG_REFERENCE(cppInstance,cInstance)    G_STMT_START{ (void)0; }G_STMT_END
#define GLIBMM_DEBUG_UNREFERENCE(cppInstance,cInstance)  G_STMT_START{ (void)0; }G_STMT_END

#endif /* GLIBMM_DEBUG_REFCOUNTING */

#endif /* _GLIBMM_DEBUG_H */

