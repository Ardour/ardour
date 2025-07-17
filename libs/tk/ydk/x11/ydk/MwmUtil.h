/**
 *
 * $Id$
 *
 * Copyright (C) 1995 Free Software Foundation, Inc.
 *
 * This file is part of the GNU LessTif Library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 *
 * * Feb 21 1999 - George Lebl (jirka@5z.com)
 *                 Owen Taylor (otaylor@redhat.com)
 *
 *   Modified so that the MotifWmHints structure defined here
 *   is suitable for client side use on 64-bit architectures.
 *   X expects fields with a format of 32 to be longs, even
 *   when sizeof(long) == 8.
 **/

#ifndef MWMUTIL_H_INCLUDED
#define MWMUTIL_H_INCLUDED

#include <X11/Xmd.h>

G_BEGIN_DECLS

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
} MotifWmHints, MwmHints;

#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

#define MWM_FUNC_ALL            (1L << 0)
#define MWM_FUNC_RESIZE         (1L << 1)
#define MWM_FUNC_MOVE           (1L << 2)
#define MWM_FUNC_MINIMIZE       (1L << 3)
#define MWM_FUNC_MAXIMIZE       (1L << 4)
#define MWM_FUNC_CLOSE          (1L << 5)

#define MWM_DECOR_ALL           (1L << 0)
#define MWM_DECOR_BORDER        (1L << 1)
#define MWM_DECOR_RESIZEH       (1L << 2)
#define MWM_DECOR_TITLE         (1L << 3)
#define MWM_DECOR_MENU          (1L << 4)
#define MWM_DECOR_MINIMIZE      (1L << 5)
#define MWM_DECOR_MAXIMIZE      (1L << 6)

#define MWM_INPUT_MODELESS 0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL 1
#define MWM_INPUT_SYSTEM_MODAL 2
#define MWM_INPUT_FULL_APPLICATION_MODAL 3
#define MWM_INPUT_APPLICATION_MODAL MWM_INPUT_PRIMARY_APPLICATION_MODAL

#define MWM_TEAROFF_WINDOW	(1L<<0)

/*
 * atoms
 */
#define _XA_MOTIF_BINDINGS		"_MOTIF_BINDINGS"
#define _XA_MOTIF_WM_HINTS		"_MOTIF_WM_HINTS"
#define _XA_MOTIF_WM_MESSAGES		"_MOTIF_WM_MESSAGES"
#define _XA_MOTIF_WM_OFFSET		"_MOTIF_WM_OFFSET"
#define _XA_MOTIF_WM_MENU		"_MOTIF_WM_MENU"
#define _XA_MOTIF_WM_INFO		"_MOTIF_WM_INFO"
#define _XA_MWM_HINTS			_XA_MOTIF_WM_HINTS
#define _XA_MWM_MESSAGES		_XA_MOTIF_WM_MESSAGES
#define _XA_MWM_MENU			_XA_MOTIF_WM_MENU
#define _XA_MWM_INFO			_XA_MOTIF_WM_INFO


/*
 * _MWM_INFO property
 */
typedef struct {
    long flags;
    Window wm_window;
} MotifWmInfo;

typedef MotifWmInfo MwmInfo;

#define MWM_INFO_STARTUP_STANDARD	(1L<<0)
#define MWM_INFO_STARTUP_CUSTOM		(1L<<1)

/*
 * _MWM_HINTS property
 */
typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long inputMode;
    unsigned long status;
} PropMotifWmHints;

typedef PropMotifWmHints PropMwmHints;

#define PROP_MOTIF_WM_HINTS_ELEMENTS 5
#define PROP_MWM_HINTS_ELEMENTS PROP_MOTIF_WM_HINTS_ELEMENTS

/*
 * _MWM_INFO property, slight return
 */
typedef struct {
    unsigned long flags;
    unsigned long wmWindow;
} PropMotifWmInfo;

typedef PropMotifWmInfo PropMwmInfo;

#define PROP_MOTIF_WM_INFO_ELEMENTS 2
#define PROP_MWM_INFO_ELEMENTS PROP_MOTIF_WM_INFO_ELEMENTS

G_END_DECLS

#endif /* MWMUTIL_H_INCLUDED */
