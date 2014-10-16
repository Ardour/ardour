/* GTK+ Integration with platform-specific application-wide features 
 * such as the OS X menubar and application delegate concepts.
 *
 * Copyright (C) 2007 Pioneer Research Center USA, Inc.
 * Copyright (C) 2007 Imendio AB
 * Copyright (C) 2009 Paul Davis
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; version 2.1
 * of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_APPLICATION_H__
#define __GTK_APPLICATION_H__

#include <gtk/gtk.h>

#include "gtkmm2ext/visibility.h"

G_BEGIN_DECLS

typedef struct _GtkApplicationMenuGroup GtkApplicationMenuGroup;

LIBGTKMM2EXT_API int  gtk_application_init (void);
LIBGTKMM2EXT_API void gtk_application_ready (void);
LIBGTKMM2EXT_API void gtk_application_cleanup (void);

LIBGTKMM2EXT_API void                      gtk_application_set_menu_bar       (GtkMenuShell    *menu_shell);
LIBGTKMM2EXT_API GtkApplicationMenuGroup * gtk_application_add_app_menu_group (void);
LIBGTKMM2EXT_API void                      gtk_application_add_app_menu_item   (GtkApplicationMenuGroup *group,
							       GtkMenuItem     *menu_item);

/* these are private but here until GtkApplication becomes a GtkObject with an interface */

LIBGTKMM2EXT_LOCAL extern GList *_gtk_application_menu_groups;

G_END_DECLS

#endif /* __GTK_APPLICATION_H__ */
