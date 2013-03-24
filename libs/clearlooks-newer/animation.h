/* Clearlooks Engine
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
 */

#include "config.h"

#ifdef HAVE_ANIMATION
#include <gtk/gtk.h>
#include <ge-support.h>

#define CL_IS_PROGRESS_BAR(widget) GE_IS_PROGRESS_BAR(widget) && widget->allocation.x != -1 && widget->allocation.y != -1
#define ANIMATION_DELAY 100
#define CHECK_ANIMATION_TIME 0.5

GE_INTERNAL void     clearlooks_animation_progressbar_add (GtkWidget *progressbar);
GE_INTERNAL void     clearlooks_animation_connect_checkbox (GtkWidget *widget);
GE_INTERNAL gboolean clearlooks_animation_is_animated (GtkWidget *widget);
GE_INTERNAL gdouble  clearlooks_animation_elapsed (gpointer data);
GE_INTERNAL void     clearlooks_animation_cleanup (void);
#endif /* HAVE_ANIMATION */
