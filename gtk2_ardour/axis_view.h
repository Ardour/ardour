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
#include <boost/unordered_map.hpp>

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
	class Stripable;
	class PresentationInfo;
}

/**
 * AxisView defines the abstract base class for horizontal and vertical
 * presentations of Stripables.
 *
 */
class AxisView : public virtual PBD::ScopedConnectionList, public virtual ARDOUR::SessionHandlePtr, public virtual Selectable
{
  public:
	ARDOUR::Session* session() const { return _session; }

	virtual std::string name() const = 0;
	virtual Gdk::Color color() const = 0;

	sigc::signal<void> Hiding;

	virtual boost::shared_ptr<ARDOUR::Stripable> stripable() const = 0;

	virtual std::string state_id() const = 0;
	/* for now, we always return properties in string form.
	 */
	std::string gui_property (const std::string& property_name) const;

	bool get_gui_property (const std::string& property_name, std::string& value) const;

	template <typename T>
	bool get_gui_property (const std::string& property_name, T& value) const
	{
		std::string str = gui_property (property_name);

		if (!str.empty ()) {
			return PBD::string_to<T>(str, value);
		}
		return false;
	}

	void set_gui_property (const std::string& property_name, const std::string& value);

	void set_gui_property (const std::string& property_name, const char* value) {
		set_gui_property (property_name, std::string(value));
	}

	template <typename T>
	void set_gui_property (const std::string& property_name, const T& value)
	{
		set_gui_property (property_name, PBD::to_string(value));
	}

	void cleanup_gui_properties () {
		/* remove related property node from the GUI state */
		gui_object_state().remove_node (state_id());
		property_hashtable.clear ();
	}

	void set_selected (bool yn);

	virtual bool marked_for_display () const;
	virtual bool set_marked_for_display (bool);

	static GUIObjectState& gui_object_state();
	void clear_property_cache() { property_hashtable.clear(); }

	/**
	 * Generate a new random TrackView color, unique from those colors already used.
	 *
	 * @return the unique random color.
	 */
	static Gdk::Color unique_random_color();

  protected:
	AxisView ();
	virtual ~AxisView();

	static std::list<Gdk::Color> used_colors;

	Gtk::Label name_label;

	mutable boost::unordered_map<std::string, std::string> property_hashtable;
}; /* class AxisView */

#endif /* __ardour_gtk_axis_view_h__ */
