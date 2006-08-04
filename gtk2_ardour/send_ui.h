/*
    Copyright (C) 2002 Paul Davis 

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

#ifndef __ardour_gtk_send_ui_h__
#define __ardour_gtk_send_ui_h__

#include "gain_meter.h"
#include "panner_ui.h"

namespace ARDOUR {
	class Send;
	class Session;
	class Redirect;
}

class IOSelector;

class SendUI : public Gtk::HBox
{
  public:
	SendUI (boost::shared_ptr<ARDOUR::Send>, ARDOUR::Session&);
	~SendUI();

	void update ();
	void fast_update ();

	IOSelector* io;

  private:
	boost::shared_ptr<ARDOUR::Send> _send;
	ARDOUR::Session& _session;
	GainMeter gpm;
	PannerUI  panners;
	Gtk::VBox vbox;
	Gtk::VBox hbox;

	sigc::connection screen_update_connection;
	sigc::connection fast_screen_update_connection;
		
	void send_going_away (ARDOUR::Redirect*);
	void ins_changed (ARDOUR::IOChange, void*);
	void outs_changed (ARDOUR::IOChange, void*);
};

class SendUIWindow : public Gtk::Window
{
  public:
	SendUIWindow(boost::shared_ptr<ARDOUR::Send>, ARDOUR::Session&);
	~SendUIWindow();

	SendUI*     ui;

  private:
	Gtk::VBox vpacker;
	Gtk::HBox hpacker;

	void send_going_away (ARDOUR::Redirect*);
};

#endif /* __ardour_gtk_send_ui_h__ */


