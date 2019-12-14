/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_message_h_
#define _ardour_message_h_

#include <gtkmm/messagedialog.h>

class ArdourMessageDialog : public Gtk::MessageDialog
{
public:
	ArdourMessageDialog (const Glib::ustring& message,
	                     bool use_markup = false,
	                     Gtk::MessageType type =  Gtk::MESSAGE_INFO,
	                     Gtk::ButtonsType buttons =  Gtk::BUTTONS_OK,
	                     bool modal = false);

	ArdourMessageDialog (Gtk::Window& parent,
	                     const Glib::ustring& message,
	                     bool use_markup = false,
	                     Gtk::MessageType type =  Gtk::MESSAGE_INFO,
	                     Gtk::ButtonsType buttons = Gtk::BUTTONS_OK,
	                     bool modal = false);

	virtual ~ArdourMessageDialog ();

	int run ();
	void show ();
	void hide ();

protected:
	void pop_splash ();
	void push_splash ();

	bool _splash_pushed;
};

#endif
