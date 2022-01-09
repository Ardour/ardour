/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _WIDGETS_PANE_H_
#define _WIDGETS_PANE_H_

#include <vector>
#include <algorithm>
#include <boost/shared_ptr.hpp>

#include <stdint.h>

#include <gdkmm/cursor.h>
#include <gtkmm/container.h>
#include <gtkmm/eventbox.h>

#include "widgets/visibility.h"

namespace Gtk {
	class Widget;
}

namespace ArdourWidgets {

class LIBWIDGETS_API Pane : public Gtk::Container
{
private:
	struct Divider;

public:
	struct Child
	{
		Pane* pane;
		Gtk::Widget* w;
		int32_t minsize;
		sigc::connection show_con;
		sigc::connection hide_con;

		Child (Pane* p, Gtk::Widget* widget, uint32_t ms) : pane (p), w (widget), minsize (ms) {}
	};

	typedef std::vector<boost::shared_ptr<Child> > Children;

	Pane (bool horizontal);
	~Pane();

	void set_divider (std::vector<float>::size_type divider, float fract);
	float get_divider (std::vector<float>::size_type divider = 0);
	void set_child_minsize (Gtk::Widget const &, int32_t);

	GType child_type_vfunc() const;
	void set_drag_cursor (Gdk::Cursor);

	void set_check_divider_position (bool);

protected:
	bool horizontal;

	void on_add (Gtk::Widget*);
	void on_remove (Gtk::Widget*);
	void on_size_request (GtkRequisition*);
	void on_size_allocate (Gtk::Allocation&);
	bool on_expose_event (GdkEventExpose*);

	bool handle_press_event (GdkEventButton*, Divider*);
	bool handle_release_event (GdkEventButton*, Divider*);
	bool handle_motion_event (GdkEventMotion*, Divider*);
	bool handle_enter_event (GdkEventCrossing*, Divider*);
	bool handle_leave_event (GdkEventCrossing*, Divider*);

	void forall_vfunc (gboolean include_internals, GtkCallback callback, gpointer callback_data);

private:
	Gdk::Cursor drag_cursor;
	bool did_move;

	void reallocate (Gtk::Allocation const &);

	Children children;

	struct Divider : public Gtk::EventBox {
		Divider ();

		float fract;
		bool dragging;

		bool on_expose_event (GdkEventExpose* ev);
	};

	typedef std::list<Divider*> Dividers;
	Dividers dividers;
	int divider_width;
	bool check_fract;

	void add_divider ();
	void handle_child_visibility ();
	float constrain_fract (Dividers::size_type, float fract);

	static void* notify_child_destroyed (void*);
	void* child_destroyed (Gtk::Widget*);
};

class LIBWIDGETS_API HPane : public Pane
{
  public:
	HPane () : Pane (true) {}
};

class LIBWIDGETS_API VPane : public Pane
{
  public:
	VPane () : Pane (false) {}
};

} /* namespace */

#endif
