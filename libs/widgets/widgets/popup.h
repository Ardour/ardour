/*
 * Copyright (C) 1998 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef _WIDGETS_POPUP_H_
#define _WIDGETS_POPUP_H_

#ifdef interface
#undef interface
#endif

#include <string>

#include <gtkmm/label.h>
#include <gtkmm/window.h>

#include <pbd/touchable.h>

#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API PopUp : public Gtk::Window, public Touchable
{
public:
	PopUp (Gtk::WindowPosition pos, unsigned int show_for_msecs = 0,
	       bool delete_on_hide = false);
	virtual ~PopUp ();
	void touch ();
	void remove ();
	void set_text (std::string);
	void set_name (std::string);
	gint button_click (GdkEventButton *);

	bool on_delete_event (GdkEventAny* );

protected:
	void on_realize ();

private:
	Gtk::Label label;
	std::string my_text;
	gint timeout;
	static gint remove_prompt_timeout (void *);
	bool delete_on_hide;
	unsigned int popdown_time;
};

} /* namespace */

#endif
