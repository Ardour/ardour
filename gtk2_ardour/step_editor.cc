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

#include "ardour/midi_track.h"
#include "ardour/midi_region.h"
#include "ardour/tempo.h"
#include "ardour/types.h"

#include "gui_thread.h"
#include "midi_region_view.h"
#include "public_editor.h"
#include "step_editor.h"
#include "step_entry.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace std;

StepEditor::StepEditor (PublicEditor& e, boost::shared_ptr<MidiTrack> t, MidiTimeAxisView& mtv)
        : _editor (e)
        , _track (t)
        , step_editor (0)
        , _mtv (mtv)
{
	step_edit_insert_position = 0;
        _step_edit_triplet_countdown = 0;
        _step_edit_within_chord = 0;
        _step_edit_chord_duration = 0.0;
        step_edit_region_view = 0;

        _track->PlaylistChanged.connect (*this, invalidator (*this),
                                         boost::bind (&StepEditor::playlist_changed, this),
                                         gui_context());
        playlist_changed ();
}

StepEditor::~StepEditor()
{
        delete step_editor;
}

void
StepEditor::start_step_editing ()
{
        _step_edit_triplet_countdown = 0;
        _step_edit_within_chord = 0;
        _step_edit_chord_duration = 0.0;
        step_edit_region.reset ();
        step_edit_region_view = 0;
        last_added_pitch = -1;
        last_added_end = 0;

        resync_step_edit_position ();
        prepare_step_edit_region ();
        reset_step_edit_beat_pos ();

        assert (step_edit_region);
        assert (step_edit_region_view);

        if (step_editor == 0) {
                step_editor = new StepEntry (*this);
                step_editor->signal_delete_event().connect (sigc::mem_fun (*this, &StepEditor::step_editor_hidden));
                step_editor->signal_hide().connect (sigc::mem_fun (*this, &StepEditor::step_editor_hide));
        }

        step_edit_region_view->show_step_edit_cursor (step_edit_beat_pos);
        step_edit_region_view->set_step_edit_cursor_width (step_editor->note_length());

        step_editor->present ();
}

void
StepEditor::resync_step_edit_position ()
{
	step_edit_insert_position = _editor.get_preferred_edit_position ();
}

void
StepEditor::resync_step_edit_to_edit_point ()
{
        resync_step_edit_position ();
        if (step_edit_region) {
                reset_step_edit_beat_pos ();
        }
}

void
StepEditor::prepare_step_edit_region ()
{
        boost::shared_ptr<Region> r = _track->playlist()->top_region_at (step_edit_insert_position);

        if (r) {
                step_edit_region = boost::dynamic_pointer_cast<MidiRegion>(r);
        }

	if (step_edit_region) {
		RegionView* rv = _mtv.midi_view()->find_view (step_edit_region);
		step_edit_region_view = dynamic_cast<MidiRegionView*> (rv);

	} else {

		const Meter& m = _mtv.session()->tempo_map().meter_at (step_edit_insert_position);
		const Tempo& t = _mtv.session()->tempo_map().tempo_at (step_edit_insert_position);

                step_edit_region = _mtv.add_region (step_edit_insert_position, floor (m.frames_per_bar (t, _mtv.session()->frame_rate())), true);

                RegionView* rv = _mtv.midi_view()->find_view (step_edit_region);
                step_edit_region_view = dynamic_cast<MidiRegionView*>(rv);
        }
}


void
StepEditor::reset_step_edit_beat_pos ()
{
        assert (step_edit_region);
        assert (step_edit_region_view);

        framecnt_t frames_from_start = _editor.get_preferred_edit_position() - step_edit_region->position();

        if (frames_from_start < 0) {
                /* this can happen with snap enabled, and the edit point == Playhead. we snap the
                   position of the new region, and it can end up after the edit point.
                */
                frames_from_start = 0;
        }

        step_edit_beat_pos = step_edit_region_view->region_frames_to_region_beats (frames_from_start);
        step_edit_region_view->move_step_edit_cursor (step_edit_beat_pos);
}

bool
StepEditor::step_editor_hidden (GdkEventAny*)
{
        step_editor_hide ();
        return true;
}

void
StepEditor::step_editor_hide ()
{
        /* everything else will follow the change in the model */
	_track->set_step_editing (false);
}

void
StepEditor::stop_step_editing ()
{
        if (step_editor) {
                step_editor->hide ();
        }

        if (step_edit_region_view) {
                step_edit_region_view->hide_step_edit_cursor();
        }

        step_edit_region.reset ();
}

void
StepEditor::check_step_edit ()
{
	MidiRingBuffer<framepos_t>& incoming (_track->step_edit_ring_buffer());
	uint8_t* buf;
	uint32_t bufsize = 32;

	buf = new uint8_t[bufsize];

	while (incoming.read_space()) {
		framepos_t time;
		Evoral::EventType type;
		uint32_t size;

		incoming.read_prefix (&time, &type, &size);

		if (size > bufsize) {
			delete [] buf;
			bufsize = size;
			buf = new uint8_t[bufsize];
		}

		incoming.read_contents (size, buf);

		if ((buf[0] & 0xf0) == MIDI_CMD_NOTE_ON) {
                        step_add_note (buf[0] & 0xf, buf[1], buf[2], 0.0);
		}
	}
}

int
StepEditor::step_add_bank_change (uint8_t /*channel*/, uint8_t /*bank*/)
{
        return 0;
}

int
StepEditor::step_add_program_change (uint8_t /*channel*/, uint8_t /*program*/)
{
        return 0;
}

void
StepEditor::step_edit_sustain (Evoral::MusicalTime beats)
{
        if (step_edit_region_view) {
                step_edit_region_view->step_sustain (beats);
        }
}

void
StepEditor::move_step_edit_beat_pos (Evoral::MusicalTime beats)
{
        if (beats > 0.0) {
                step_edit_beat_pos = min (step_edit_beat_pos + beats,
                                          step_edit_region_view->region_frames_to_region_beats (step_edit_region->length()));
        } else if (beats < 0.0) {
                if (-beats < step_edit_beat_pos) {
                        step_edit_beat_pos += beats; // its negative, remember
                } else {
                        step_edit_beat_pos = 0;
                }
        }
        step_edit_region_view->move_step_edit_cursor (step_edit_beat_pos);
}

int
StepEditor::step_add_note (uint8_t channel, uint8_t pitch, uint8_t velocity, Evoral::MusicalTime beat_duration)
{
        /* do these things in case undo removed the step edit region
        */
        if (!step_edit_region) {
                resync_step_edit_position ();
                prepare_step_edit_region ();
                reset_step_edit_beat_pos ();
                step_edit_region_view->show_step_edit_cursor (step_edit_beat_pos);
                step_edit_region_view->set_step_edit_cursor_width (step_editor->note_length());
        }

        assert (step_edit_region);
        assert (step_edit_region_view);

        if (beat_duration == 0.0) {
                bool success;
                beat_duration = _editor.get_grid_type_as_beats (success, step_edit_insert_position);

                if (!success) {
                        return -1;
                }
        }

        MidiStreamView* msv = _mtv.midi_view();

        /* make sure its visible on the vertical axis */

        if (pitch < msv->lowest_note() || pitch > msv->highest_note()) {
                msv->update_note_range (pitch);
                msv->set_note_range (MidiStreamView::ContentsRange);
        }

        /* make sure its visible on the horizontal axis */

        framepos_t fpos = step_edit_region_view->region_beats_to_absolute_frames (step_edit_beat_pos + beat_duration);

        if (fpos >= (_editor.leftmost_position() + _editor.current_page_frames())) {
                _editor.reset_x_origin (fpos - (_editor.current_page_frames()/4));
        }

        Evoral::MusicalTime at = step_edit_beat_pos;
        Evoral::MusicalTime len = beat_duration;

        if ((last_added_pitch >= 0) && (pitch == last_added_pitch) && (last_added_end == step_edit_beat_pos)) {

                /* avoid any apparent note overlap - move the start of this note
                   up by 1 tick from where the last note ended
                */

                at += 1.0/Timecode::BBT_Time::ticks_per_beat;
                len -= 1.0/Timecode::BBT_Time::ticks_per_beat;
        }

        step_edit_region_view->step_add_note (channel, pitch, velocity, at, len);

        last_added_pitch = pitch;
        last_added_end = at+len;

        if (_step_edit_triplet_countdown > 0) {
                _step_edit_triplet_countdown--;

                if (_step_edit_triplet_countdown == 0) {
                        _step_edit_triplet_countdown = 3;
                }
        }

        if (!_step_edit_within_chord) {
                step_edit_beat_pos += beat_duration;
                step_edit_region_view->move_step_edit_cursor (step_edit_beat_pos);
        } else {
                step_edit_beat_pos += 1.0/Timecode::BBT_Time::ticks_per_beat; // tiny, but no longer overlapping
                _step_edit_chord_duration = max (_step_edit_chord_duration, beat_duration);
        }

        return 0;
}

void
StepEditor::set_step_edit_cursor_width (Evoral::MusicalTime beats)
{
        if (step_edit_region_view) {
                step_edit_region_view->set_step_edit_cursor_width (beats);
        }
}

bool
StepEditor::step_edit_within_triplet() const
{
        return _step_edit_triplet_countdown > 0;
}

bool
StepEditor::step_edit_within_chord() const
{
        return _step_edit_within_chord;
}

void
StepEditor::step_edit_toggle_triplet ()
{
        if (_step_edit_triplet_countdown == 0) {
                _step_edit_within_chord = false;
                _step_edit_triplet_countdown = 3;
        } else {
                _step_edit_triplet_countdown = 0;
        }
}

void
StepEditor::step_edit_toggle_chord ()
{
        if (_step_edit_within_chord) {
                _step_edit_within_chord = false;
                step_edit_beat_pos += _step_edit_chord_duration;
                step_edit_region_view->move_step_edit_cursor (step_edit_beat_pos);
        } else {
                _step_edit_triplet_countdown = 0;
                _step_edit_within_chord = true;
        }
}

void
StepEditor::step_edit_rest (Evoral::MusicalTime beats)
{
	bool success;

        if (beats == 0.0) {
                beats = _editor.get_grid_type_as_beats (success, step_edit_insert_position);
        } else {
                success = true;
        }

        if (success) {
                step_edit_beat_pos += beats;
                step_edit_region_view->move_step_edit_cursor (step_edit_beat_pos);
        }
}

void
StepEditor::step_edit_beat_sync ()
{
        step_edit_beat_pos = ceil (step_edit_beat_pos);
        step_edit_region_view->move_step_edit_cursor (step_edit_beat_pos);
}

void
StepEditor::step_edit_bar_sync ()
{
        Session* _session = _mtv.session ();

        if (!_session || !step_edit_region_view || !step_edit_region) {
                return;
        }

        framepos_t fpos = step_edit_region_view->region_beats_to_absolute_frames (step_edit_beat_pos);
        fpos = _session->tempo_map().round_to_bar (fpos, 1);
        step_edit_beat_pos = ceil (step_edit_region_view->region_frames_to_region_beats (fpos - step_edit_region->position()));
        step_edit_region_view->move_step_edit_cursor (step_edit_beat_pos);
}

void
StepEditor::playlist_changed ()
{
        step_edit_region_connection.disconnect ();
        _track->playlist()->RegionRemoved.connect (step_edit_region_connection, invalidator (*this),
                                                   boost::bind (&StepEditor::region_removed, this, _1),
                                                   gui_context());
}

void
StepEditor::region_removed (boost::weak_ptr<Region> wr)
{
        boost::shared_ptr<Region> r (wr.lock());

        if (!r) {
                return;
        }

        if (step_edit_region == r) {
                step_edit_region.reset();
                step_edit_region_view = 0;
                // force a recompute of the insert position
                step_edit_beat_pos = -1.0;
        }
}

string
StepEditor::name() const
{
        return _track->name();
}
