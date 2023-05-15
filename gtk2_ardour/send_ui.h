/*
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2009 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
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

#ifndef __ardour_gtk_send_ui_h__
#define __ardour_gtk_send_ui_h__

#include "widgets/ardour_button.h"

#include "ardour_window.h"
#include "gain_meter.h"
#include "panner_ui.h"

namespace ARDOUR
{
	class Send;
	class IOProcessor;
}

class IOSelector;

class SendUI : public Gtk::HBox
{
public:
	SendUI (Gtk::Window*, ARDOUR::Session*, std::shared_ptr<ARDOUR::Send>);
	~SendUI ();

private:
	void fast_update ();
	void outs_changed (ARDOUR::IOChange, void*);

	bool invert_press (GdkEventButton* ev);
	bool invert_release (GdkEventButton* ev);

	std::shared_ptr<ARDOUR::Send> _send;

	ArdourWidgets::ArdourButton _invert_button;
	GainMeter                   _gpm;
	PannerUI                    _panners;
	Gtk::VBox                   _vbox;
	IOSelector*                 _io;

	sigc::connection      _fast_screen_update_connection;
	PBD::ScopedConnection _send_connection;
};

class SendUIWindow : public ArdourWindow
{
public:
	SendUIWindow (Gtk::Window&, ARDOUR::Session*, std::shared_ptr<ARDOUR::Send>);
	SendUIWindow (ARDOUR::Session*, std::shared_ptr<ARDOUR::Send>);

private:
	SendUI _ui;
};

#endif /* __ardour_gtk_send_ui_h__ */
