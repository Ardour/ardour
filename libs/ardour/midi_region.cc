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

#include <sigc++/bind.h>
#include <sigc++/class_slot.h>

#include <glibmm/thread.h>

#include <pbd/basename.h>
#include <pbd/xml++.h>

#include <ardour/midi_region.h>
#include <ardour/session.h>
#include <ardour/gain.h>
#include <ardour/dB.h>
#include <ardour/playlist.h>
#include <ardour/midi_source.h>
#include <ardour/types.h>

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;

/** Basic MidiRegion constructor (one channel) */
MidiRegion::MidiRegion (MidiSource& src, jack_nframes_t start, jack_nframes_t length, bool announce)
	: Region (src, start, length, PBD::basename_nosuffix(src.name()), DataType::MIDI, 0,  Region::Flag(Region::DefaultFlags|Region::External))
{
	save_state ("initial state");

	if (announce) {
		 CheckNewRegion (this); /* EMIT SIGNAL */
	}
}

/* Basic MidiRegion constructor (one channel) */
MidiRegion::MidiRegion (MidiSource& src, jack_nframes_t start, jack_nframes_t length, const string& name, layer_t layer, Flag flags, bool announce)
	: Region (src, start, length, name, DataType::MIDI, layer, flags)
{
	save_state ("initial state");

	if (announce) {
		 CheckNewRegion (this); /* EMIT SIGNAL */
	}
}

/* Basic MidiRegion constructor (many channels) */
MidiRegion::MidiRegion (SourceList& srcs, jack_nframes_t start, jack_nframes_t length, const string& name, layer_t layer, Flag flags, bool announce)
	: Region (srcs, start, length, name, DataType::MIDI, layer, flags)
{
	save_state ("initial state");

	if (announce) {
		 CheckNewRegion (this); /* EMIT SIGNAL */
	}
}


/** Create a new MidiRegion, that is part of an existing one */
MidiRegion::MidiRegion (const MidiRegion& other, jack_nframes_t offset, jack_nframes_t length, const string& name, layer_t layer, Flag flags, bool announce)
	: Region (other, offset, length, name, layer, flags)
{
	save_state ("initial state");

	if (announce) {
		CheckNewRegion (this); /* EMIT SIGNAL */
	}
}

MidiRegion::MidiRegion (const MidiRegion &other)
	: Region (other)
{
	save_state ("initial state");

	/* NOTE: no CheckNewRegion signal emitted here. This is the copy constructor */
}

MidiRegion::MidiRegion (MidiSource& src, const XMLNode& node)
	: Region (src, node)
{
	if (set_state (node)) {
		throw failed_constructor();
	}

	save_state ("initial state");

	assert(_type == DataType::MIDI);

	CheckNewRegion (this); /* EMIT SIGNAL */
}

MidiRegion::MidiRegion (SourceList& srcs, const XMLNode& node)
	: Region (srcs, node)
{
	if (set_state (node)) {
		throw failed_constructor();
	}

	save_state ("initial state");

	assert(_type == DataType::MIDI);

	CheckNewRegion (this); /* EMIT SIGNAL */
}

MidiRegion::~MidiRegion ()
{
	GoingAway (this);
}

StateManager::State*
MidiRegion::state_factory (std::string why) const
{
	RegionState* state = new RegionState (why);

	Region::store_state (*state);

	return state;
}	

Change
MidiRegion::restore_state (StateManager::State& sstate) 
{
	RegionState* state = dynamic_cast<RegionState*> (&sstate);
	Change what_changed = Region::restore_and_return_flags (*state);
	
	if (_flags != Flag (state->_flags)) {
		
		//uint32_t old_flags = _flags;
		
		_flags = Flag (state->_flags);
	}
		
	what_changed = Change (what_changed);

	return what_changed;
}

UndoAction
MidiRegion::get_memento() const
{
	return sigc::bind (mem_fun (*(const_cast<MidiRegion *> (this)), &StateManager::use_state), _current_state_id);
}

jack_nframes_t
MidiRegion::read_at (RawMidi *out, RawMidi* mix_buf, jack_nframes_t position, 
		      jack_nframes_t cnt, 
		      uint32_t chan_n, jack_nframes_t read_frames, jack_nframes_t skip_frames) const
{
	return _read_at (_sources, out, position, cnt, chan_n, read_frames, skip_frames);
}

jack_nframes_t
MidiRegion::master_read_at (RawMidi *out, RawMidi* mix_buf, jack_nframes_t position, 
			     jack_nframes_t cnt, uint32_t chan_n) const
{
	return _read_at (_master_sources, out, position, cnt, chan_n, 0, 0);
}

jack_nframes_t
MidiRegion::_read_at (const SourceList& srcs, RawMidi *buf, 
		       jack_nframes_t position, jack_nframes_t cnt, 
		       uint32_t chan_n, jack_nframes_t read_frames, jack_nframes_t skip_frames) const
{
	jack_nframes_t internal_offset;
	jack_nframes_t buf_offset;
	jack_nframes_t to_read;
	
	/* precondition: caller has verified that we cover the desired section */

	assert(chan_n == 0);
	
	if (position < _position) {
		internal_offset = 0;
		buf_offset = _position - position;
		cnt -= buf_offset;
	} else {
		internal_offset = position - _position;
		buf_offset = 0;
	}

	if (internal_offset >= _length) {
		return 0; /* read nothing */
	}
	

	if ((to_read = min (cnt, _length - internal_offset)) == 0) {
		return 0; /* read nothing */
	}

	// FIXME: non-opaque MIDI regions not yet supported
	assert(opaque());

	if (muted()) {
		return 0; /* read nothing */
	}

	_read_data_count = 0;

	MidiSource& src = midi_source(chan_n);
	if (src.read (buf, _start + internal_offset, to_read) != to_read) {
		return 0; /* "read nothing" */
	}

	_read_data_count += src.read_data_count();

	return to_read;
}
	
XMLNode&
MidiRegion::state (bool full)
{
	XMLNode& node (Region::state (full));
	XMLNode *child;
	char buf[64];
	char buf2[64];
	LocaleGuard lg (X_("POSIX"));
	
	snprintf (buf, sizeof (buf), "0x%x", (int) _flags);
	node.add_property ("flags", buf);

	for (uint32_t n=0; n < _sources.size(); ++n) {
		snprintf (buf2, sizeof(buf2), "source-%d", n);
		_sources[n]->id().print (buf);
		node.add_property (buf2, buf);
	}

	snprintf (buf, sizeof (buf), "%u", (uint32_t) _sources.size());
	node.add_property ("channels", buf);

	child = node.add_child ("Envelope");

	if ( ! full) {
		child->add_property ("default", "yes");
	}

	if (full && _extra_xml) {
		node.add_child_copy (*_extra_xml);
	}

	return node;
}

int
MidiRegion::set_state (const XMLNode& node)
{
	const XMLNodeList& nlist = node.children();
	const XMLProperty *prop;
	LocaleGuard lg (X_("POSIX"));

	Region::set_state (node);

	if ((prop = node.property ("flags")) != 0) {
		_flags = Flag (strtol (prop->value().c_str(), (char **) 0, 16));
	}

	/* Now find child items */
	for (XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {
		
		XMLNode *child;
		//XMLProperty *prop;
		
		child = (*niter);
		
		/** Hello, children */
	}

	return 0;
}

void
MidiRegion::recompute_at_end ()
{
	/* our length has changed
	 * (non destructively) "chop" notes that pass the end boundary, to
	 * prevent stuck notes.
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
MidiRegion::separate_by_channel (Session& session, vector<MidiRegion*>& v) const
{
#if 0
	SourceList srcs;
	string new_name;

	for (SourceList::const_iterator i = _master_sources.begin(); i != _master_sources.end(); ++i) {

		srcs.clear ();
		srcs.push_back (*i);

		/* generate a new name */
		
		if (session.region_name (new_name, _name)) {
			return -1;
		}

		/* create a copy with just one source */

		v.push_back (new MidiRegion (srcs, _start, _length, new_name, _layer, _flags));
	}
#endif

	// Actually, I would prefer not if that's alright
	return -1;
}

MidiSource&
MidiRegion::midi_source (uint32_t n) const
{
	// Guaranteed to succeed (use a static cast?)
	return dynamic_cast<MidiSource&>(source(n));
}

