/*
    Copyright (C) 2012 Paul Davis

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

#ifndef __gtk2_ardour_step_editor_h__
#define __gtk2_ardour_step_editor_h__

#include <string>

#include <gdk/gdk.h>
#include <sigc++/trackable.h>

#include "pbd/signals.h"

#include "temporal/beats.h"

namespace ARDOUR {
class MidiTrack;
class MidiRegion;
}

class MidiRegionView;
class MidiTimeAxisView;
class PublicEditor;
class StepEntry;

class StepEditor : public PBD::ScopedConnectionList, public sigc::trackable
{
public:
	StepEditor (PublicEditor&, boost::shared_ptr<ARDOUR::MidiTrack>, MidiTimeAxisView&);
	virtual ~StepEditor ();

	void check_step_edit ();
	void step_edit_rest (Temporal::Beats beats);
	void step_edit_beat_sync ();
	void step_edit_bar_sync ();
	int  step_add_bank_change (uint8_t channel, uint8_t bank);
	int  step_add_program_change (uint8_t channel, uint8_t program);
	int  step_add_note (uint8_t channel, uint8_t pitch, uint8_t velocity,
	                    Temporal::Beats beat_duration);
	void step_edit_sustain (Temporal::Beats beats);
	bool step_edit_within_triplet () const;
	void step_edit_toggle_triplet ();
	bool step_edit_within_chord () const;
	void step_edit_toggle_chord ();
	void reset_step_edit_beat_pos ();
	void resync_step_edit_to_edit_point ();
	void move_step_edit_beat_pos (Temporal::Beats beats);
	void set_step_edit_cursor_width (Temporal::Beats beats);

	std::string name() const;

	void start_step_editing ();
	void stop_step_editing ();

private:
	ARDOUR::samplepos_t                    step_edit_insert_position;
	Temporal::Beats                       step_edit_beat_pos;
	boost::shared_ptr<ARDOUR::MidiRegion> step_edit_region;
	MidiRegionView*                       step_edit_region_view;
	uint8_t                               _step_edit_triplet_countdown;
	bool                                  _step_edit_within_chord;
	Temporal::Beats                       _step_edit_chord_duration;
	PBD::ScopedConnection                 step_edit_region_connection;
	PublicEditor&                         _editor;
	boost::shared_ptr<ARDOUR::MidiTrack>  _track;
	StepEntry*                            step_editor;
	MidiTimeAxisView&                     _mtv;
	int8_t                                last_added_pitch;
	Temporal::Beats                       last_added_end;

	void region_removed (boost::weak_ptr<ARDOUR::Region>);
	void playlist_changed ();
	bool step_editor_hidden (GdkEventAny*);
	void step_editor_hide ();
	void resync_step_edit_position ();
	void prepare_step_edit_region ();
};

#endif /* __gtk2_ardour_step_editor_h__ */
