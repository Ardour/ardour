/*
 * Copyright (C) 1999-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __pbd_gtkmm_textviewer_h__
#define __pbd_gtkmm_textviewer_h__

#include <string>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/textview.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/window.h>

#include "pbd/transmitter.h"

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API TextViewer : public Gtk::Window, public Transmitter
{
	Gtk::TextView etext;
	Gtk::VBox vbox1;
	Gtk::VBox vbox2;
	Gtk::ScrolledWindow scrollwin;
	Gtk::Button dismiss;
	bool _editable;

	void toggle_edit ();
	void toggle_word_wrap ();
	void signal_released_handler ();

 public:
	TextViewer (size_t width, size_t height);
	Gtk::TextView& text()         { return etext; }
	Gtk::Button& dismiss_button() { return dismiss; }

	void scroll_to_bottom ();

	void deliver ();
};

} /* namespace */

#endif  // __pbd_gtkmm_textviewer_h__
