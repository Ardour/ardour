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
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

#include "gtkmm2ext/cairo_icon.h"
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
	Tabbable (Gtk::Widget&, const std::string&);
	~Tabbable ();

	void add_to_notebook (Gtk::Notebook& notebook, const std::string& tab_title);
	void make_visible ();
	void make_invisible ();
	void attach ();
	void detach ();
	
	Gtk::Widget& contents() const { return _contents; }
	
	Gtk::Window* get (bool create = false);
	Gtk::Window* own_window () { return get (false); } 
	virtual Gtk::Window* use_own_window (bool and_pack_it);

	bool has_own_window () const;
	bool is_tabbed () const;

	void set_default_tabbed (bool yn);
	
	virtual void show_window ();

	bool window_visible ();
	bool tabbed() const;

	Gtk::Window* current_toplevel () const;

	Gtk::Notebook* tab_root_drop ();

	int set_state (const XMLNode&, int version);
	XMLNode& get_state ();
	
	static std::string xml_node_name();

	sigc::signal1<void,Tabbable&> StateChange;
	
  protected:
	bool delete_event_handler (GdkEventAny *ev);
	
  private:
	Gtk::Widget&   _contents;
	Gtk::Notebook  _own_notebook;
	Gtk::Notebook* _parent_notebook;
	std::string    _tab_title;
	Gtk::HBox      _tab_box;
	Gtk::Label     _tab_label;
	Gtk::Button    _tab_close_button;
	CairoIcon       tab_close_image;
	bool            tab_requested_by_state;
	
	void show_tab ();
	void hide_tab ();
	void tab_close_clicked ();
	void show_own_window (bool and_pack_it);
};


}

#endif
