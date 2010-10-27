/*
    Copyright (C) 2001 Paul Davis

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

#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"

#include "ardour/session.h"
#include "ardour/audioregion.h"
#include "ardour/playlist.h"
#include "ardour/utils.h"
#include "ardour/dB.h"
#include <gtkmm2ext/utils.h>
#include <cmath>

#include "audio_region_editor.h"
#include "audio_region_view.h"
#include "ardour_ui.h"
#include "utils.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace Gtkmm2ext;

AudioRegionEditor::AudioRegionEditor (Session* s, boost::shared_ptr<AudioRegion> r)
	: RegionEditor (s, r)
	, _audio_region (r)
	, gain_adjustment(accurate_coefficient_to_dB(_audio_region->scale_amplitude()), -40.0, +40.0, 0.1, 1.0, 0)	  

{
	gain_label.set_alignment (1, 0.5);

	Gtk::HBox* gb = Gtk::manage (new Gtk::HBox);
	gb->set_spacing (6);
	gb->pack_start (gain_entry);
	gb->pack_start (*Gtk::manage (new Gtk::Label (_("dB"))), false, false);

	gain_label.set_name ("AudioRegionEditorLabel");
	gain_label.set_text (_("Region gain:"));
	gain_entry.configure (gain_adjustment, 0.0, 1);
	_table.attach (gain_label, 0, 1, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);
	_table.attach (*gb, 1, 2, _table_row, _table_row + 1, Gtk::FILL, Gtk::FILL);

	gain_changed ();

	gain_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &AudioRegionEditor::gain_adjustment_changed));
}

void
AudioRegionEditor::region_changed (const PBD::PropertyChange& what_changed)
{
	RegionEditor::region_changed (what_changed);
	
	if (what_changed.contains (ARDOUR::Properties::scale_amplitude)) {
		gain_changed ();
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
