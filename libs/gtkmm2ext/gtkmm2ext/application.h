/* GTK+ Integration with platform-specific application-wide features
 * such as the OS X menubar and application delegate concepts.
 *
 * Copyright (C) 2009 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __GTK_APPLICATION_MM_H__
#define __GTK_APPLICATION_MM_H__

#include <sigc++/signal.h>

#include "gtkmm2ext/visibility.h"
#include "gtkmm2ext/gtkapplication.h" // for GtkApplicationGroup typedef

namespace Gtk {
	class MenuItem;
	class MenuShell;
}

namespace Glib {
	class ustring;
}

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API Application
{
public:
    static Application* instance();
    ~Application ();

    void                     ready ();
    void                     hide ();
    void                     cleanup ();
    void                     set_menu_bar (Gtk::MenuShell&);
    GtkApplicationMenuGroup* add_app_menu_group ();
    void                     add_app_menu_item (GtkApplicationMenuGroup*, Gtk::MenuItem*);

    sigc::signal<void,bool>                 ActivationChanged;
    sigc::signal<void,const Glib::ustring&> ShouldLoad;
    sigc::signal<void>                      ShouldQuit;

private:
    Application ();

    static Application* _instance;
};

}

#endif /* __GTK_APPLICATION_MM_H__ */
