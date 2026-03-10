/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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


#pragma once

#include "ytkmm/adjustment.h"
#include "ytkmm/colorselection.h"

#include "ardour_dialog.h"

namespace Gtk {
	class ColorButton;
}

class PitchColorDialog : public ArdourDialog
{
public:
	PitchColorDialog();
	sigc::signal<void> ColorsChanged;

private:
	Gtk::ColorSelection color_dialog;
	std::vector<uint32_t> colors;
	Gtk::VBox* pitch_vpacker;
	Gtk::Adjustment cycle_adjust;

	void refill ();
	void color_chosen (int n, Gtk::ColorButton*);
};


