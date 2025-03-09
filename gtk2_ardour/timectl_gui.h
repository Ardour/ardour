/*
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2017-2024 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk2_ardour_timectl_gui_h__
#define __gtk2_ardour_timectl_gui_h__

#include <vector>
#include <string>

#include <ytkmm/adjustment.h>
#include <ytkmm/box.h>
#include <ytkmm/button.h>
#include <ytkmm/comboboxtext.h>

#include "pbd/controllable.h"
#include "ardour/types.h"
#include "widgets/barcontroller.h"

#include "ardour_dialog.h"

namespace ARDOUR {
	class Latent;
	class TailTime;
}

class TimeCtlGUI;

class TimeCtlGUIControllable : public PBD::Controllable
{
public:
	TimeCtlGUIControllable (TimeCtlGUI* g)
		: PBD::Controllable ("ignoreMe")
		, _timectl_gui (g)
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
	TimeCtlGUI* _timectl_gui;
};

class TimeCtlBarController : public ArdourWidgets::BarController
{
public:
	TimeCtlBarController (Gtk::Adjustment& adj, TimeCtlGUI* g)
		: BarController (adj, std::shared_ptr<PBD::Controllable> (new TimeCtlGUIControllable (g)))
		, _timectl_gui (g)
	{
		set_digits (0);
	}

private:
	TimeCtlGUI* _timectl_gui;

	std::string get_label (double&);
};

class TimeCtlGUI : public Gtk::VBox
{
public:
	TimeCtlGUI (ARDOUR::Latent&, samplepos_t sample_rate, samplepos_t period_size);
	TimeCtlGUI (ARDOUR::TailTime&, samplepos_t sample_rate, samplepos_t period_size);
	~TimeCtlGUI() { }

	void refresh ();

private:
	void init ();
	void reset ();
	void finish ();

	ARDOUR::Latent*   _latent;
	ARDOUR::TailTime* _tailtime;

	samplepos_t sample_rate;
	samplepos_t period_size;

	bool _ignore_change;
	Gtk::Adjustment adjustment;
	TimeCtlBarController bc;
	Gtk::HBox hbox1;
	Gtk::HBox hbox2;
	Gtk::HButtonBox hbbox;
	Gtk::Button minus_button;
	Gtk::Button plus_button;
	Gtk::Button reset_button;
	Gtk::ComboBoxText units_combo;

	void change_from_button (int dir);

	friend class TimeCtlBarController;
	friend class TimeCtlGUIControllable;

	static std::vector<std::string> unit_strings;
};

#endif /* __gtk2_ardour_timectl_gui_h__ */
