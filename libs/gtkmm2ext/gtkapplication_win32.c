/* GTK+ Integration with platform-specific application-wide features 
 * such as the OS X menubar and application delegate concepts (for X11)
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

int  
gtk_application_init (void)
{
	return 0;
}

void gtk_application_cleanup (void)
{
}

void                      
gtk_application_set_menu_bar (GtkMenuShell* menushell)
{
}

void                      
gtk_application_add_app_menu_item (GtkApplicationMenuGroup* group, GtkMenuItem* item)
{
}

void
gtk_application_ready (void)
{
}
