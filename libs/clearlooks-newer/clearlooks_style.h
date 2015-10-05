/* Clearlooks Engine
 * Copyright (C) 2005 Richard Stellingwerff.
 * Copyright (C) 2006 Benjamin Berg
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
 * Written by Owen Taylor <otaylor@redhat.com>
 * and by Alexander Larsson <alexl@redhat.com>
 * Modified by Richard Stellingwerff <remenic@gmail.com>
 */
#include <gtk/gtkstyle.h>

#ifndef CLEARLOOKS_STYLE_H
#define CLEARLOOKS_STYLE_H

#include "animation.h"
#include "clearlooks_types.h"

typedef struct _ClearlooksStyle ClearlooksStyle;
typedef struct _ClearlooksStyleClass ClearlooksStyleClass;

GE_INTERNAL extern GType clearlooks_type_style;

#define CLEARLOOKS_TYPE_STYLE              clearlooks_type_style
#define CLEARLOOKS_STYLE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), CLEARLOOKS_TYPE_STYLE, ClearlooksStyle))
#define CLEARLOOKS_STYLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLEARLOOKS_TYPE_STYLE, ClearlooksStyleClass))
#define CLEARLOOKS_IS_STYLE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), CLEARLOOKS_TYPE_STYLE))
#define CLEARLOOKS_IS_STYLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLEARLOOKS_TYPE_STYLE))
#define CLEARLOOKS_STYLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLEARLOOKS_TYPE_STYLE, ClearlooksStyleClass))

struct _ClearlooksStyle
{
	GtkStyle parent_instance;

	ClearlooksColors colors;

	ClearlooksStyles style;

	guint8   menubarstyle;
	guint8   toolbarstyle;
	GdkColor scrollbar_color;
	gboolean colorize_scrollbar;
	gboolean has_scrollbar_color;
	gboolean animation;
	gfloat   radius;
};

struct _ClearlooksStyleClass
{
	GtkStyleClass parent_class;

	ClearlooksStyleFunctions style_functions[CL_NUM_STYLES];
};

GE_INTERNAL void clearlooks_style_register_type (GTypeModule *module);

#endif /* CLEARLOOKS_STYLE_H */
