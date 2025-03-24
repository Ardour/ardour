/* GtkCellRendererSpin
 * Copyright (C) 2004 Lorenzo Gil Sanchez
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_CELL_RENDERER_SPIN_H__
#define __GTK_CELL_RENDERER_SPIN_H__

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <ytk/ytk.h> can be included directly."
#endif

#include <ytk/gtkcellrenderertext.h>

G_BEGIN_DECLS

#define GTK_TYPE_CELL_RENDERER_SPIN		(gtk_cell_renderer_spin_get_type ())
#define GTK_CELL_RENDERER_SPIN(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_RENDERER_SPIN, GtkCellRendererSpin))
#define GTK_CELL_RENDERER_SPIN_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_RENDERER_SPIN, GtkCellRendererSpinClass))
#define GTK_IS_CELL_RENDERER_SPIN(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_RENDERER_SPIN))
#define GTK_IS_CELL_RENDERER_SPIN_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_RENDERER_SPIN))
#define GTK_CELL_RENDERER_SPIN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CELL_RENDERER_SPIN, GtkCellRendererTextClass))

typedef struct _GtkCellRendererSpin        GtkCellRendererSpin;
typedef struct _GtkCellRendererSpinClass   GtkCellRendererSpinClass;
typedef struct _GtkCellRendererSpinPrivate GtkCellRendererSpinPrivate;

struct _GtkCellRendererSpin
{
  GtkCellRendererText parent;
};

struct _GtkCellRendererSpinClass
{
  GtkCellRendererTextClass parent;
};

GType            gtk_cell_renderer_spin_get_type (void);
GtkCellRenderer *gtk_cell_renderer_spin_new      (void);

G_END_DECLS

#endif  /* __GTK_CELL_RENDERER_SPIN_H__ */
