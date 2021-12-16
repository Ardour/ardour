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

#include "audio_clock.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor.h"
#include "region_view.h"

#include "audio_region_properties_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;
using std::max;
using std::min;

RegionPropertiesBox::RegionPropertiesBox ()
	: length_clock (X_("regionlength"), true, "", true, false, true)
	, start_clock (X_("regionstart"), true, "", false, false)
	, bbt_toggle (ArdourButton::led_default_elements)
{
	Gtk::Label* label;
	int         row = 0;

	_header_label.set_alignment (0.0, 0.5);
	pack_start (_header_label, false, false, 6);

	Gtk::Table* bpm_table = manage (new Gtk::Table ());
	bpm_table->set_homogeneous (false);
	bpm_table->set_spacings (4);
	bpm_table->set_border_width (2);
	label = manage (new Gtk::Label (_("BPM:")));
	label->set_alignment (1.0, 0.5);
	bpm_table->attach (*label,     0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	bpm_table->attach (bpm_button, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	pack_start (*bpm_table, false, false);

	Gtk::Table* metrum_table = manage (new Gtk::Table ());
	metrum_table->set_homogeneous (false);
	metrum_table->set_spacings (4);
	metrum_table->set_border_width (2);
	label = manage (new Gtk::Label (_("Time Sig:")));
	label->set_alignment (1.0, 0.5);
	bpm_table->attach (*label,        0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	bpm_table->attach (metrum_button, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	pack_start (*metrum_table, false, false);

	row = 0;

	bbt_toggle.set_text (_("Stretch"));
	table.attach (bbt_toggle, 0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	label = manage (new Gtk::Label (_("Start:")));
	label->set_alignment (1.0, 0.5);
	table.attach (*label, 0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	table.attach (start_clock, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	label = manage (new Gtk::Label (_("Length:")));
	label->set_alignment (1.0, 0.5);
	table.attach (*label, 0, 1, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	table.attach (length_clock, 1, 2, row, row + 1, Gtk::SHRINK, Gtk::SHRINK);
	row++;

	table.set_homogeneous (false);
	table.set_spacings (4);
	table.set_border_width (2);
	pack_start (table, false, false);
}

RegionPropertiesBox::~RegionPropertiesBox ()
{
}

void
RegionPropertiesBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	length_clock.set_session (s);
	start_clock.set_session (s);
}

void
RegionPropertiesBox::set_region (boost::shared_ptr<Region> r)
{
	set_session (&r->session ());

	state_connection.disconnect ();

	_region = r;

	PBD::PropertyChange interesting_stuff;
	region_changed (interesting_stuff);

	_region->PropertyChanged.connect (state_connection, invalidator (*this), boost::bind (&RegionPropertiesBox::region_changed, this, _1), gui_context ());
}

void
RegionPropertiesBox::region_changed (const PBD::PropertyChange& what_changed)
{
	// TODO: refactor the region_editor.cc to cover this basic stuff
	{
		AudioClock::Mode mode = _region->position_time_domain () == Temporal::AudioTime ? AudioClock::Samples : AudioClock::BBT;

		start_clock.set_mode (mode);
		length_clock.set_mode (mode);

		start_clock.set (_region->start ());
		length_clock.set_duration (_region->length ());

		bpm_button.set_text ("122.2");
		metrum_button.set_text ("4/4");
	}
}

AudioRegionPropertiesBox::AudioRegionPropertiesBox ()
{
	_header_label.set_text (_("AUDIO Region Properties:"));

	Gtk::Label* label;

	Gtk::Table* audio_t = manage (new Gtk::Table ());
	audio_t->set_homogeneous (true);
	audio_t->set_spacings (4);

	int row = 0;

	label = manage (new Gtk::Label (_("Fades:")));
	label->set_alignment (1.0, 0.5);
	fade_in_enable_button.set_text (_("In"));
	fade_in_enable_button.set_name ("generic button");
	fade_out_enable_button.set_text (_("Out"));
	fade_out_enable_button.set_name ("generic button");
	audio_t->attach (*label,                 0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	audio_t->attach (fade_in_enable_button,  1, 2, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	audio_t->attach (fade_out_enable_button, 2, 3, row, row + 1, Gtk::FILL, Gtk::SHRINK);

	row++;

	label = manage (new Gtk::Label (_("Gain:")));
	label->set_alignment (1.0, 0.5);
	audio_t->attach (*label, 0, 1, row, row + 1, Gtk::FILL, Gtk::SHRINK);

	gain_control.set_text (_("+6dB"));
	gain_control.set_name ("generic button");
	audio_t->attach (gain_control, 1, 3, row, row + 1, Gtk::FILL, Gtk::SHRINK);

	row++;

	pack_start (*audio_t);
}

AudioRegionPropertiesBox::~AudioRegionPropertiesBox ()
{
}

void
AudioRegionPropertiesBox::set_region (boost::shared_ptr<Region> r)
{
	RegionPropertiesBox::set_region (r);
}
