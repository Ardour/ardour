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

#include <gtkmm/menushell.h>
#include <gtkmm/menuitem.h>

#include "gtkmm2ext/application.h"
#include "gtkmm2ext/gtkapplication.h"

using namespace Gtk;
using namespace Gtkmm2ext;

Application* Application::_instance = 0;

Application*
Application::instance ()
{
	if (!_instance) {
		_instance = new Application;
	}
	return _instance;
}

Application::Application ()
{
	gtk_application_init ();
}

Application::~Application ()
{
	_instance = 0;
	gtk_application_cleanup ();
}

void
Application::ready ()
{
	gtk_application_ready ();
}

void
Application::hide ()
{
    gtk_application_hide ();
}

void
Application::cleanup ()
{
	gtk_application_cleanup ();
}

void
Application::set_menu_bar (MenuShell& shell)
{
	gtk_application_set_menu_bar (shell.gobj());
}

GtkApplicationMenuGroup*
Application::add_app_menu_group ()
{
	return gtk_application_add_app_menu_group ();
}

void
Application::add_app_menu_item (GtkApplicationMenuGroup* group,
				MenuItem* item)
{
	gtk_application_add_app_menu_item (group, item->gobj());
}
