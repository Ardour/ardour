/*
    Copyright (C) 2000-2006 Paul Davis

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

    $Id: midiregion.cc 746 2006-08-02 02:44:23Z drobilla $
*/

#include <cmath>
#include <climits>
#include <cfloat>

#include <set>

#include <glibmm/threads.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "evoral/Beats.hpp"

#include "pbd/xml++.h"
#include "pbd/basename.h"

#include "ardour/automation_control.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_source.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/source_factory.h"
#include "ardour/tempo.h"
#include "ardour/types.h"

#include "pbd/i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace ARDOUR {
	namespace Properties {
		PBD::PropertyDescriptor<Evoral::Beats> start_beats;
		PBD::PropertyDescriptor<Evoral::Beats> length_beats;
	}
}

void
MidiRegion::make_property_quarks ()
{
	Properties::start_beats.property_id = g_quark_from_static_string (X_("start-beats"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for start-beats = %1\n", Properties::start_beats.property_id));
	Properties::length_beats.property_id = g_quark_from_static_string (X_("length-beats"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for length-beats = %1\n", Properties::length_beats.property_id));
}

void
MidiRegion::register_properties ()
{
	add_property (_start_beats);
	add_property (_length_beats);
}

/* Basic MidiRegion constructor (many channels) */
MidiRegion::MidiRegion (const SourceList& srcs)
	: Region (srcs)
	, _start_beats (Properties::start_beats, Evoral::Beats())
	, _length_beats (Properties::length_beats, midi_source(0)->length_beats())
{
	register_properties ();

	midi_source(0)->ModelChanged.connect_same_thread (_source_connection, boost::bind (&MidiRegion::model_changed, this));
	model_changed ();
	assert(_name.val().find("/") == string::npos);
	assert(_type == DataType::MIDI);
}

MidiRegion::MidiRegion (boost::shared_ptr<const MidiRegion> other)
	: Region (other)
	, _start_beats (Properties::start_beats, other->_start_beats)
	, _length_beats (Properties::length_beats, other->_length_beats)
{
	//update_length_beats ();
	register_properties ();

	assert(_name.val().find("/") == string::npos);
	midi_source(0)->ModelChanged.connect_same_thread (_source_connection, boost::bind (&MidiRegion::model_changed, this));
	model_changed ();
}

/** Create a new MidiRegion that is part of an existing one */
MidiRegion::MidiRegion (boost::shared_ptr<const MidiRegion> other, frameoffset_t offset, const int32_t sub_num)
	: Region (other, offset, sub_num)
	, _start_beats (Properties::start_beats, Evoral::Beats())
	, _length_beats (Properties::length_beats, other->_length_beats)
{
	_start_beats = Evoral::Beats (_session.tempo_map().exact_beat_at_frame ((other->_position + offset), sub_num) - other->beat()) + other->_start_beats;
	update_length_beats (sub_num);
	register_properties ();

	assert(_name.val().find("/") == string::npos);
	midi_source(0)->ModelChanged.connect_same_thread (_source_connection, boost::bind (&MidiRegion::model_changed, this));
	model_changed ();
}

MidiRegion::~MidiRegion ()
{
}

/** Export the MIDI data of the MidiRegion to a new MIDI file (SMF).
 */
bool
MidiRegion::do_export (string path) const
{
	boost::shared_ptr<MidiSource> newsrc;

	/* caller must check for pre-existing file */
	assert (!path.empty());
	assert (!Glib::file_test (path, Glib::FILE_TEST_EXISTS));
	newsrc = boost::dynamic_pointer_cast<MidiSource>(
		SourceFactory::createWritable(DataType::MIDI, _session,
		                              path, false, _session.frame_rate()));

	BeatsFramesConverter bfc (_session.tempo_map(), _position);
	Evoral::Beats const bbegin = bfc.from (_start);
	Evoral::Beats const bend = bfc.from (_start + _length);

	{
		/* Lock our source since we'll be reading from it.  write_to() will
		   take a lock on newsrc. */
		Source::Lock lm (midi_source(0)->mutex());
		if (midi_source(0)->export_write_to (lm, newsrc, bbegin, bend)) {
			return false;
		}
	}

	return true;
}


/** Create a new MidiRegion that has its own version of some/all of the Source used by another.
 */
boost::shared_ptr<MidiRegion>
MidiRegion::clone (string path) const
{
	boost::shared_ptr<MidiSource> newsrc;

	/* caller must check for pre-existing file */
	assert (!path.empty());
	assert (!Glib::file_test (path, Glib::FILE_TEST_EXISTS));
	newsrc = boost::dynamic_pointer_cast<MidiSource>(
		SourceFactory::createWritable(DataType::MIDI, _session,
					      path, false, _session.frame_rate()));
	return clone (newsrc);
}

boost::shared_ptr<MidiRegion>
MidiRegion::clone (boost::shared_ptr<MidiSource> newsrc) const
{
	BeatsFramesConverter bfc (_session.tempo_map(), _position);
	Evoral::Beats const bbegin = bfc.from (_start);
	Evoral::Beats const bend = bfc.from (_start + _length);

	{
		/* Lock our source since we'll be reading from it.  write_to() will
		   take a lock on newsrc. */
		Source::Lock lm (midi_source(0)->mutex());
		if (midi_source(0)->write_to (lm, newsrc, bbegin, bend)) {
			return boost::shared_ptr<MidiRegion> ();
		}
	}

	PropertyList plist;

	plist.add (Properties::name, PBD::basename_nosuffix (newsrc->name()));
	plist.add (Properties::whole_file, true);
	plist.add (Properties::start, _start);
	plist.add (Properties::start_beats, _start_beats);
	plist.add (Properties::length, _length);
	plist.add (Properties::length_beats, _length_beats);
	plist.add (Properties::layer, 0);

	boost::shared_ptr<MidiRegion> ret (boost::dynamic_pointer_cast<MidiRegion> (RegionFactory::create (newsrc, plist, true)));
	ret->set_beat (beat());

	return ret;
}

void
MidiRegion::post_set (const PropertyChange& pc)
{
	Region::post_set (pc);

	if (pc.contains (Properties::length) && !pc.contains (Properties::length_beats)) {
		/* update non-musically */
		update_length_beats (0);
	} else if (pc.contains (Properties::start) && !pc.contains (Properties::start_beats)) {
		set_start_beats_from_start_frames ();
	}
}

void
MidiRegion::set_start_beats_from_start_frames ()
{
	_start_beats = Evoral::Beats (beat() - _session.tempo_map().beat_at_frame (_position - _start));
}

void
MidiRegion::set_length_internal (framecnt_t len, const int32_t sub_num)
{
	Region::set_length_internal (len, sub_num);
	update_length_beats (sub_num);
}

void
MidiRegion::update_after_tempo_map_change (bool /* send */)
{
	boost::shared_ptr<Playlist> pl (playlist());

	if (!pl) {
		return;
	}

	const framepos_t old_pos = _position;
	const framepos_t old_length = _length;
	const framepos_t old_start = _start;

	PropertyChange s_and_l;

	if (position_lock_style() == AudioTime) {
		recompute_position_from_lock_style (0);

		/*
		  set _start to new position in tempo map.

		  The user probably expects the region contents to maintain audio position as the 
		  tempo changes, but AFAICT this requires modifying the src file to use
		  SMPTE timestamps with the current disk read model (?).

		  We could arguably use _start to set _start_beats here,
		  resulting in viewport-like behaviour (the contents maintain
		  their musical position while the region is stationary).

		  For now, the musical position at the region start is retained, but subsequent events
		  will maintain their beat distance according to the map.
		*/
		_start = _position - _session.tempo_map().frame_at_beat (beat() - _start_beats.val().to_double());

		/* _length doesn't change for audio-locked regions. update length_beats to match. */
		_length_beats = Evoral::Beats (_session.tempo_map().beat_at_frame (_position + _length) - _session.tempo_map().beat_at_frame (_position));

		s_and_l.add (Properties::start);
		s_and_l.add (Properties::length_beats);

		send_change  (s_and_l);
		return;
	}

	Region::update_after_tempo_map_change (false);

	/* _start has now been updated. */
	_length = _session.tempo_map().frame_at_beat (beat() + _length_beats.val().to_double()) - _position;

	if (old_start != _start) {
		s_and_l.add (Properties::start);
	}
	if (old_length != _length) {
		s_and_l.add (Properties::length);
	}
	if (old_pos != _position) {
		s_and_l.add (Properties::position);
	}

	send_change (s_and_l);
}

void
MidiRegion::update_length_beats (const int32_t sub_num)
{
	_length_beats = Evoral::Beats (_session.tempo_map().exact_beat_at_frame (_position + _length, sub_num) - beat());
}

void
MidiRegion::set_position_internal (framepos_t pos, bool allow_bbt_recompute, const int32_t sub_num)
{
	Region::set_position_internal (pos, allow_bbt_recompute, sub_num);

	/* set _start to new position in tempo map */
	_start = _position - _session.tempo_map().frame_at_beat (beat() - _start_beats.val().to_double());

	/* in construction from src */
	if (_length_beats == Evoral::Beats()) {
		update_length_beats (sub_num);
	}

	if (position_lock_style() == AudioTime) {
		_length_beats = Evoral::Beats (_session.tempo_map().beat_at_frame (_position + _length) - _session.tempo_map().beat_at_frame (_position));
	} else {
		/* leave _length_beats alone, and change _length to reflect the state of things
		   at the new position (tempo map may dictate a different number of frames).
		*/
		Region::set_length_internal (_session.tempo_map().frame_at_beat (beat() + _length_beats.val().to_double()) - _position, sub_num);
	}
}

framecnt_t
MidiRegion::read_at (Evoral::EventSink<framepos_t>& out,
                     framepos_t                     position,
                     framecnt_t                     dur,
                     uint32_t                       chan_n,
                     NoteMode                       mode,
                     MidiStateTracker*              tracker,
                     MidiChannelFilter*             filter) const
{
	return _read_at (_sources, out, position, dur, chan_n, mode, tracker, filter);
}

framecnt_t
MidiRegion::master_read_at (MidiRingBuffer<framepos_t>& out, framepos_t position, framecnt_t dur, uint32_t chan_n, NoteMode mode) const
{
	return _read_at (_master_sources, out, position, dur, chan_n, mode); /* no tracker */
}

framecnt_t
MidiRegion::_read_at (const SourceList&              /*srcs*/,
                      Evoral::EventSink<framepos_t>& dst,
                      framepos_t                     position,
                      framecnt_t                     dur,
                      uint32_t                       chan_n,
                      NoteMode                       mode,
                      MidiStateTracker*              tracker,
                      MidiChannelFilter*             filter) const
{
	frameoffset_t internal_offset = 0;
	framecnt_t    to_read         = 0;

	/* precondition: caller has verified that we cover the desired section */

	assert(chan_n == 0);

	if (muted()) {
		return 0; /* read nothing */
	}

	if (position < _position) {
		/* we are starting the read from before the start of the region */
		internal_offset = 0;
		dur -= _position - position;
	} else {
		/* we are starting the read from after the start of the region */
		internal_offset = position - _position;
	}

	if (internal_offset >= _length) {
		return 0; /* read nothing */
	}

	if ((to_read = min (dur, _length - internal_offset)) == 0) {
		return 0; /* read nothing */
	}

	boost::shared_ptr<MidiSource> src = midi_source(chan_n);

	Glib::Threads::Mutex::Lock lm(src->mutex());

	src->set_note_mode(lm, mode);

	/*
	  cerr << "MR " << name () << " read @ " << position << " * " << to_read
	  << " _position = " << _position
	  << " _start = " << _start
	  << " intoffset = " << internal_offset
	  << endl;
	*/

	/* This call reads events from a source and writes them to `dst' timed in session frames */

	if (src->midi_read (
		    lm, // source lock
			dst, // destination buffer
			_position - _start, // start position of the source in session frames
			_start + internal_offset, // where to start reading in the source
			to_read, // read duration in frames
			tracker,
			filter,
			_filtered_parameters,
			beat(),
			_start_beats.val().to_double()
		    ) != to_read) {
		return 0; /* "read nothing" */
	}

	return to_read;
}

XMLNode&
MidiRegion::state ()
{
	return Region::state ();
}

int
MidiRegion::set_state (const XMLNode& node, int version)
{
	int ret = Region::set_state (node, version);

	if (ret == 0) {
		/* set length beats to the frame (non-musical) */
		if (position_lock_style() == AudioTime) {
			update_length_beats (0);
		}
	}

	return ret;
}

void
MidiRegion::recompute_at_end ()
{
	/* our length has changed
	 * so what? stuck notes are dealt with via a note state tracker
	 */
}

void
MidiRegion::recompute_at_start ()
{
	/* as above, but the shift was from the front
	 * maybe bump currently active note's note-ons up so they sound here?
	 * that could be undesireable in certain situations though.. maybe
	 * remove the note entirely, including it's note off?  something needs to
	 * be done to keep the played MIDI sane to avoid messing up voices of
	 * polyhonic things etc........
	 */
}

int
MidiRegion::separate_by_channel (ARDOUR::Session&, vector< boost::shared_ptr<Region> >&) const
{
	// TODO
	return -1;
}

boost::shared_ptr<Evoral::Control>
MidiRegion::control (const Evoral::Parameter& id, bool create)
{
	return model()->control(id, create);
}

boost::shared_ptr<const Evoral::Control>
MidiRegion::control (const Evoral::Parameter& id) const
{
	return model()->control(id);
}

boost::shared_ptr<MidiModel>
MidiRegion::model()
{
	return midi_source()->model();
}

boost::shared_ptr<const MidiModel>
MidiRegion::model() const
{
	return midi_source()->model();
}

boost::shared_ptr<MidiSource>
MidiRegion::midi_source (uint32_t n) const
{
	// Guaranteed to succeed (use a static cast?)
	return boost::dynamic_pointer_cast<MidiSource>(source(n));
}

void
MidiRegion::model_changed ()
{
	if (!model()) {
		return;
	}

	/* build list of filtered Parameters, being those whose automation state is not `Play' */

	_filtered_parameters.clear ();

	Automatable::Controls const & c = model()->controls();

	for (Automatable::Controls::const_iterator i = c.begin(); i != c.end(); ++i) {
		boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl> (i->second);
		assert (ac);
		if (ac->alist()->automation_state() != Play) {
			_filtered_parameters.insert (ac->parameter ());
		}
	}

	/* watch for changes to controls' AutoState */
	midi_source()->AutomationStateChanged.connect_same_thread (
		_model_connection, boost::bind (&MidiRegion::model_automation_state_changed, this, _1)
		);
}

void
MidiRegion::model_automation_state_changed (Evoral::Parameter const & p)
{
	/* Update our filtered parameters list after a change to a parameter's AutoState */

	boost::shared_ptr<AutomationControl> ac = model()->automation_control (p);
	if (!ac || ac->alist()->automation_state() == Play) {
		/* It should be "impossible" for ac to be NULL, but if it is, don't
		   filter the parameter so events aren't lost. */
		_filtered_parameters.erase (p);
	} else {
		_filtered_parameters.insert (p);
	}

	/* the source will have an iterator into the model, and that iterator will have been set up
	   for a given set of filtered_parameters, so now that we've changed that list we must invalidate
	   the iterator.
	*/
	Glib::Threads::Mutex::Lock lm (midi_source(0)->mutex(), Glib::Threads::TRY_LOCK);
	if (lm.locked()) {
		/* TODO: This is too aggressive, we need more fine-grained invalidation. */
		midi_source(0)->invalidate (lm);
	}
}

/** This is called when a trim drag has resulted in a -ve _start time for this region.
 *  Fix it up by adding some empty space to the source.
 */
void
MidiRegion::fix_negative_start ()
{
	BeatsFramesConverter c (_session.tempo_map(), _position);

	model()->insert_silence_at_start (c.from (-_start));
	_start = 0;
	_start_beats = Evoral::Beats();
}

void
MidiRegion::set_start_internal (framecnt_t s, const int32_t sub_num)
{
	Region::set_start_internal (s, sub_num);

	if (position_lock_style() == AudioTime) {
		set_start_beats_from_start_frames ();
		}
}

void
MidiRegion::trim_to_internal (framepos_t position, framecnt_t length, const int32_t sub_num)
{
	if (locked()) {
		return;
	}

	PropertyChange what_changed;

	/* beat has been set exactly by set_position_internal, but the source starts on a frame.
	   working in beats seems the correct thing to do, but reports of a missing first note
	   on playback suggest otherwise. for now, we work in exact beats.
	*/
	const double pos_beat = _session.tempo_map().exact_beat_at_frame (position, sub_num);
	const double beat_delta = pos_beat - beat();

	/* Set position before length, otherwise for MIDI regions this bad thing happens:
	 * 1. we call set_length_internal; length in beats is computed using the region's current
	 *    (soon-to-be old) position
	 * 2. we call set_position_internal; position is set and length in frames re-computed using
	 *    length in beats from (1) but at the new position, which is wrong if the region
	 *    straddles a tempo/meter change.
	 */

	if (_position != position) {
		/* sets _beat to new position.*/
		set_position_internal (position, true, sub_num);
		what_changed.add (Properties::position);

		const double new_start_beat = _start_beats.val().to_double() + beat_delta;
		const framepos_t new_start = _position - _session.tempo_map().frame_at_beat (beat() - new_start_beat);

		if (!verify_start_and_length (new_start, length)) {
			return;
		}

		_start_beats = Evoral::Beats (new_start_beat);
		what_changed.add (Properties::start_beats);

		set_start_internal (new_start, sub_num);
		what_changed.add (Properties::start);
	}

	if (_length != length) {

		if (!verify_start_and_length (_start, length)) {
			return;
		}

		set_length_internal (length, sub_num);
		what_changed.add (Properties::length);
		what_changed.add (Properties::length_beats);
	}

	set_whole_file (false);

	PropertyChange start_and_length;

	start_and_length.add (Properties::start);
	start_and_length.add (Properties::length);

	if (what_changed.contains (start_and_length)) {
		first_edit ();
	}

	if (!what_changed.empty()) {
		send_change (what_changed);
	}
}
