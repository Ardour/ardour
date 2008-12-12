/* Clearlooks Engine
 * Copyright (C) 2005 Richard Stellingwerff.
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

#include "clearlooks_draw.h"

typedef struct _ClearlooksStyle ClearlooksStyle;
typedef struct _ClearlooksStyleClass ClearlooksStyleClass;

extern GType clearlooks_type_style;

#define CLEARLOOKS_TYPE_STYLE              clearlooks_type_style
#define CLEARLOOKS_STYLE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), CLEARLOOKS_TYPE_STYLE, ClearlooksStyle))
#define CLEARLOOKS_STYLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLEARLOOKS_TYPE_STYLE, ClearlooksStyleClass))
#define CLEARLOOKS_IS_STYLE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), CLEARLOOKS_TYPE_STYLE))
#define CLEARLOOKS_IS_STYLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLEARLOOKS_TYPE_STYLE))
#define CLEARLOOKS_STYLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLEARLOOKS_TYPE_STYLE, ClearlooksStyleClass))

typedef enum
{
	CL_BORDER_UPPER = 0,
	CL_BORDER_LOWER,
	CL_BORDER_UPPER_ACTIVE,
	CL_BORDER_LOWER_ACTIVE,
	CL_BORDER_COUNT
} ClBorderColorType;

typedef enum
{
	CL_SCROLLBUTTON_BEGIN = 0,
	CL_SCROLLBUTTON_END,
	CL_SCROLLBUTTON_OTHER
} ClScrollButtonType;

struct _ClearlooksStyle
{
	GtkStyle parent_instance;
	
	GdkColor shade[9];
	
	GdkColor spot_color;
	GdkColor spot1;
	GdkColor spot2;
	GdkColor spot3;
	
	GdkColor border[CL_BORDER_COUNT];
	
	/* from light to dark */
	GdkGC *shade_gc[9];
	GdkGC *border_gc[CL_BORDER_COUNT];
	
	GdkGC *spot1_gc;
	GdkGC *spot2_gc;
	GdkGC *spot3_gc;
	
	GdkColor inset_light[5];
	GdkColor inset_dark[5];
	
	GdkColor button_g1[5];
	GdkColor button_g2[5];
	GdkColor button_g3[5];
	GdkColor button_g4[5];
	
	GdkColor listview_bg[5];

	GdkPixmap *radio_pixmap_nonactive[5];
	GdkPixmap *radio_pixmap_active[5];
	GdkPixmap *radio_pixmap_inconsistent[5];
	GdkBitmap *radio_pixmap_mask; /* All masks are the same */
	
	GdkPixmap *check_pixmap_nonactive[5];
	GdkPixmap *check_pixmap_active[5];
	GdkPixmap *check_pixmap_inconsistent[5];
	
	gboolean sunkenmenubar:1;

	guint8   progressbarstyle;
	guint8   menubarstyle;
	guint8   menuitemstyle;
	guint8   listviewitemstyle;
};

struct _ClearlooksStyleClass
{
  GtkStyleClass parent_class;
};

void clearlooks_style_register_type (GTypeModule *module);
