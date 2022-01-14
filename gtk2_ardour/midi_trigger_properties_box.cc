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

#include "midi_trigger_properties_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;
using std::max;
using std::min;

MidiTriggerPropertiesBox::MidiTriggerPropertiesBox ()
{
	Gtk::Table* midi_t = manage (new Gtk::Table ());
	midi_t->set_homogeneous (true);
	midi_t->set_spacings (4);

	int row = 0;

	_patch_enable_button.set_text (_("Send Patches"));
	_patch_enable_button.set_name ("generic button");

	midi_t->attach (_patch_enable_button, 0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	_cc_enable_button.set_text (_("Send CCs"));
	_cc_enable_button.set_name ("generic button");

	midi_t->attach (_cc_enable_button, 0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	attach (*midi_t, 0, 1, 0, 1, Gtk::SHRINK, Gtk::SHRINK);
}

MidiTriggerPropertiesBox::~MidiTriggerPropertiesBox ()
{
}

void
MidiTriggerPropertiesBox::on_trigger_changed (const PBD::PropertyChange& what_changed)
{
	/* CC and Pgm stuff ...? */
}
