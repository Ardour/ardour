/*
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk2_ardour_latency_gui_h__
#define __gtk2_ardour_latency_gui_h__

#include <vector>
#include <string>

#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/comboboxtext.h>

#include "pbd/controllable.h"
#include "ardour/types.h"
#include "widgets/barcontroller.h"

#include "ardour_dialog.h"

namespace ARDOUR {
	class Latent;
}

class LatencyGUI;

class LatencyGUIControllable : public PBD::Controllable
{
public:
	LatencyGUIControllable (LatencyGUI* g)
		: PBD::Controllable ("ignoreMe")
		, _latency_gui (g)
	{}

	void set_value (double v, PBD::Controllable::GroupControlDisposition group_override);
	double get_value () const;
	double lower() const;
  double upper() const;
	double internal_to_interface (double i, bool rotary = false) const {
		return i;
	}
	double interface_to_internal (double i, bool rotary = false) const {
		return i;
	}

private:
	LatencyGUI* _latency_gui;
};

class LatencyBarController : public ArdourWidgets::BarController
{
public:
	LatencyBarController (Gtk::Adjustment& adj, LatencyGUI* g)
		: BarController (adj, boost::shared_ptr<PBD::Controllable> (new LatencyGUIControllable (g)))
		, _latency_gui (g)
	{
		set_digits (0);
	}

private:
	LatencyGUI* _latency_gui;

	std::string get_label (double&);
};

class LatencyGUI : public Gtk::VBox
{
public:
	LatencyGUI (ARDOUR::Latent&, samplepos_t sample_rate, samplepos_t period_size);
	~LatencyGUI() { }

	void refresh ();

private:
	void reset ();
	void finish ();

	ARDOUR::Latent& _latent;
	samplepos_t sample_rate;
	samplepos_t period_size;

	bool _ignore_change;
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
	friend class LatencyGUIControllable;

	static std::vector<std::string> unit_strings;
};

#endif /* __gtk2_ardour_latency_gui_h__ */
