/*
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Julien "_FrnchFrgg_" RIVAUD <frnchfrgg@free.fr>
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
#include <climits>
#include <cfloat>

#include <set>

#include <glibmm/threads.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "temporal/beats.h"

#include "pbd/xml++.h"
#include "pbd/basename.h"

#include "ardour/automation_control.h"
#include "ardour/midi_cursor.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_source.h"
#include "ardour/playlist.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/source_factory.h"
#include "ardour/tempo.h"
#include "ardour/thawlist.h"
#include "ardour/types.h"
#include "ardour/evoral_types_convert.h"

#include "pbd/i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace ARDOUR {
	namespace Properties {
		PBD::PropertyDescriptor<double> start_beats;
		PBD::PropertyDescriptor<double> length_beats;
	}
}

/* Basic MidiRegion constructor (many channels) */
MidiRegion::MidiRegion (const SourceList& srcs)
	: Region (srcs)
	, _start_beats (Properties::start_beats, 0.0)
	, _length_beats (Properties::length_beats, midi_source(0)->length_beats().to_double())
	, _ignore_shift (false)
{
	midi_source(0)->ModelChanged.connect_same_thread (_source_connection, boost::bind (&MidiRegion::model_changed, this));
	model_changed ();
	assert(_name.val().find("/") == string::npos);
	assert(_type == DataType::MIDI);
}

MidiRegion::MidiRegion (boost::shared_ptr<const MidiRegion> other)
	: Region (other)
	, _start_beats (Properties::start_beats, other->_start_beats)
	, _length_beats (Properties::length_beats, other->_length_beats)
	, _ignore_shift (false)
{
	assert(_name.val().find("/") == string::npos);
	midi_source(0)->ModelChanged.connect_same_thread (_source_connection, boost::bind (&MidiRegion::model_changed, this));
	model_changed ();
}

/** Create a new MidiRegion that is part of an existing one */
MidiRegion::MidiRegion (boost::shared_ptr<const MidiRegion> other, timecnt_t const & offset)
	: Region (other, offset)
	, _start_beats (Properties::start_beats, other->_start_beats)
	, _length_beats (Properties::length_beats, other->_length_beats)
	, _ignore_shift (false)
{

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
	newsrc = boost::dynamic_pointer_cast<MidiSource>(SourceFactory::createWritable(DataType::MIDI, _session, path, _session.sample_rate()));

	BeatsSamplesConverter bfc (_session.tempo_map(), _position);
	Temporal::Beats const bbegin = bfc.from (_start);
	Temporal::Beats const bend = bfc.from (_start + _length);

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
		SourceFactory::createWritable(DataType::MIDI, _session, path, _session.sample_rate()));
	return clone (newsrc);
}

boost::shared_ptr<MidiRegion>
MidiRegion::clone (boost::shared_ptr<MidiSource> newsrc, ThawList* tl) const
{
	BeatsSamplesConverter bfc (_session.tempo_map(), _position);
	Temporal::Beats const bbegin = bfc.from (_start);
	Temporal::Beats const bend = bfc.from (_start + _length);

	{
		boost::shared_ptr<MidiSource> ms = midi_source(0);
		Source::Lock lm (ms->mutex());

		if (!ms->model()) {
			ms->load_model (lm);
		}

		/* Lock our source since we'll be reading from it.  write_to() will
		   take a lock on newsrc.
		*/

		if (ms->write_to (lm, newsrc, bbegin, bend)) {
			return boost::shared_ptr<MidiRegion> ();
		}
	}

	PropertyList plist;

	plist.add (Properties::name, PBD::basename_nosuffix (newsrc->name()));
	plist.add (Properties::whole_file, true);
	plist.add (Properties::start, _start);
	plist.add (Properties::start_beats, _start_beats);
	plist.add (Properties::length, _length);
	plist.add (Properties::position, _position);
	plist.add (Properties::beat, _beat);
	plist.add (Properties::length_beats, _length_beats);
	plist.add (Properties::layer, 0);

	boost::shared_ptr<MidiRegion> ret (boost::dynamic_pointer_cast<MidiRegion> (RegionFactory::create (newsrc, plist, true, tl)));
	ret->set_quarter_note (quarter_note());

	return ret;
}

void
MidiRegion::post_set (const PropertyChange& pc)
{
	Region::post_set (pc);

	if (pc.contains (Properties::length) && !pc.contains (Properties::length_beats)) {
		/* we're called by Stateful::set_values() which sends a change
		   only if the value is different from _current.
		   session load means we can clobber length_beats here in error (not all properties differ from current),
		   so disallow (this has been set from XML state anyway).
		*/
		if (!_session.loading()) {
			update_length_beats (0);
		}
	}

	if (pc.contains (Properties::start) && !pc.contains (Properties::start_beats)) {
		set_start_beats_from_start_samples ();
	}
}

void
MidiRegion::set_start_beats_from_start_samples ()
{
	if (position_lock_style() == AudioTime) {
		_start_beats = quarter_note() - _session.tempo_map().quarter_note_at_sample (_position - _start);
	}
}

void
MidiRegion::set_length_internal (samplecnt_t len, const int32_t sub_num)
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

	const samplepos_t old_pos = _position;
	const samplepos_t old_length = _length;
	const samplepos_t old_start = _start;

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
		_start = _session.tempo_map().samples_between_quarter_notes (quarter_note() - start_beats(), quarter_note());

		/* _length doesn't change for audio-locked regions. update length_beats to match. */
		_length_beats = _session.tempo_map().quarter_note_at_sample (_position + _length) - quarter_note();

		s_and_l.add (Properties::start);
		s_and_l.add (Properties::length_beats);

		send_change  (s_and_l);
		return;
	}

	Region::update_after_tempo_map_change (false);

	/* _start has now been updated. */
	_length = max ((samplecnt_t) 1, _session.tempo_map().samples_between_quarter_notes (quarter_note(), quarter_note() + _length_beats));

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
	_length_beats = _session.tempo_map().exact_qn_at_sample (_position + _length, sub_num) - quarter_note();
}

void
MidiRegion::set_position_internal (timepos_t const & pos, bool allow_bbt_recompute, const int32_t sub_num)
{
	Region::set_position_internal (pos, allow_bbt_recompute, sub_num);

	/* don't clobber _start _length and _length_beats if session loading.*/
	if (_session.loading()) {
		return;
	}

	/* set _start to new position in tempo map */
	_start = _session.tempo_map().samples_between_quarter_notes (quarter_note() - start_beats(), quarter_note());

	/* in construction from src */
	if (_length_beats == 0.0) {
		update_length_beats (sub_num);
	}

	if (position_lock_style() == AudioTime) {
		_length_beats = _session.tempo_map().quarter_note_at_sample (_position + _length) - quarter_note();
	} else {
		/* leave _length_beats alone, and change _length to reflect the state of things
		   at the new position (tempo map may dictate a different number of samples).
		*/
		Region::set_length_internal (_session.tempo_map().samples_between_quarter_notes (quarter_note(), quarter_note() + length_beats()), sub_num);
	}
}

samplecnt_t
MidiRegion::read_at (Evoral::EventSink<samplepos_t>& out,
                     samplepos_t                     position,
                     samplecnt_t                     dur,
                     Evoral::Range<samplepos_t>*     loop_range,
                     MidiCursor&                    cursor,
                     uint32_t                       chan_n,
                     NoteMode                       mode,
                     MidiStateTracker*              tracker,
                     MidiChannelFilter*             filter) const
{
	return _read_at (_sources, out, position, dur, loop_range, cursor, chan_n, mode, tracker, filter);
}

samplecnt_t
MidiRegion::master_read_at (MidiRingBuffer<samplepos_t>& out,
                            samplepos_t                  position,
                            samplecnt_t                  dur,
                            Evoral::Range<samplepos_t>*  loop_range,
                            MidiCursor&                 cursor,
                            uint32_t                    chan_n,
                            NoteMode                    mode) const
{
	return _read_at (_master_sources, out, position, dur, loop_range, cursor, chan_n, mode); /* no tracker */
}

samplecnt_t
MidiRegion::_read_at (const SourceList&              /*srcs*/,
                      Evoral::EventSink<samplepos_t>& dst,
                      samplepos_t                     position,
                      samplecnt_t                     dur,
                      Evoral::Range<samplepos_t>*     loop_range,
                      MidiCursor&                    cursor,
                      uint32_t                       chan_n,
                      NoteMode                       mode,
                      MidiStateTracker*              tracker,
                      MidiChannelFilter*             filter) const
{
	sampleoffset_t internal_offset = 0;
	samplecnt_t    to_read         = 0;

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

#if 0
	cerr << "MR " << name () << " read @ " << position << " + " << to_read
	     << " dur was " << dur
	     << " len " << _length
	     << " l-io " << (_length - internal_offset)
	     << " _position = " << _position
	     << " _start = " << _start
	     << " intoffset = " << internal_offset
	     << " quarter_note = " << quarter_note()
	     << " start_beat = " << _start_beats
	     << endl;
#endif

	/* This call reads events from a source and writes them to `dst' timed in session samples */

	if (src->midi_read (
		    lm, // source lock
		    dst, // destination buffer
		    _position - _start, // start position of the source in session samples
		    _start + internal_offset, // where to start reading in the source
		    to_read, // read duration in samples
		    loop_range,
		    cursor,
		    tracker,
		    filter,
		    _filtered_parameters,
		    quarter_note(),
		    _start_beats
		    ) != to_read) {
		return 0; /* "read nothing" */
	}

	return to_read;
}


int
MidiRegion::render (Evoral::EventSink<samplepos_t>& dst,
                    uint32_t                        chan_n,
                    NoteMode                        mode,
                    MidiChannelFilter*              filter) const
{
	sampleoffset_t internal_offset = 0;

	/* precondition: caller has verified that we cover the desired section */

	assert(chan_n == 0);

	if (muted()) {
		return 0; /* read nothing */
	}


	/* dump pulls from zero to infinity ... */

	if (_position) {
		/* we are starting the read from before the start of the region */
		internal_offset = 0;
	} else {
		/* we are starting the read from after the start of the region */
		internal_offset = -_position;
	}

	if (internal_offset >= _length) {
		return 0; /* read nothing */
	}

	boost::shared_ptr<MidiSource> src = midi_source(chan_n);

	Glib::Threads::Mutex::Lock lm(src->mutex());

	src->set_note_mode(lm, mode);

#if 0
	cerr << "MR " << name () << " render "
	     << " _position = " << _position
	     << " _start = " << _start
	     << " intoffset = " << internal_offset
	     << " quarter_note = " << quarter_note()
	     << " start_beat = " << _start_beats
	     << " a1 " << _position - _start
	     << " a2 " << _start + internal_offset
	     << " a3 " << _length
	     << endl;
#endif

	MidiCursor cursor;
	MidiStateTracker tracker;

	/* This call reads events from a source and writes them to `dst' timed in session samples */

	src->midi_read (
		lm, // source lock
		dst, // destination buffer
		_position - _start, // start position of the source in session samples
		_start + internal_offset, // where to start reading in the source
		_length, // length to read
		0,
		cursor,
		&tracker,
		filter,
		_filtered_parameters,
		quarter_note(),
		_start_beats);

	/* resolve any notes that were "cut off" by the end of the region. The
	 * Note-Off's get inserted at the end of the region
	 */

	tracker.resolve_notes (dst, (_position - _start) + (_start + internal_offset + _length));

	return 0;
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
MidiRegion::separate_by_channel (vector< boost::shared_ptr<Region> >&) const
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

/* don't use this. hopefully it will go away.
   currently used by headless-chicken session utility.
*/
void
MidiRegion::clobber_sources (boost::shared_ptr<MidiSource> s)
{
       drop_sources();

       _sources.push_back (s);
       s->inc_use_count ();
       _master_sources.push_back (s);
       s->inc_use_count ();

       s->DropReferences.connect_same_thread (*this, boost::bind (&Region::source_deleted, this, boost::weak_ptr<Source>(s)));

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

	model()->ContentsShifted.connect_same_thread (_model_shift_connection, boost::bind (&MidiRegion::model_shifted, this, _1));
	model()->ContentsChanged.connect_same_thread (_model_changed_connection, boost::bind (&MidiRegion::model_contents_changed, this));
}

void
MidiRegion::model_contents_changed ()
{
	send_change (Properties::contents);
}

void
MidiRegion::model_shifted (double qn_distance)
{
	if (!model()) {
		return;
	}

	if (!_ignore_shift) {
		PropertyChange what_changed;
		_start_beats += qn_distance;
		samplepos_t const new_start = _session.tempo_map().samples_between_quarter_notes (_quarter_note - _start_beats, _quarter_note);
		_start = new_start;
		what_changed.add (Properties::start);
		what_changed.add (Properties::start_beats);
		what_changed.add (Properties::contents);
		send_change (what_changed);
	} else {
		_ignore_shift = false;
	}
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
	BeatsSamplesConverter c (_session.tempo_map(), _position);

	_ignore_shift = true;

	model()->insert_silence_at_start (Temporal::Beats (- _start_beats));

	_start = 0;
	_start_beats = 0.0;
}

void
MidiRegion::set_start_internal (samplecnt_t s, const int32_t sub_num)
{
	Region::set_start_internal (s, sub_num);

	set_start_beats_from_start_samples ();
}

void
MidiRegion::trim_to_internal (timepos_t const &  position, samplecnt_t length, const int32_t sub_num)
{
	if (locked()) {
		return;
	}

	PropertyChange what_changed;


	/* Set position before length, otherwise for MIDI regions this bad thing happens:
	 * 1. we call set_length_internal; length in beats is computed using the region's current
	 *    (soon-to-be old) position
	 * 2. we call set_position_internal; position is set and length in samples re-computed using
	 *    length in beats from (1) but at the new position, which is wrong if the region
	 *    straddles a tempo/meter change.
	 */

	if (_position != position) {

		const double pos_qn = _session.tempo_map().exact_qn_at_sample (position, sub_num);
		const double old_pos_qn = quarter_note();

		/* sets _pulse to new position.*/
		set_position_internal (position, true, sub_num);
		what_changed.add (Properties::position);

		double new_start_qn = start_beats() + (pos_qn - old_pos_qn);
		samplepos_t new_start = _session.tempo_map().samples_between_quarter_notes (pos_qn - new_start_qn, pos_qn);

		if (!verify_start_and_length (new_start, length)) {
			return;
		}

		_start_beats = new_start_qn;
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

bool
MidiRegion::set_name (const std::string& str)
{
	if (_name == str) {
		return true;
	}

	if (!Session::session_name_is_legal (str).empty ()) {
		return false;
	}

	return Region::set_name (str);
}
