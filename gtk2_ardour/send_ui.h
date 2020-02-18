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

#include "gain_meter.h"
#include "panner_ui.h"
#include "ardour_window.h"

namespace ARDOUR {
	class Send;
	class IOProcessor;
}

class IOSelector;

class SendUI : public Gtk::HBox
{
  public:
	SendUI (Gtk::Window *, boost::shared_ptr<ARDOUR::Send>, ARDOUR::Session*);
	~SendUI();

	void update ();
	void fast_update ();

	boost::shared_ptr<ARDOUR::Send>& send() { return _send; }

  private:
	boost::shared_ptr<ARDOUR::Send> _send;
	GainMeter                       _gpm;
	PannerUI                        _panners;
	Gtk::VBox                       _vbox;
	Gtk::VBox                       _hbox;
	IOSelector*                     _io;

	sigc::connection screen_update_connection;
	sigc::connection fast_screen_update_connection;

	void outs_changed (ARDOUR::IOChange, void*);
	PBD::ScopedConnectionList connections;
};

class SendUIWindow : public ArdourWindow
{
  public:
	SendUIWindow(boost::shared_ptr<ARDOUR::Send>, ARDOUR::Session*);
	~SendUIWindow();

	SendUI* ui;

  private:
	Gtk::HBox hpacker;

	PBD::ScopedConnection going_away_connection;
};

#endif /* __ardour_gtk_send_ui_h__ */


