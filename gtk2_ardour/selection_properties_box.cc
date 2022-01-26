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

#include <algorithm>
#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/actions.h"

#include "ardour/location.h"
#include "ardour/profile.h"
#include "ardour/session.h"

#include "audio_clock.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor.h"
#include "region_view.h"
#include "time_info_box.h"
#include "triggerbox_ui.h"

#include "multi_region_properties_box.h"

#include "audio_region_properties_box.h"
#include "midi_region_properties_box.h"

#include "audio_region_operations_box.h"
#include "midi_region_operations_box.h"

#include "slot_properties_box.h"

#include "selection_properties_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using std::min;
using std::max;

SelectionPropertiesBox::SelectionPropertiesBox ()
{
	_header_label.set_text(_("Selection Properties (ESC = Deselect All)"));
	_header_label.set_alignment(0.0, 0.5);
	pack_start(_header_label, false, false, 6);

	/* Time Info, for Range selections  ToDo:  range operations*/
	_time_info_box = new TimeInfoBox ("EditorTimeInfo", true);
	pack_start(*_time_info_box, false, false, 0);

#if SELECTION_PROPERTIES_BOX_TODO
	/* Region ops (mute/unmute), for multiple-Region selections */
	_mregions_prop_box = new MultiRegionPropertiesBox ();
	pack_start(*_mregions_prop_box, false, false, 0);


	/* MIDI Region props, for Clips */
	_midi_prop_box = new MidiRegionPropertiesBox ();
	pack_start(*_midi_prop_box, false, false, 0);

	/* AUDIO Region props for Clips */
	_audio_prop_box = new AudioRegionPropertiesBox ();
	pack_start(*_audio_prop_box, false, false, 0);


	/* MIDI Region ops (transpose, quantize), for only-midi selections */
	_midi_ops_box = new MidiRegionOperationsBox ();
	pack_start(*_midi_ops_box, false, false, 0);

	/* AUDIO Region ops (reverse, normalize), for only-audio selections */
	_audio_ops_box = new AudioRegionOperationsBox ();
	pack_start(*_audio_ops_box, false, false, 0);


	/* SLOT properties, for Trigger slot selections */
	_slot_prop_box = new SlotPropertiesBox ();
	pack_start(*_slot_prop_box, false, false, 0);
#endif

	/* watch for any change in our selection, so we can show an appropriate property editor */
	Editor::instance().get_selection().TracksChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	Editor::instance().get_selection().RegionsChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	Editor::instance().get_selection().TimeChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	Editor::instance().get_selection().LinesChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	Editor::instance().get_selection().PlaylistsChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	Editor::instance().get_selection().PointsChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	Editor::instance().get_selection().MarkersChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	Editor::instance().get_selection().MidiNotesChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	Editor::instance().get_selection().TriggersChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));

	/* maybe we care about mouse mode?? */
	Editor::instance().MouseModeChanged.connect (editor_connections, invalidator(*this), boost::bind (&SelectionPropertiesBox::track_mouse_mode, this), gui_context());

	selection_changed();
}

SelectionPropertiesBox::~SelectionPropertiesBox ()
{
	delete _time_info_box;

#if SELECTION_PROPERTIES_BOX_TODO
	delete _mregions_prop_box;

	delete _slot_prop_box;

	delete _midi_ops_box;
	delete _audio_ops_box;

	delete _midi_prop_box;
	delete _audio_prop_box;
#endif
}

void
SelectionPropertiesBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	_time_info_box->set_session(s);

#if SELECTION_PROPERTIES_BOX_TODO
	_mregions_prop_box->set_session(s);

	_midi_prop_box->set_session(s);
	_audio_prop_box->set_session(s);

	_midi_ops_box->set_session(s);
	_audio_ops_box->set_session(s);

	_slot_prop_box->set_session(s);
#endif
}

void
SelectionPropertiesBox::track_mouse_mode ()
{
	/* maybe do something here? */
}

void
SelectionPropertiesBox::selection_changed ()
{
	Selection& selection (Editor::instance().get_selection());

	_time_info_box->hide();

#if SELECTION_PROPERTIES_BOX_TODO
	_mregions_prop_box->hide();

	_midi_ops_box->hide();
	_audio_ops_box->hide();

	_midi_prop_box->hide();
	_audio_prop_box->hide();

	_slot_prop_box->hide();
#endif

	_header_label.hide();

	if (!selection.time.empty()) {
		_time_info_box->show();
		_header_label.set_text(_("Range Properties (Press ESC to Deselect All)"));
		_header_label.show();
	}

#if SELECTION_PROPERTIES_BOX_TODO
	/* one or more regions, show the multi-region operations box (just MUTE? kinda boring) */
	if (!selection.regions.empty()) {
		_mregions_prop_box->show();
	}

	bool found_midi_regions = false;
	for (RegionSelection::iterator s = selection.regions.begin(); s != selection.regions.end(); ++s) {
		ARDOUR::Region* region = (*s)->region().get();
		if (region->data_type() == DataType::MIDI) {
			found_midi_regions = true;
			break;
		}
	}

	bool found_audio_regions = false;
	for (RegionSelection::iterator s = selection.regions.begin(); s != selection.regions.end(); ++s) {
		ARDOUR::Region* region = (*s)->region().get();
		if (region->data_type() == DataType::AUDIO) {
			found_audio_regions = true;
			break;
		}
	}

	if (found_midi_regions && ! found_audio_regions) {
		_midi_ops_box->show();
	}
	if (found_audio_regions && ! found_midi_regions) {
		_audio_ops_box->show();
	}

	boost::shared_ptr<ARDOUR::Region> selected_region = boost::shared_ptr<ARDOUR::Region>();

	if (!selection.triggers.empty()) {
		TriggerSelection ts = selection.triggers;
		TriggerEntry* entry = *ts.begin();
		TriggerReference ref = entry->trigger_reference();

		//slot properties incl "Follow Actions"
		_slot_prop_box->set_slot(ref);
		_slot_prop_box->show();

		selected_region = ref.trigger()->region();
	} else if (selection.regions.size()==1)  {
		selected_region = (*(selection.regions.begin()))->region();
	}

	if (selected_region) {
		//region properties
		if (selected_region->data_type() == DataType::MIDI) {
			_midi_prop_box->set_region(selected_region);
			_midi_prop_box->show();
			_midi_ops_box->show();
		} else  {
			_audio_prop_box->set_region(selected_region);
			_audio_prop_box->show();
			_audio_ops_box->show();
		}
	}
#endif
}
