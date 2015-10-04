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

#include <gtkmm2ext/gtkapplication.h>
#include <gtkmm2ext/gtkapplication-private.h>

GList *_gtk_application_menu_groups = NULL;

GtkApplicationMenuGroup *
gtk_application_add_app_menu_group (void)
{
  GtkApplicationMenuGroup *group = g_slice_new0 (GtkApplicationMenuGroup);
  _gtk_application_menu_groups = g_list_append (_gtk_application_menu_groups, group);
  return group;
}

