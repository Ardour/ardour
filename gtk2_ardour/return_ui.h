/*
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_gtk_return_ui_h__
#define __ardour_gtk_return_ui_h__

#include "gain_meter.h"
#include "panner_ui.h"
#include "ardour_window.h"

namespace ARDOUR {
	class Return;
	class IOProcessor;
}

class IOSelector;

class ReturnUI : public Gtk::HBox
{
public:
	ReturnUI (Gtk::Window *,boost::shared_ptr<ARDOUR::Return>, ARDOUR::Session*);
	~ReturnUI();

	void update ();
	void fast_update ();

	IOSelector* io;

	boost::shared_ptr<ARDOUR::Return>& retrn() { return _return; }

private:
	boost::shared_ptr<ARDOUR::Return> _return;
	GainMeter                         _gpm;
	Gtk::VBox                         _vbox;
	Gtk::VBox                         _hbox;

	sigc::connection screen_update_connection;
	sigc::connection fast_screen_update_connection;
	PBD::ScopedConnection input_change_connection;

	void ins_changed (ARDOUR::IOChange, void*);
};

class ReturnUIWindow : public ArdourWindow
{
  public:
	ReturnUIWindow(boost::shared_ptr<ARDOUR::Return>, ARDOUR::Session*);
	~ReturnUIWindow();

	ReturnUI* ui;

  private:
	Gtk::HBox hpacker;

	PBD::ScopedConnection going_away_connection;
};

#endif /* __ardour_gtk_return_ui_h__ */

