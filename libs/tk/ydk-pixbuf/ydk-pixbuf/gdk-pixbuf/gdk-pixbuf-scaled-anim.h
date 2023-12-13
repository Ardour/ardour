/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* GdkPixbuf library - Simple transformations of animations
 *
 * Copyright (C) 2007 Red Hat, Inc
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GDK_PIXBUF_SCALED_ANIM_H
#define GDK_PIXBUF_SCALED_ANIM_H

#include <gdk-pixbuf/gdk-pixbuf-animation.h>

G_BEGIN_DECLS

#define GDK_TYPE_PIXBUF_SCALED_ANIM              (gdk_pixbuf_scaled_anim_get_type ())
#define GDK_TYPE_PIXBUF_SCALED_ANIM_ITER         (gdk_pixbuf_scaled_anim_iter_get_type ())

typedef struct _GdkPixbufScaledAnim GdkPixbufScaledAnim;
typedef struct _GdkPixbufScaledAnimClass GdkPixbufScaledAnimClass;

GType gdk_pixbuf_scaled_anim_get_type (void) G_GNUC_CONST;
GType gdk_pixbuf_scaled_anim_iter_get_type (void) G_GNUC_CONST;

GdkPixbufScaledAnim *_gdk_pixbuf_scaled_anim_new (GdkPixbufAnimation *anim,
                                                  gdouble             xscale, 
                                                  gdouble             yscale,
                                                  gdouble             tscale);

G_END_DECLS

#endif  /* GDK_PIXBUF_SCALED_ANIM_H */
