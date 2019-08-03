/*
 * Copyright (C) 2013-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#ifndef gtkmm2ext_persistent_tooltip_h
#define gtkmm2ext_persistent_tooltip_h

#include <sigc++/trackable.h>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

/** A class which offers a tooltip-like window which can be made to
 *  stay open during a drag.
 */
class LIBGTKMM2EXT_API PersistentTooltip : public sigc::trackable
{
  public:
	PersistentTooltip (Gtk::Widget *, bool draggable = false, int margin_y = 0);
	virtual ~PersistentTooltip ();

	void set_tip (std::string);
	void set_font (Pango::FontDescription font);
	void set_center_alignment (bool align_to_center);

	virtual bool dragging () const;
	static void set_tooltips_enabled (bool en) { _tooltips_enabled = en; }
	static bool tooltips_enabled () { return _tooltips_enabled; }

  private:
	static bool _tooltips_enabled;
	static unsigned int _tooltip_timeout;
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

	/** allow to drag
	 */
	bool _draggable;
	/** true if we are `dragging', in the sense that button 1
	    is being held over _target.
	*/
	bool _maybe_dragging;
	/** Connection to a timeout used to open the tooltip */
	sigc::connection _timeout;
	/** The tip text */
	std::string _tip;
	Pango::FontDescription _font;
	bool _align_to_center;
	int _margin_y;
};

}

#endif
