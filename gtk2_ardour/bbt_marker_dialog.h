/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_gtk_bbt_marker_dialog_h__
#define __ardour_gtk_bbt_marker_dialog_h__

#include <gtkmm/box.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/spinbutton.h>

#include "temporal/tempo.h"
#include "temporal/types.h"

#include "ardour_dialog.h"
#include "audio_clock.h"

class BBTMarkerDialog : public ArdourDialog
{
public:
	BBTMarkerDialog (Temporal::timepos_t const &, Temporal::BBT_Time const&);
	BBTMarkerDialog (Temporal::MusicTimePoint&);

	Temporal::timepos_t position() const;
	Temporal::BBT_Time  bbt_value () const;
	std::string         name() const;

private:
	void init (bool add);
	Temporal::MusicTimePoint* _point;
	Temporal::timepos_t       _position;
	Temporal::BBT_Time        _bbt;

	Gtk::HBox       bbt_box;
	Gtk::SpinButton bar_entry;
	Gtk::SpinButton beat_entry;
	Gtk::Label      bar_label;
	Gtk::Label      beat_label;

	Gtk::HBox       name_box;
	Gtk::Entry      name_entry;
	Gtk::Label      name_label;
};

#endif /* __ardour_gtk_bbt_marker_dialog_h__ */
