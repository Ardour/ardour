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

    $Id$
*/

#ifndef __ardour_gtk_axis_view_h__
#define __ardour_gtk_axis_view_h__

#include <list>
#include <gtkmm.h>
#include <pbd/xml++.h>
#include "prompter.h"

namespace ARDOUR {
	class Session;
}

/**
 * AxisView defines the abstract base class for time-axis trackviews and routes.
 *
 */
class AxisView : public virtual sigc::trackable
{
  public:
	/**
	 * Returns the current 'Track' Color
	 *
	 * @return the current Track Color
	 */
	Gdk::Color color() const { return _color; }

	ARDOUR::Session& session() const { return _session; }

	virtual string name() const = 0;

	virtual void set_selected (bool yn) {
		if (yn != _selected) {
			_selected = yn;
		}
	}
	
	virtual bool marked_for_display() const { return _marked_for_display; }

	virtual void set_marked_for_display (bool yn) {
		if (yn != _marked_for_display) {
			_marked_for_display = yn;
		}
	}
	
	virtual bool selected() const { return _selected; }
	sigc::signal<void> Hiding;
	sigc::signal<void> GoingAway;

  protected:

	AxisView (ARDOUR::Session& sess);
	virtual ~AxisView();
	

	/**
	 * Generate a new random TrackView color, unique from those colors already used.
	 *
	 * @return the unique random color.
	 */
	static Gdk::Color unique_random_color();


	ARDOUR::Session& _session;
	Gdk::Color _color;

	static list<Gdk::Color> used_colors;

	Gtk::Label name_label;

	bool _selected;

	bool _marked_for_display;
	
}; /* class AxisView */

#endif /* __ardour_gtk_axis_view_h__ */

