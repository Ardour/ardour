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


#include <glibmm/thread.h>

#include "pbd/basename.h"
#include "pbd/xml++.h"
#include "pbd/enumwriter.h"

#include "ardour/midi_region.h"
#include "ardour/session.h"
#include "ardour/gain.h"
#include "ardour/dB.h"
#include "ardour/playlist.h"
#include "ardour/midi_source.h"
#include "ardour/region_factory.h"
#include "ardour/types.h"
#include "ardour/midi_ring_buffer.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/* Basic MidiRegion constructor (many channels) */
MidiRegion::MidiRegion (const SourceList& srcs)
	: Region (srcs)
{
	midi_source(0)->Switched.connect_same_thread (*this, boost::bind (&MidiRegion::switch_source, this, _1));
	assert(_name.val().find("/") == string::npos);
	assert(_type == DataType::MIDI);
}

/** Create a new MidiRegion, that is part of an existing one */
MidiRegion::MidiRegion (boost::shared_ptr<const MidiRegion> other, frameoffset_t offset, bool offset_relative)
	: Region (other, offset, offset_relative)
{
	assert(_name.val().find("/") == string::npos);
	midi_source(0)->Switched.connect_same_thread (*this, boost::bind (&MidiRegion::switch_source, this, _1));
}

MidiRegion::~MidiRegion ()
{
}

/** Create a new MidiRegion that has its own version of some/all of the Source used by another. 
 */
boost::shared_ptr<MidiRegion>
MidiRegion::clone ()
{
        BeatsFramesConverter bfc (_session.tempo_map(), _position);
        double bbegin = bfc.from (_position);
        double bend = bfc.from (last_frame() + 1);

        boost::shared_ptr<MidiSource> ms = midi_source(0)->clone (bbegin, bend);

        PropertyList plist;

        plist.add (Properties::name, ms->name());
        plist.add (Properties::whole_file, true);
        plist.add (Properties::start, 0);
        plist.add (Properties::length, _length);
        plist.add (Properties::layer, 0);

        return boost::dynamic_pointer_cast<MidiRegion> (RegionFactory::create (ms, plist, true));
}

void
MidiRegion::set_position_internal (framepos_t pos, bool allow_bbt_recompute)
{
	BeatsFramesConverter old_converter(_session.tempo_map(), _position - _start);
	double length_beats = old_converter.from(_length);

	Region::set_position_internal(pos, allow_bbt_recompute);

	BeatsFramesConverter new_converter(_session.tempo_map(), pos - _start);

	set_length(new_converter.to(length_beats), 0);
}

framecnt_t
MidiRegion::read_at (Evoral::EventSink<nframes_t>& out, framepos_t position, framecnt_t dur, uint32_t chan_n, NoteMode mode, MidiStateTracker* tracker) const
{
	return _read_at (_sources, out, position, dur, chan_n, mode, tracker);
}

framecnt_t
MidiRegion::master_read_at (MidiRingBuffer<nframes_t>& out, framepos_t position, framecnt_t dur, uint32_t chan_n, NoteMode mode) const
{
	return _read_at (_master_sources, out, position, dur, chan_n, mode); /* no tracker */
}

framecnt_t
MidiRegion::_read_at (const SourceList& /*srcs*/, Evoral::EventSink<nframes_t>& dst, framepos_t position, framecnt_t dur, uint32_t chan_n, 
		      NoteMode mode, MidiStateTracker* tracker) const
{
	frameoffset_t internal_offset = 0;
	frameoffset_t src_offset      = 0;
	framecnt_t to_read         = 0;

	/* precondition: caller has verified that we cover the desired section */

	assert(chan_n == 0);

	if (muted()) {
		return 0; /* read nothing */
	}

	if (position < _position) {
		internal_offset = 0;
		src_offset = _position - position;
		dur -= src_offset;
	} else {
		internal_offset = position - _position;
		src_offset = 0;
	}

	if (internal_offset >= _length) {
		return 0; /* read nothing */
	}

	if ((to_read = min (dur, _length - internal_offset)) == 0) {
		return 0; /* read nothing */
	}

	_read_data_count = 0;

	boost::shared_ptr<MidiSource> src = midi_source(chan_n);
	src->set_note_mode(mode);

	framepos_t output_buffer_position = 0;
	framepos_t negative_output_buffer_position = 0;
	if (_position >= _start) {
		// handle resizing of beginnings of regions correctly
		output_buffer_position = _position - _start;
	} else {
		// when _start is greater than _position, we have to subtract
		// _start from the note times in the midi source
		negative_output_buffer_position = _start;
	}

	/*cerr << "MR read @ " << position << " * " << to_read
		<< " _position = " << _position
	    << " _start = " << _start
	    << " offset = " << output_buffer_position
	    << " negoffset = " << negative_output_buffer_position
	    << " intoffset = " << internal_offset
	    << endl;*/

	if (src->midi_read (
			dst, // destination buffer
			_position - _start, // start position of the source in this read context
			_start + internal_offset, // where to start reading in the source
			to_read, // read duration in frames
			output_buffer_position, // the offset in the output buffer
			negative_output_buffer_position, // amount to substract from note times
			tracker
		    ) != to_read) {
		return 0; /* "read nothing" */
	}

	_read_data_count += src->read_data_count();

	return to_read;
}

XMLNode&
MidiRegion::state (bool full)
{
	XMLNode& node (Region::state (full));
	char buf[64];
	char buf2[64];
	LocaleGuard lg (X_("POSIX"));

	// XXX these should move into Region

	for (uint32_t n=0; n < _sources.size(); ++n) {
		snprintf (buf2, sizeof(buf2), "source-%d", n);
		_sources[n]->id().print (buf, sizeof(buf));
		node.add_property (buf2, buf);
	}

	for (uint32_t n=0; n < _master_sources.size(); ++n) {
		snprintf (buf2, sizeof(buf2), "master-source-%d", n);
		_master_sources[n]->id().print (buf, sizeof (buf));
		node.add_property (buf2, buf);
	}

	if (full && _extra_xml) {
		node.add_child_copy (*_extra_xml);
	}

	return node;
}

int
MidiRegion::set_state (const XMLNode& node, int version)
{
	return Region::set_state (node, version);
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

int
MidiRegion::exportme (ARDOUR::Session&, ARDOUR::ExportSpecification&)
{
	return -1;
}

boost::shared_ptr<MidiSource>
MidiRegion::midi_source (uint32_t n) const
{
	// Guaranteed to succeed (use a static cast?)
	return boost::dynamic_pointer_cast<MidiSource>(source(n));
}


void
MidiRegion::switch_source(boost::shared_ptr<Source> src)
{
	boost::shared_ptr<MidiSource> msrc = boost::dynamic_pointer_cast<MidiSource>(src);
	if (!msrc)
		return;

	// MIDI regions have only one source
	_sources.clear();
	_sources.push_back(msrc);

	set_name(msrc->name());
}

