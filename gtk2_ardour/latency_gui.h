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

#ifndef __gtk2_ardour_latency_gui_h__
#define __gtk2_ardour_latency_gui_h__

#include <vector>
#include <string>

#include <gtkmm/dialog.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/adjustment.h>

#include <gtkmm2ext/barcontroller.h>
#include "pbd/controllable.h"

#include "ardour/types.h"

#include "ardour_dialog.h"

namespace ARDOUR {
	class Latent;
}

class LatencyGUI;

class LatencyBarController : public Gtkmm2ext::BarController
{
public:
	LatencyBarController (Gtk::Adjustment& adj, LatencyGUI* g)
		: BarController (adj, boost::shared_ptr<PBD::IgnorableControllable> (new PBD::IgnorableControllable ())),
		  _latency_gui (g) {}

private:
	LatencyGUI* _latency_gui;

	std::string get_label (double&);
};

class LatencyGUI : public Gtk::VBox
{
  public:
	LatencyGUI (ARDOUR::Latent&, framepos_t sample_rate, framepos_t period_size);
	~LatencyGUI() { }

	void finish ();
	void reset ();
	void refresh ();

  private:
	ARDOUR::Latent& _latent;
	framepos_t initial_value;
	framepos_t sample_rate;
	framepos_t period_size;
	boost::shared_ptr<PBD::IgnorableControllable> ignored;

	Gtk::Adjustment adjustment;
	LatencyBarController bc;
	Gtk::HBox hbox1;
	Gtk::HBox hbox2;
	Gtk::HButtonBox hbbox;
	Gtk::Button minus_button;
	Gtk::Button plus_button;
	Gtk::Button reset_button;
	Gtk::ComboBoxText units_combo;

	void change_latency_from_button (int dir);

	friend class LatencyBarController;

	static std::vector<std::string> unit_strings;
};

class LatencyDialog : public ArdourDialog
{
  public:
	LatencyDialog (const std::string& title, ARDOUR::Latent&, framepos_t sample_rate, framepos_t period_size);
	~LatencyDialog() {}

  private:
	LatencyGUI lwidget;
};

#endif /* __gtk2_ardour_latency_gui_h__ */
