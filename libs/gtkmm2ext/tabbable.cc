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
#include <gtkmm/stock.h>

#include "gtkmm2ext/tabbable.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/visibility_tracker.h"

#include "pbd/stacktrace.h"

#include "pbd/i18n.h"

using namespace Gtkmm2ext;
using namespace Gtk;
using std::string;

Tabbable::Tabbable (Widget& w, const string& name, bool tabbed_by_default)
	: WindowProxy (name)
	, _contents (w)
	, _parent_notebook (0)
	, tab_requested_by_state (tabbed_by_default)
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
	_parent_notebook = &notebook;

	if (tab_requested_by_state) {
		attach ();
	}
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
		_own_notebook.append_page (_contents);
	}

	return win;

}

bool
Tabbable::window_visible () const
{
	if (!_window) {
		return false;
	}

	return _window->is_visible();
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

	_window->signal_map().connect (sigc::mem_fun (*this, &Tabbable::window_mapped));
	_window->signal_unmap().connect (sigc::mem_fun (*this, &Tabbable::window_unmapped));

	/* do other window-related setup */

	setup ();

	/* window should be ready for derived classes to do something with it */

	return _window;
}

void
Tabbable::show_own_window (bool and_pack_it)
{
	Gtk::Widget* parent = _contents.get_parent();
	Gtk::Allocation alloc;

	if (parent) {
		alloc = parent->get_allocation();
	}

	(void) use_own_window (and_pack_it);

	if (parent) {
		_window->set_default_size (alloc.get_width(), alloc.get_height());
	}

	tab_requested_by_state = false;

	_window->present ();
}

Gtk::Notebook*
Tabbable::tab_root_drop ()
{
	/* This is called after a drop of a tab onto the root window. Its
	 * responsibility xois to return the notebook that this Tabbable's
	 * contents should be packed into before the drop handling is
	 * completed. It is not responsible for actually taking care of this
	 * packing.
	 */

	show_own_window (false);
	return &_own_notebook;
}

void
Tabbable::show_window ()
{
	make_visible ();

	if (_window && (current_toplevel() == _window)) {
		if (!_visible) { /* was hidden, update status */
			set_pos_and_size ();
		}
	}
}

/** If this Tabbable is currently parented by a tab, ensure that the tab is the
 * current one. If it is parented by a window, then toggle the visibility of
 * that window.
 */
void
Tabbable::change_visibility ()
{
	if (tabbed()) {
		_parent_notebook->set_current_page (_parent_notebook->page_num (_contents));
		return;
	}

	if (tab_requested_by_state) {
		/* should be tabbed, but currently isn't parented by a notebook */
		return;
	}

	if (_window && (current_toplevel() == _window)) {
		/* Use WindowProxy method which will rotate then hide */
		toggle();
	}
}

void
Tabbable::make_visible ()
{
	if (_window && (current_toplevel() == _window)) {
		set_pos ();
		_window->present ();
	} else {

		if (!tab_requested_by_state) {
			show_own_window (true);
		} else {
			show_tab ();
		}
	}
}

void
Tabbable::make_invisible ()
{
	if (_window && (current_toplevel() == _window)) {
		_window->hide ();
	} else {
		hide_tab ();
	}
}

void
Tabbable::detach ()
{
	show_own_window (true);
}

void
Tabbable::attach ()
{
	if (!_parent_notebook) {
		return;
	}

	if (tabbed()) {
		/* already tabbed */
		return;
	}


	if (_window && current_toplevel() == _window) {
		/* unpack Tabbable from parent, put it back in the main tabbed
		 * notebook
		 */

		save_pos_and_size ();

		_contents.get_parent()->remove (_contents);

		/* leave the window around */

		_window->hide ();
	}

	_parent_notebook->append_page (_contents);
	_parent_notebook->set_tab_detachable (_contents);
	_parent_notebook->set_tab_reorderable (_contents);
	_parent_notebook->set_current_page (_parent_notebook->page_num (_contents));

	/* have to force this on, which is semantically correct, since
	 * the user has effectively asked for it.
	 */

	tab_requested_by_state = true;
	StateChange (*this);
}

bool
Tabbable::delete_event_handler (GdkEventAny *ev)
{
	_window->hide();

	return true;
}

bool
Tabbable::tabbed () const
{
	if (_window && (current_toplevel() == _window)) {
		return false;
	}

	if (_parent_notebook && (_parent_notebook->page_num (_contents) >= 0)) {
		return true;
	}

	return false;
}

void
Tabbable::hide_tab ()
{
	if (tabbed()) {
		_parent_notebook->remove_page (_contents);
		StateChange (*this);
	}
}

void
Tabbable::show_tab ()
{
	if (!window_visible() && _parent_notebook) {
		if (_contents.get_parent() == 0) {
			tab_requested_by_state = true;
			add_to_notebook (*_parent_notebook, _tab_title);
		}
		_parent_notebook->set_current_page (_parent_notebook->page_num (_contents));
		current_toplevel()->present ();
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

bool
Tabbable::tabbed_by_default() const
{
	return tab_requested_by_state;
}

XMLNode&
Tabbable::get_state()
{
	XMLNode& node (WindowProxy::get_state());

	node.set_property (X_("tabbed"),  tabbed());

	return node;
}

int
Tabbable::set_state (const XMLNode& node, int version)
{
	int ret;

	if ((ret = WindowProxy::set_state (node, version)) != 0) {
		return ret;
	}

	if (_visible) {
		show_own_window (true);
	}

	XMLNodeList children = node.children ();
	XMLNode* window_node = node.child ("Window");

	if (window_node) {
		window_node->get_property (X_("tabbed"), tab_requested_by_state);
	}

	if (!_visible) {
		if (tab_requested_by_state) {
			attach ();
		} else {
			/* this does nothing if not tabbed */
			hide_tab ();
		}
	}

	return ret;
}

void
Tabbable::window_mapped ()
{
	StateChange (*this);
}

void
Tabbable::window_unmapped ()
{
	StateChange (*this);
}
