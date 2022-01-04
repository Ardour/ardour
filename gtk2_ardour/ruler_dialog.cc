/*
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/stock.h>

#include "actions.h"
#include "ruler_dialog.h"

#include "pbd/i18n.h"

RulerDialog::RulerDialog ()
	: ArdourDialog (_("Rulers"))
{
	add_button (Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);

	get_vbox()->pack_start (minsec_button);
	get_vbox()->pack_start (timecode_button);
	get_vbox()->pack_start (samples_button);
	get_vbox()->pack_start (bbt_button);
	get_vbox()->pack_start (meter_button);
	get_vbox()->pack_start (tempo_button);
	get_vbox()->pack_start (range_button);
	get_vbox()->pack_start (loop_punch_button);
	get_vbox()->pack_start (cdmark_button);
	get_vbox()->pack_start (cuemark_button);
	get_vbox()->pack_start (mark_button);
	get_vbox()->pack_start (video_button);

	get_vbox()->show_all ();

	connect_action (samples_button, "samples-ruler");
	connect_action (timecode_button, "timecode-ruler");
	connect_action (minsec_button, "minsec-ruler");
	connect_action (bbt_button, "bbt-ruler");
	connect_action (tempo_button, "tempo-ruler");
	connect_action (meter_button, "meter-ruler");
	connect_action (loop_punch_button, "loop-punch-ruler");
	connect_action (range_button, "range-ruler");
	connect_action (mark_button, "marker-ruler");
	connect_action (cdmark_button, "cd-marker-ruler");
	connect_action (cuemark_button, "cue-marker-ruler");
	connect_action (video_button, "video-ruler");
}

RulerDialog::~RulerDialog ()
{
}


void
RulerDialog::connect_action (Gtk::CheckButton& button, std::string const &action_name_part)
{
	std::string action_name = "toggle-";
	action_name += action_name_part;

	Glib::RefPtr<Gtk::Action> act = ActionManager::get_action ("Rulers", action_name.c_str());
	if (!act) {
		return;
	}

	Glib::RefPtr<Gtk::ToggleAction> tact = Glib::RefPtr<Gtk::ToggleAction>::cast_dynamic (act);
	if (!tact) {
		return;
	}

	tact->connect_proxy (button);
}
