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

#include "pbd/xml++.h"
#include "pbd/basename.h"

#include "ardour/automation_control.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_source.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/types.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace ARDOUR {
	namespace Properties {
		PBD::PropertyDescriptor<void*>                midi_data;
		PBD::PropertyDescriptor<Evoral::MusicalTime>  start_beats;
		PBD::PropertyDescriptor<Evoral::MusicalTime>  length_beats;
	}
}

void
MidiRegion::make_property_quarks ()
{
	Properties::midi_data.property_id = g_quark_from_static_string (X_("midi-data"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for midi-data = %1\n", Properties::midi_data.property_id));
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
	, _start_beats (Properties::start_beats, 0)
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
	, _length_beats (Properties::length_beats, (Evoral::MusicalTime) 0)
{
	update_length_beats ();
	register_properties ();

	assert(_name.val().find("/") == string::npos);
	midi_source(0)->ModelChanged.connect_same_thread (_source_connection, boost::bind (&MidiRegion::model_changed, this));
	model_changed ();
}

/** Create a new MidiRegion that is part of an existing one */
MidiRegion::MidiRegion (boost::shared_ptr<const MidiRegion> other, frameoffset_t offset)
	: Region (other, offset)
	, _start_beats (Properties::start_beats, (Evoral::MusicalTime) 0)
	, _length_beats (Properties::length_beats, (Evoral::MusicalTime) 0)
{
	BeatsFramesConverter bfc (_session.tempo_map(), _position);
	Evoral::MusicalTime const offset_beats = bfc.from (offset);

	_start_beats = other->_start_beats + offset_beats;
	_length_beats = other->_length_beats - offset_beats;

	register_properties ();

	assert(_name.val().find("/") == string::npos);
	midi_source(0)->ModelChanged.connect_same_thread (_source_connection, boost::bind (&MidiRegion::model_changed, this));
	model_changed ();
}

MidiRegion::~MidiRegion ()
{
}

/** Create a new MidiRegion that has its own version of some/all of the Source used by another.
 */
boost::shared_ptr<MidiRegion>
MidiRegion::clone (string path) const
{
	BeatsFramesConverter bfc (_session.tempo_map(), _position);
	Evoral::MusicalTime const bbegin = bfc.from (_start);
	Evoral::MusicalTime const bend = bfc.from (_start + _length);

	boost::shared_ptr<MidiSource> ms = midi_source(0)->clone (path, bbegin, bend);

	PropertyList plist;

	plist.add (Properties::name, PBD::basename_nosuffix (ms->name()));
	plist.add (Properties::whole_file, true);
	plist.add (Properties::start, _start);
	plist.add (Properties::start_beats, _start_beats);
	plist.add (Properties::length, _length);
	plist.add (Properties::length_beats, _length_beats);
	plist.add (Properties::layer, 0);

	return boost::dynamic_pointer_cast<MidiRegion> (RegionFactory::create (ms, plist, true));
}

void
MidiRegion::post_set (const PropertyChange& pc)
{
	Region::post_set (pc);

	if (pc.contains (Properties::length) && !pc.contains (Properties::length_beats)) {
		update_length_beats ();
	} else if (pc.contains (Properties::start) && !pc.contains (Properties::start_beats)) {
		set_start_beats_from_start_frames ();
	}
}

void
MidiRegion::set_start_beats_from_start_frames ()
{
	BeatsFramesConverter c (_session.tempo_map(), _position - _start);
	_start_beats = c.from (_start);
}

void
MidiRegion::set_length_internal (framecnt_t len)
{
	Region::set_length_internal (len);
	update_length_beats ();
}

void
MidiRegion::update_after_tempo_map_change ()
{
	Region::update_after_tempo_map_change ();

	/* _position has now been updated for the new tempo map */
	_start = _position - _session.tempo_map().framepos_minus_beats (_position, _start_beats);

	send_change (Properties::start);
}

void
MidiRegion::update_length_beats ()
{
	BeatsFramesConverter converter (_session.tempo_map(), _position);
	_length_beats = converter.from (_length);
}

void
MidiRegion::set_position_internal (framepos_t pos, bool allow_bbt_recompute)
{
	Region::set_position_internal (pos, allow_bbt_recompute);
	/* zero length regions don't exist - so if _length_beats is zero, this object
	   is under construction.
	*/
	if (_length_beats) {
		/* leave _length_beats alone, and change _length to reflect the state of things
		   at the new position (tempo map may dictate a different number of frames
		*/
		BeatsFramesConverter converter (_session.tempo_map(), _position);
		Region::set_length_internal (converter.to (_length_beats));
	}
}

framecnt_t
MidiRegion::read_at (Evoral::EventSink<framepos_t>& out, framepos_t position, framecnt_t dur, uint32_t chan_n, NoteMode mode, MidiStateTracker* tracker) const
{
	return _read_at (_sources, out, position, dur, chan_n, mode, tracker);
}

framecnt_t
MidiRegion::master_read_at (MidiRingBuffer<framepos_t>& out, framepos_t position, framecnt_t dur, uint32_t chan_n, NoteMode mode) const
{
	return _read_at (_master_sources, out, position, dur, chan_n, mode); /* no tracker */
}

framecnt_t
MidiRegion::_read_at (const SourceList& /*srcs*/, Evoral::EventSink<framepos_t>& dst, framepos_t position, framecnt_t dur, uint32_t chan_n,
		      NoteMode mode, MidiStateTracker* tracker) const
{
	frameoffset_t internal_offset = 0;
	framecnt_t to_read         = 0;

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
	src->set_note_mode(mode);

	/*
	  cerr << "MR " << name () << " read @ " << position << " * " << to_read
	  << " _position = " << _position
	  << " _start = " << _start
	  << " intoffset = " << internal_offset
	  << endl;
	*/

	/* This call reads events from a source and writes them to `dst' timed in session frames */

	if (src->midi_read (
			dst, // destination buffer
			_position - _start, // start position of the source in session frames
			_start + internal_offset, // where to start reading in the source
			to_read, // read duration in frames
			tracker,
			_filtered_parameters
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
		update_length_beats ();
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

	model()->ContentsChanged.connect_same_thread (
	        _model_contents_connection, boost::bind (&MidiRegion::model_contents_changed, this));
}

void
MidiRegion::model_contents_changed ()
{
	send_change (PropertyChange (Properties::midi_data));
}

void
MidiRegion::model_automation_state_changed (Evoral::Parameter const & p)
{
	/* Update our filtered parameters list after a change to a parameter's AutoState */

	boost::shared_ptr<AutomationControl> ac = model()->automation_control (p);
	assert (ac);

	if (ac->alist()->automation_state() == Play) {
		_filtered_parameters.erase (p);
	} else {
		_filtered_parameters.insert (p);
	}

	/* the source will have an iterator into the model, and that iterator will have been set up
	   for a given set of filtered_parameters, so now that we've changed that list we must invalidate
	   the iterator.
	*/
	Glib::Threads::Mutex::Lock lm (midi_source(0)->mutex());
	midi_source(0)->invalidate ();
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
	_start_beats = 0;
}

/** Transpose the notes in this region by a given number of semitones */
void
MidiRegion::transpose (int semitones)
{
	BeatsFramesConverter c (_session.tempo_map(), _start);
	model()->transpose (c.from (_start), c.from (_start + _length), semitones);
}

void
MidiRegion::set_start_internal (framecnt_t s)
{
	Region::set_start_internal (s);
	set_start_beats_from_start_frames ();
}
