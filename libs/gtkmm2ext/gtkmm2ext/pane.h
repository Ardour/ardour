/*
    Copyright (C) 2016 Paul Davis

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

#ifndef __libgtkmm2ext_pane_h__
#define __libgtkmm2ext_pane_h__

#include <vector>
#include <algorithm>

#include <stdint.h>

#include <gtkmm/container.h>
#include <gtkmm/eventbox.h>

#include "gtkmm2ext/visibility.h"

namespace Gtk {
	class Widget;
}

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API Pane : public Gtk::Container
{
  private:
	class Divider;

  public:
	Pane (bool horizontal);
	void set_divider (std::vector<float>::size_type divider, float fract);
	float get_divider (std::vector<float>::size_type divider = 0);

	GType child_type_vfunc() const;

  protected:
	bool horizontal;
	bool dragging;

	void on_add (Gtk::Widget*);
	void on_remove (Gtk::Widget*);
	void on_size_request (GtkRequisition*);
	void on_size_allocate (Gtk::Allocation&);
	bool on_expose_event (GdkEventExpose*);

	bool handle_press_event (GdkEventButton*, Divider*);
	bool handle_release_event (GdkEventButton*, Divider*);
	bool handle_motion_event (GdkEventMotion*, Divider*);

	void forall_vfunc (gboolean include_internals, GtkCallback callback, gpointer callback_data);

  private:
	void reallocate (Gtk::Allocation const &);

	typedef std::list<Gtk::Widget*> Children;
	Children children;

	struct Divider : public Gtk::EventBox {
		Divider ();
		float fract;
		bool on_expose_event (GdkEventExpose* ev);
	};

	typedef std::vector<Divider*> Dividers;
	Dividers dividers;
	int divider_width;
	void add_divider ();
};

class LIBGTKMM2EXT_API HPane : public Pane
{
  public:
	HPane () : Pane (true) {}
};

class LIBGTKMM2EXT_API VPane : public Pane
{
  public:
	VPane () : Pane (false) {}
};

} /* namespace */

#endif /* __libgtkmm2ext_pane_h__ */
