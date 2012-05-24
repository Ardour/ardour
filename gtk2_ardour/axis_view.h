/*
    Copyright (C) 2003 Paul Davis

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

#ifndef __ardour_gtk_axis_view_h__
#define __ardour_gtk_axis_view_h__

#include <list>

#include <gtkmm/label.h>
#include <gdkmm/color.h>

#include "pbd/xml++.h"
#include "pbd/signals.h"

#include "ardour/session_handle.h"

#include "gui_object.h"
#include "prompter.h"
#include "selectable.h"

namespace ARDOUR {
	class Session;
}

/**
 * AxisView defines the abstract base class for time-axis trackviews and routes.
 *
 */
class AxisView : public virtual Selectable, public PBD::ScopedConnectionList, public ARDOUR::SessionHandlePtr
{
  public:
	/** @return the track's own color */
	Gdk::Color color () const { return _color; }

	ARDOUR::Session* session() const { return _session; }

	virtual std::string name() const = 0;

	sigc::signal<void> Hiding;
	
	void set_old_order_key (uint32_t ok) { _old_order_key = ok; }
	uint32_t old_order_key() const { return _old_order_key; }

	virtual std::string state_id() const = 0;
	/* for now, we always return properties in string form.
	 */
	std::string gui_property (const std::string& property_name) const;
	
	template<typename T> void set_gui_property (const std::string& property_name, const T& value) {
		gui_object_state().set_property<T> (state_id(), property_name, value);
	}

	bool marked_for_display () const;
	virtual bool set_marked_for_display (bool);

	static GUIObjectState& gui_object_state();
	
  protected:

	AxisView (ARDOUR::Session* sess);
	virtual ~AxisView();

	/**
	 * Generate a new random TrackView color, unique from those colors already used.
	 *
	 * @return the unique random color.
	 */
	static Gdk::Color unique_random_color();


	Gdk::Color _color;

	static std::list<Gdk::Color> used_colors;

	Gtk::Label name_label;

	bool _marked_for_display;
	uint32_t _old_order_key;
}; /* class AxisView */

#endif /* __ardour_gtk_axis_view_h__ */

