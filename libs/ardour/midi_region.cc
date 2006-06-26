/*
    Copyright (C) 2006 Paul Davis 
	Written by Dave Robillard, 2006

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

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;

MidiRegionState::MidiRegionState (string why)
	: RegionState (why)
{
}

MidiRegion::MidiRegion (MidiSource& src, jack_nframes_t start, jack_nframes_t length, bool announce)
	: Region (start, length, PBD::basename_nosuffix(src.name()), 0,  Region::Flag(Region::DefaultFlags|Region::External))
{
	/* basic MidiRegion constructor */

	sources.push_back (&src);
	master_sources.push_back (&src);
	src.GoingAway.connect (mem_fun (*this, &MidiRegion::source_deleted));

	save_state ("initial state");

	if (announce) {
		 CheckNewRegion (this); /* EMIT SIGNAL */
	}
}

MidiRegion::MidiRegion (MidiSource& src, jack_nframes_t start, jack_nframes_t length, const string& name, layer_t layer, Flag flags, bool announce)
	: Region (start, length, name, layer, flags)
{
	/* basic MidiRegion constructor */

	sources.push_back (&src);
	master_sources.push_back (&src);
	src.GoingAway.connect (mem_fun (*this, &MidiRegion::source_deleted));

	save_state ("initial state");

	if (announce) {
		 CheckNewRegion (this); /* EMIT SIGNAL */
	}
}

MidiRegion::MidiRegion (SourceList& srcs, jack_nframes_t start, jack_nframes_t length, const string& name, layer_t layer, Flag flags, bool announce)
	: Region (start, length, name, layer, flags)
{
	/* basic MidiRegion constructor */
#if 0
	for (SourceList::iterator i=srcs.begin(); i != srcs.end(); ++i) {
		sources.push_back (*i);
		master_sources.push_back (*i);
		(*i)->GoingAway.connect (mem_fun (*this, &MidiRegion::source_deleted));
	}

{
	/* create a new MidiRegion, that is part of an existing one */
	
	set<MidiSource*> unique_srcs;

	for (SourceList::const_iterator i= other.sources.begin(); i != other.sources.end(); ++i) {
		sources.push_back (*i);
		(*i)->GoingAway.connect (mem_fun (*this, &MidiRegion::source_deleted));
		unique_srcs.insert (*i);
	}

	for (SourceList::const_iterator i = other.master_sources.begin(); i != other.master_sources.end(); ++i) {
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->GoingAway.connect (mem_fun (*this, &MidiRegion::source_deleted));
		}
		master_sources.push_back (*i);
	}

	save_state ("initial state");

	if (announce) {
		CheckNewRegion (this); /* EMIT SIGNAL */
	}
#endif
}

MidiRegion::MidiRegion (const MidiRegion &other)
	: Region (other)
{
	/* Pure copy constructor */

	set<MidiSource*> unique_srcs;

	for (SourceList::const_iterator i = other.sources.begin(); i != other.sources.end(); ++i) {
		sources.push_back (*i);
		(*i)->GoingAway.connect (mem_fun (*this, &MidiRegion::source_deleted));
		unique_srcs.insert (*i);
	}

	for (SourceList::const_iterator i = other.master_sources.begin(); i != other.master_sources.end(); ++i) {
		master_sources.push_back (*i);
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->GoingAway.connect (mem_fun (*this, &MidiRegion::source_deleted));
		}
	}

	save_state ("initial state");

	/* NOTE: no CheckNewRegion signal emitted here. This is the copy constructor */
}

MidiRegion::MidiRegion (MidiSource& src, const XMLNode& node)
	: Region (node)
{
	sources.push_back (&src);
	master_sources.push_back (&src);
	src.GoingAway.connect (mem_fun (*this, &MidiRegion::source_deleted));

	if (set_state (node)) {
		throw failed_constructor();
	}

	save_state ("initial state");

	CheckNewRegion (this); /* EMIT SIGNAL */
}

MidiRegion::MidiRegion (SourceList& srcs, const XMLNode& node)
	: Region (node)
{
	/* basic MidiRegion constructor */

	set<MidiSource*> unique_srcs;

	for (SourceList::iterator i=srcs.begin(); i != srcs.end(); ++i) {
		sources.push_back (*i);
		(*i)->GoingAway.connect (mem_fun (*this, &MidiRegion::source_deleted));
		unique_srcs.insert (*i);
	}

	for (SourceList::iterator i = srcs.begin(); i != srcs.end(); ++i) {
		master_sources.push_back (*i);
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->GoingAway.connect (mem_fun (*this, &MidiRegion::source_deleted));
		}
	}

	if (set_state (node)) {
		throw failed_constructor();
	}

	save_state ("initial state");

	CheckNewRegion (this); /* EMIT SIGNAL */
}

MidiRegion::~MidiRegion ()
{
	GoingAway (this);
}

StateManager::State*
MidiRegion::state_factory (std::string why) const
{
	MidiRegionState* state = new MidiRegionState (why);

	Region::store_state (*state);

	return state;
}	

Change
MidiRegion::restore_state (StateManager::State& sstate) 
{
	MidiRegionState* state = dynamic_cast<MidiRegionState*> (&sstate);

	Change what_changed = Region::restore_and_return_flags (*state);
	
	if (_flags != Flag (state->_flags)) {
		
		//uint32_t old_flags = _flags;
		
		_flags = Flag (state->_flags);
		
	}
		
	/* XXX need a way to test stored state versus current for envelopes */

	what_changed = Change (what_changed);

	return what_changed;
}

UndoAction
MidiRegion::get_memento() const
{
	return sigc::bind (mem_fun (*(const_cast<MidiRegion *> (this)), &StateManager::use_state), _current_state_id);
}

bool
MidiRegion::verify_length (jack_nframes_t len)
{
	for (uint32_t n=0; n < sources.size(); ++n) {
		if (_start > sources[n]->length() - len) {
			return false;
		}
	}
	return true;
}

bool
MidiRegion::verify_start_and_length (jack_nframes_t new_start, jack_nframes_t new_length)
{
	for (uint32_t n=0; n < sources.size(); ++n) {
		if (new_length > sources[n]->length() - new_start) {
			return false;
		}
	}
	return true;
}
bool
MidiRegion::verify_start (jack_nframes_t pos)
{
	for (uint32_t n=0; n < sources.size(); ++n) {
		if (pos > sources[n]->length() - _length) {
			return false;
		}
	}
	return true;
}

bool
MidiRegion::verify_start_mutable (jack_nframes_t& new_start)
{
	for (uint32_t n=0; n < sources.size(); ++n) {
		if (new_start > sources[n]->length() - _length) {
			new_start = sources[n]->length() - _length;
		}
	}
	return true;
}

jack_nframes_t
MidiRegion::read_at (unsigned char *buf, unsigned char *mixdown_buffer, char * workbuf, jack_nframes_t position, 
		      jack_nframes_t cnt, 
		      uint32_t chan_n, jack_nframes_t read_frames, jack_nframes_t skip_frames) const
{
	return _read_at (sources, buf, mixdown_buffer, workbuf, position, cnt, chan_n, read_frames, skip_frames);
}

jack_nframes_t
MidiRegion::master_read_at (unsigned char *buf, unsigned char *mixdown_buffer, char * workbuf, jack_nframes_t position, 
			     jack_nframes_t cnt, uint32_t chan_n) const
{
	return _read_at (master_sources, buf, mixdown_buffer, workbuf, position, cnt, chan_n, 0, 0);
}

jack_nframes_t
MidiRegion::_read_at (const SourceList& srcs, unsigned char *buf, unsigned char *mixdown_buffer, char * workbuf,
		       jack_nframes_t position, jack_nframes_t cnt, 
		       uint32_t chan_n, jack_nframes_t read_frames, jack_nframes_t skip_frames) const
{
	jack_nframes_t internal_offset;
	jack_nframes_t buf_offset;
	jack_nframes_t to_read;
	
	/* precondition: caller has verified that we cover the desired section */

	if (chan_n >= sources.size()) {
		return 0; /* read nothing */
	}
	
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

	if (opaque()) {
		/* overwrite whatever is there */
		mixdown_buffer = buf + buf_offset;
	} else {
		mixdown_buffer += buf_offset;
	}

	if (muted()) {
		return 0; /* read nothing */
	}

	_read_data_count = 0;

	if (srcs[chan_n]->read (mixdown_buffer, _start + internal_offset, to_read, workbuf) != to_read) {
		return 0; /* "read nothing" */
	}

	_read_data_count += srcs[chan_n]->read_data_count();

	if (!opaque()) {

		/* gack. the things we do for users.
		 */

		buf += buf_offset;

		for (jack_nframes_t n = 0; n < to_read; ++n) {
			buf[n] += mixdown_buffer[n];
		}
	} 
	
	return to_read;
}
	
XMLNode&
MidiRegion::get_state ()
{
	return state (true);
}

XMLNode&
MidiRegion::state (bool full)
{
	XMLNode& node (Region::state (full));
	//XMLNode *child;
	char buf[64];
	char buf2[64];
	LocaleGuard lg (X_("POSIX"));
	
	snprintf (buf, sizeof (buf), "0x%x", (int) _flags);
	node.add_property ("flags", buf);

	for (uint32_t n=0; n < sources.size(); ++n) {
		snprintf (buf2, sizeof(buf2), "source-%d", n);
		snprintf (buf, sizeof(buf), "%" PRIu64, sources[n]->id());
		node.add_property (buf2, buf);
	}

	snprintf (buf, sizeof (buf), "%u", (uint32_t) sources.size());
	node.add_property ("channels", buf);

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

		_flags = Flag (_flags & ~Region::LeftOfSplit);
		_flags = Flag (_flags & ~Region::RightOfSplit);
	}

	/* Now find envelope description and other misc child items */
				
	for (XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {
		
		XMLNode *child;
		//XMLProperty *prop;
		
		child = (*niter);
	}

	return 0;
}

int
MidiRegion::separate_by_channel (Session& session, vector<MidiRegion*>& v) const
{
	SourceList srcs;
	string new_name;

	for (SourceList::const_iterator i = master_sources.begin(); i != master_sources.end(); ++i) {

		srcs.clear ();
		srcs.push_back (*i);

		/* generate a new name */
		
		if (session.region_name (new_name, _name)) {
			return -1;
		}

		/* create a copy with just one source */

		v.push_back (new MidiRegion (srcs, _start, _length, new_name, _layer, _flags));
	}

	return 0;
}

void
MidiRegion::source_deleted (Source* ignored)
{
	delete this;
}

void
MidiRegion::lock_sources ()
{
	SourceList::iterator i;
	set<MidiSource*> unique_srcs;

	for (i = sources.begin(); i != sources.end(); ++i) {
		unique_srcs.insert (*i);
		(*i)->use ();
	}

	for (i = master_sources.begin(); i != master_sources.end(); ++i) {
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->use ();
		}
	}
}

void
MidiRegion::unlock_sources ()
{
	SourceList::iterator i;
	set<MidiSource*> unique_srcs;

	for (i = sources.begin(); i != sources.end(); ++i) {
		unique_srcs.insert (*i);
		(*i)->release ();
	}

	for (i = master_sources.begin(); i != master_sources.end(); ++i) {
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->release ();
		}
	}
}

vector<string>
MidiRegion::master_source_names ()
{
	SourceList::iterator i;

	vector<string> names;
	for (i = master_sources.begin(); i != master_sources.end(); ++i) {
		names.push_back((*i)->name());
	}

	return names;
}

bool
MidiRegion::region_list_equivalent (const MidiRegion& other) const
{
	return size_equivalent (other) && source_equivalent (other) && _name == other._name;
}

bool
MidiRegion::source_equivalent (const MidiRegion& other) const
{
	SourceList::const_iterator i;
	SourceList::const_iterator io;

	for (i = sources.begin(), io = other.sources.begin(); i != sources.end() && io != other.sources.end(); ++i, ++io) {
		if ((*i)->id() != (*io)->id()) {
			return false;
		}
	}

	for (i = master_sources.begin(), io = other.master_sources.begin(); i != master_sources.end() && io != other.master_sources.end(); ++i, ++io) {
		if ((*i)->id() != (*io)->id()) {
			return false;
		}
	}

	return true;
}

bool
MidiRegion::overlap_equivalent (const MidiRegion& other) const
{
	return coverage (other.first_frame(), other.last_frame()) != OverlapNone;
}

bool
MidiRegion::equivalent (const MidiRegion& other) const
{
	return _start == other._start &&
		_position == other._position &&
		_length == other._length;
}

bool
MidiRegion::size_equivalent (const MidiRegion& other) const
{
	return _start == other._start &&
		_length == other._length;
}

#if 0
int
MidiRegion::exportme (Session& session, AudioExportSpecification& spec)
{
	const jack_nframes_t blocksize = 4096;
	jack_nframes_t to_read;
	int status = -1;

	spec.channels = sources.size();

	if (spec.prepare (blocksize, session.frame_rate())) {
		goto out;
	}

	spec.pos = 0;
	spec.total_frames = _length;

	while (spec.pos < _length && !spec.stop) {
		
		
		/* step 1: interleave */
		
		to_read = min (_length - spec.pos, blocksize);
		
		if (spec.channels == 1) {

			if (sources.front()->read (spec.dataF, _start + spec.pos, to_read, 0) != to_read) {
				goto out;
			}

		} else {

			Sample buf[blocksize];

			for (uint32_t chan = 0; chan < spec.channels; ++chan) {
				
				if (sources[chan]->read (buf, _start + spec.pos, to_read, 0) != to_read) {
					goto out;
				}
				
				for (jack_nframes_t x = 0; x < to_read; ++x) {
					spec.dataF[chan+(x*spec.channels)] = buf[x];
				}
			}
		}
		
		if (spec.process (to_read)) {
			goto out;
		}
		
		spec.pos += to_read;
		spec.progress = (double) spec.pos /_length;
		
	}
	
	status = 0;

  out:	
	spec.running = false;
	spec.status = status;
	spec.clear();
	
	return status;
}
#endif

Region*
MidiRegion::get_parent()
{
#if 0
	Region* r = 0;

	if (_playlist) {
		r = _playlist->session().find_whole_file_parent (*this);
	}
	
	return r;
#endif
	return NULL;
}


bool
MidiRegion::speed_mismatch (float sr) const
{
#if 0
	if (sources.empty()) {
		/* impossible, but ... */
		return false;
	}

	float fsr = sources.front()->sample_rate();

	return fsr == sr;
#endif
	return false;
}

