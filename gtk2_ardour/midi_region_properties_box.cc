/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
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

#include "pbd/compose.h"
#include <algorithm>

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour/location.h"
#include "ardour/profile.h"
#include "ardour/session.h"

#include "widgets/ardour_button.h"

#include "audio_clock.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor.h"
#include "region_view.h"

#include "midi_region_properties_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;
using std::max;
using std::min;

MidiRegionPropertiesBox::MidiRegionPropertiesBox ()
{
	_header_label.set_text (_("MIDI Region Properties:"));

	Gtk::Table* midi_t = manage (new Gtk::Table ());
	midi_t->set_homogeneous (true);
	midi_t->set_spacings (4);

	int row = 0;

	patch_selector_button.set_text (_("Patches..."));
	patch_selector_button.set_name ("generic button");

	midi_t->attach (patch_selector_button, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	cc_selector_button.set_text (_("CCs..."));
	cc_selector_button.set_name ("generic button");

	midi_t->attach (cc_selector_button, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	pack_start (*midi_t);
}

MidiRegionPropertiesBox::~MidiRegionPropertiesBox ()
{
}

void
MidiRegionPropertiesBox::set_region (boost::shared_ptr<Region> r)
{
	RegionPropertiesBox::set_region (r);

	_region->PropertyChanged.connect (midi_state_connection, invalidator (*this), boost::bind (&MidiRegionPropertiesBox::region_changed, this, _1), gui_context ());
}

void
MidiRegionPropertiesBox::region_changed (const PBD::PropertyChange& what_changed)
{
	/* CC and Pgm stuff ...? */
}
