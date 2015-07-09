/*
    Copyright (C) 2015 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <gtkmm/action.h>
#include <gtkmm/notebook.h>
#include <gtkmm/window.h>

#include "gtkmm2ext/tabbable.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/visibility_tracker.h"

#include "i18n.h"

using namespace Gtkmm2ext;
using namespace Gtk;
using std::string;

Tabbable::Tabbable (Widget& w, const string& name)
	: WindowProxy (name)
	, _contents (w)
{
}

Tabbable::~Tabbable ()
{
	if (_window) {
		delete _window;
		_window = 0;
	}
}

void
Tabbable::add_to_notebook (Notebook& notebook, const string& tab_title)
{
	notebook.append_page (_contents, tab_title, false);
	Gtk::Widget* tab_label = notebook.get_tab_label (_contents);

	if (tab_label) {
		Gtkmm2ext::UI::instance()->set_tip (*tab_label,
		                                    string_compose (_("Drag this tab to the desktop to show %1 in its own window\n\n"
		                                                      "To put the window back, click on its \"close\" button"), tab_title));
	}
	
	notebook.set_tab_detachable (_contents);
	notebook.set_tab_reorderable (_contents);

	_parent_notebook = &notebook;
	_tab_title = tab_title;
}

Window*
Tabbable::use_own_window (bool and_pack_it)
{
	Gtk::Window* win = get (true);

	if (and_pack_it) {
		Gtk::Container* parent = _contents.get_parent();
		if (parent) {
			parent->remove (_contents);
		}
		_own_notebook.append_page (_contents, _tab_title);
	}

	return win;

}

bool
Tabbable::window_visible ()
{
	if (!own_window()) {
		return false;
	}

	return visible();
}

Window*
Tabbable::get (bool create)
{
	if (_window) {
		return _window;
	}

	if (!create) {
		return 0;
	}

	/* From here on, we're creating the window 
	 */
	
	if ((_window = new Window (WINDOW_TOPLEVEL)) == 0) {
		return 0;
	}

	_window->add (_own_notebook);
	_own_notebook.show ();
	_own_notebook.set_show_tabs (false);

	/* do other window-related setup */

	setup ();

	/* window should be ready for derived classes to do something with it */
	
	return _window;
}

Gtk::Notebook*
Tabbable::tab_root_drop ()
{
	Gtk::Allocation alloc;

	alloc = _contents.get_parent()->get_allocation();
	
	(void) use_own_window (false);
	
	/* This is called after a drop of a tab onto the root window. Its
	 * responsibility is to return the notebook that this Tabbable's
	 * contents should be packed into before the drop handling is
	 * completed. It is not responsible for actually taking care of this
	 * packing.
	 */

	_window->set_default_size (alloc.get_width(), alloc.get_height());
	_window->show_all ();
	_window->present ();

	return &_own_notebook;
}

void
Tabbable::show_window ()
{
	Window* toplevel = dynamic_cast<Window*> (_contents.get_toplevel());

	if (toplevel == _window) {
		_window->present ();
	}

	if (!_visible) { /* was hidden, update status */
		set_pos_and_size ();
	}

	if (toplevel != _window) {
		/* not in its own window, just switch parent notebook to show
		   this Tabbable.
		*/
		if (_parent_notebook) {
			_parent_notebook->set_current_page (_parent_notebook->page_num (_contents));
		}
	}
}

bool
Tabbable::delete_event_handler (GdkEventAny *ev)
{
	Window* toplevel = dynamic_cast<Window*> (_contents.get_toplevel());

	if (_window == toplevel) {

		/* unpack Tabbable from parent, put it back in the main tabbed
		 * notebook
		 */

		save_pos_and_size ();

		_contents.get_parent()->remove (_contents);

		/* leave the window around */

		_window->hide ();
		
		if (_parent_notebook) {

			_parent_notebook->append_page (_contents, _tab_title);
			_parent_notebook->set_tab_detachable (_contents);
			_parent_notebook->set_tab_reorderable (_contents);
			_parent_notebook->set_current_page (_parent_notebook->page_num (_contents));
		}

		/* don't let anything else handle this */
		
		return true;
	} 

	/* nothing to do */
	return false;
}

bool
Tabbable::is_tabbed () const
{
	Window* toplevel = (Window*) _contents.get_toplevel();

	if (_window && (toplevel == _window)) {
		return false;
	}

	if (_parent_notebook) {
		return true;
	}
	
	return false;
}

void
Tabbable::show_tab ()
{
	if (!window_visible() && _parent_notebook) {
		_parent_notebook->set_current_page (_parent_notebook->page_num (_contents));
	}
}

Gtk::Window*
Tabbable::current_toplevel () const
{
	return dynamic_cast<Gtk::Window*> (contents().get_toplevel());
}

string
Tabbable::xml_node_name()
{
	return WindowProxy::xml_node_name();
}

XMLNode&
Tabbable::get_state()
{
	XMLNode& node (WindowProxy::get_state());

	return node;
}

int
Tabbable::set_state (const XMLNode& node, int version)
{
	int ret;

	if ((ret = WindowProxy::set_state (node, version)) == 0) {
		if (_visible) {
			if (use_own_window (true) == 0) {
				ret = -1;
			}
		}
	}

	return ret;
}
