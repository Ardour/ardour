/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2006-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2016 Robin Gareus <robin@gareus.org>
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

#include <cmath>

#include <gtkmm2ext/utils.h>

#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"
#include "pbd/pthread_utils.h"

#include "ardour/audioregion.h"
#include "ardour/session_event.h"
#include "ardour/dB.h"

#include "audio_region_editor.h"
#include "audio_region_view.h"
#include "gui_thread.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace Gtkmm2ext;

static void *
_peak_amplitude_thread (void* arg)
{
	static_cast<AudioRegionEditor*>(arg)->peak_amplitude_thread ();
	return 0;
}

AudioRegionEditor::AudioRegionEditor (Session* s, boost::shared_ptr<AudioRegion> r)
	: RegionEditor (s, r)
	, _audio_region (r)
	, gain_adjustment(accurate_coefficient_to_dB(_audio_region->scale_amplitude()), -40.0, +40.0, 0.1, 1.0, 0)
	, _peak_channel (false)
{

	Gtk::HBox* b = Gtk::manage (new Gtk::HBox);
	b->set_spacing (6);
	b->pack_start (gain_entry);
	b->pack_start (*Gtk::manage (new Gtk::Label (_("dB"))), false, false);

	gain_label.set_name ("AudioRegionEditorLabel");
	gain_label.set_text (_("Region gain:"));
	gain_label.set_alignment (1, 0.5);
	gain_entry.configure (gain_adjustment, 0.0, 1);
	_table.attach (gain_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (*b, 1, 2, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	++_table_row;

	b = Gtk::manage (new Gtk::HBox);
	b->set_spacing (6);
	b->pack_start (_peak_amplitude);
	b->pack_start (*Gtk::manage (new Gtk::Label (_("dBFS"))), false, false);

	_peak_amplitude_label.set_name ("AudioRegionEditorLabel");
	_peak_amplitude_label.set_text (_("Peak amplitude:"));
	_peak_amplitude_label.set_alignment (1, 0.5);
	_table.attach (_peak_amplitude_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (*b, 1, 2, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	++_table_row;

	gain_changed ();

	gain_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &AudioRegionEditor::gain_adjustment_changed));

	_peak_amplitude.property_editable() = false;
	_peak_amplitude.set_text (_("Calculating..."));

	PeakAmplitudeFound.connect (_peak_amplitude_connection, invalidator (*this), boost::bind (&AudioRegionEditor::peak_amplitude_found, this, _1), gui_context ());

	char name[64];
	snprintf (name, 64, "peak amplitude-%p", this);
	pthread_create_and_store (name, &_peak_amplitude_thread_handle, _peak_amplitude_thread, this);
	signal_peak_thread ();
}

AudioRegionEditor::~AudioRegionEditor ()
{
	void* v;
	_peak_channel.deliver ('t');
	pthread_join (_peak_amplitude_thread_handle, &v);
}

void
AudioRegionEditor::region_changed (const PBD::PropertyChange& what_changed)
{
	RegionEditor::region_changed (what_changed);

	if (what_changed.contains (ARDOUR::Properties::scale_amplitude)) {
		gain_changed ();
	}

	if (what_changed.contains (ARDOUR::Properties::start) || what_changed.contains (ARDOUR::Properties::length)) {
		/* ask the peak thread to run again */
		signal_peak_thread ();
	}
}
void
AudioRegionEditor::gain_changed ()
{
	float const region_gain_dB = accurate_coefficient_to_dB (_audio_region->scale_amplitude());
	if (region_gain_dB != gain_adjustment.get_value()) {
		gain_adjustment.set_value(region_gain_dB);
	}
}

void
AudioRegionEditor::gain_adjustment_changed ()
{
	float const gain = dB_to_coefficient (gain_adjustment.get_value());
	if (_audio_region->scale_amplitude() != gain) {
		_audio_region->set_scale_amplitude (gain);
	}
}

void
AudioRegionEditor::signal_peak_thread ()
{
	_peak_channel.deliver ('c');
}

void
AudioRegionEditor::peak_amplitude_thread ()
{
	while (1) {
		char msg;
		/* await instructions to run */
		_peak_channel.receive (msg);

		if (msg == 't') {
			break;
		}

		/* compute peak amplitude and signal the fact */
		PeakAmplitudeFound (accurate_coefficient_to_dB (_audio_region->maximum_amplitude ())); /* EMIT SIGNAL */
	}
}

void
AudioRegionEditor::peak_amplitude_found (double p)
{
	stringstream s;
	s.precision (2);
	s.setf (ios::fixed, ios::floatfield);
	s << p;
	_peak_amplitude.set_text (s.str ());
}

