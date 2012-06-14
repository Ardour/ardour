/*
    Copyright (C) 2012 Paul Davis

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

#ifndef gtkmm2ext_persistent_tooltip_h
#define gtkmm2ext_persistent_tooltip_h

#include <sigc++/trackable.h>

namespace Gtkmm2ext {

/** A class which offers a tooltip-like window which can be made to
 *  stay open during a drag.
 */
class PersistentTooltip : public sigc::trackable
{
public:
	PersistentTooltip (Gtk::Widget *);
	virtual ~PersistentTooltip ();
	
	void set_tip (std::string);

	virtual bool dragging () const;

private:
	bool timeout ();
	void show ();
	void hide ();
	bool enter (GdkEventCrossing *);
	bool leave (GdkEventCrossing *);
	bool press (GdkEventButton *);
	bool release (GdkEventButton *);

	/** The widget that we are providing a tooltip for */
	Gtk::Widget* _target;
	/** Our window */
	Gtk::Window* _window;
	/** Our label */
	Gtk::Label* _label;
	/** true if we are `dragging', in the sense that button 1
	    is being held over _target.
	*/
	bool _maybe_dragging;
	/** Connection to a timeout used to open the tooltip */
	sigc::connection _timeout;
	/** The tip text */
	std::string _tip;
};

}

#endif
