/*
    Copyright (C) 2017 Paul Davis

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

#include "pbd/i18n.h"

#include "ardour/beatbox.h"
#include "beatbox_gui.h"

using namespace ARDOUR;

BBGUI::BBGUI (boost::shared_ptr<BeatBox> bb)
	: ArdourDialog (_("BeatBox"))
	, bbox (bb)
	, quantize_off (quantize_group, "None")
	, quantize_32nd (quantize_group, "ThirtySecond")
	, quantize_16th (quantize_group, "Sixteenth")
	, quantize_8th (quantize_group, "Eighth")
	, quantize_quarter (quantize_group, "Quarter")
	, quantize_half (quantize_group, "Half")
	, quantize_whole (quantize_group, "Whole")
	, play_button ("Run")
	, clear_button ("Clear")
	, tempo_adjustment (bb->tempo(), 1, 300, 1, 10)
	, tempo_spinner (tempo_adjustment)
{
	quantize_off.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 0));
	quantize_32nd.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 32));
	quantize_16th.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 16));
	quantize_8th.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 8));
	quantize_quarter.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 4));
	quantize_half.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 2));
	quantize_whole.signal_toggled().connect (sigc::bind (sigc::mem_fun (*this, &BBGUI::set_quantize), 1));

	quantize_button_box.pack_start (quantize_off);
	quantize_button_box.pack_start (quantize_32nd);
	quantize_button_box.pack_start (quantize_16th);
	quantize_button_box.pack_start (quantize_8th);
	quantize_button_box.pack_start (quantize_quarter);
	quantize_button_box.pack_start (quantize_half);
	quantize_button_box.pack_start (quantize_whole);

	play_button.signal_toggled().connect (sigc::mem_fun (*this, &BBGUI::toggle_play));
	clear_button.signal_clicked().connect (sigc::mem_fun (*this, &BBGUI::clear));

	misc_button_box.pack_start (play_button);
	misc_button_box.pack_start (clear_button);

	tempo_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &BBGUI::tempo_changed));

	misc_button_box.pack_start (tempo_spinner);

	get_vbox()->pack_start (misc_button_box);
	get_vbox()->pack_start (quantize_button_box, true, true);

	show_all ();
}

BBGUI::~BBGUI ()
{
}

void
BBGUI::tempo_changed ()
{
	float t = tempo_adjustment.get_value();
	bbox->set_tempo (t);
}

void
BBGUI::set_quantize (int divisor)
{
	bbox->set_quantize (divisor);
}

void
BBGUI::clear ()
{
	bbox->clear ();
}

void
BBGUI::toggle_play ()
{
	if (bbox->running()) {
		bbox->stop ();
	} else {
		bbox->start ();
	}
}
