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

#ifndef __gtkmm2ext_tabbable_h__
#define __gtkmm2ext_tabbable_h__

#include <gtkmm/bin.h>

#include "gtkmm2ext/window_proxy.h"
#include "gtkmm2ext/visibility.h"

namespace Gtk {
	class Window;
	class Notebook;
}

namespace Gtkmm2ext {

class VisibilityTracker;

class LIBGTKMM2EXT_API Tabbable : public WindowProxy {
  public:
	Tabbable (Gtk::Widget&);
	~Tabbable ();

	void add_to_notebook (Gtk::Notebook& notebook, const std::string& tab_title, int position);

	Gtk::Window* get (bool create = false);
	Gtk::Window* own_window () { return get (false); } 
	Gtk::Notebook* tabbed_parent ();
	virtual Gtk::Window* use_own_window ();

	bool has_own_window () const;
	bool is_tabbed () const;

	virtual void show_window ();

	bool window_visible ();
	
  protected:
	bool delete_event_handler (GdkEventAny *ev);
	
  private:
	Gtk::Widget&   _contents;
	Gtk::Notebook* _parent_notebook;
	std::string    _tab_title;
	int            _notebook_position;
};

}

#endif
