/* Clearlooks Theme Engine
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

#include <gtk/gtkrc.h>

typedef struct _ClearlooksRcStyle ClearlooksRcStyle;
typedef struct _ClearlooksRcStyleClass ClearlooksRcStyleClass;

extern GType clearlooks_type_rc_style;

#define CLEARLOOKS_TYPE_RC_STYLE              clearlooks_type_rc_style
#define CLEARLOOKS_RC_STYLE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), CLEARLOOKS_TYPE_RC_STYLE, ClearlooksRcStyle))
#define CLEARLOOKS_RC_STYLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLEARLOOKS_TYPE_RC_STYLE, ClearlooksRcStyleClass))
#define CLEARLOOKS_IS_RC_STYLE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), CLEARLOOKS_TYPE_RC_STYLE))
#define CLEARLOOKS_IS_RC_STYLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLEARLOOKS_TYPE_RC_STYLE))
#define CLEARLOOKS_RC_STYLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLEARLOOKS_TYPE_RC_STYLE, ClearlooksRcStyleClass))

struct _ClearlooksRcStyle
{
  GtkRcStyle parent_instance;

  GdkColor spot_color;
  gboolean has_spot_color;
  double contrast;
  guint8 sunkenmenubar;
  guint8 progressbarstyle;
  guint8 menubarstyle;
  guint8 menuitemstyle;
  guint8 listviewitemstyle;
};

struct _ClearlooksRcStyleClass
{
  GtkRcStyleClass parent_class;
};

void clearlooks_rc_style_register_type (GTypeModule *module);
