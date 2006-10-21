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

    $Id$
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
#include <pbd/stacktrace.h>

#include <ardour/audioregion.h>
#include <ardour/session.h>
#include <ardour/gain.h>
#include <ardour/dB.h>
#include <ardour/playlist.h>
#include <ardour/audiofilter.h>
#include <ardour/audiofilesource.h>
#include <ardour/destructive_filesource.h>

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;

/* a Session will reset these to its chosen defaults by calling AudioRegion::set_default_fade() */

Change AudioRegion::FadeInChanged         = ARDOUR::new_change();
Change AudioRegion::FadeOutChanged        = ARDOUR::new_change();
Change AudioRegion::FadeInActiveChanged   = ARDOUR::new_change();
Change AudioRegion::FadeOutActiveChanged  = ARDOUR::new_change();
Change AudioRegion::EnvelopeActiveChanged = ARDOUR::new_change();
Change AudioRegion::ScaleAmplitudeChanged = ARDOUR::new_change();
Change AudioRegion::EnvelopeChanged       = ARDOUR::new_change();

AudioRegionState::AudioRegionState (string why)
	: RegionState (why)
	, _fade_in (0.0, 2.0, 1.0, false)
	, _fade_out (0.0, 2.0, 1.0, false)
	, _envelope (0.0, 2.0, 1.0, false)
{
}

/** Basic AudioRegion constructor (one channel) */
AudioRegion::AudioRegion (boost::shared_ptr<AudioSource> src, nframes_t start, nframes_t length)
	: Region (src, start, length, PBD::basename_nosuffix(src->name()), DataType::AUDIO, 0,  Region::Flag(Region::DefaultFlags|Region::External)),
	  _fade_in (0.0, 2.0, 1.0, false),
	  _fade_out (0.0, 2.0, 1.0, false),
	  _envelope (0.0, 2.0, 1.0, false)
{
	boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> (src);
	if (afs) {
		afs->HeaderPositionOffsetChanged.connect (mem_fun (*this, &AudioRegion::source_offset_changed));
	}

	_scale_amplitude = 1.0;

	set_default_fades ();
	set_default_envelope ();

	save_state ("initial state");

	_envelope.StateChanged.connect (mem_fun (*this, &AudioRegion::envelope_changed));
}

/* Basic AudioRegion constructor (one channel) */
AudioRegion::AudioRegion (boost::shared_ptr<AudioSource> src, nframes_t start, nframes_t length, const string& name, layer_t layer, Flag flags)
	: Region (src, start, length, name, DataType::AUDIO, layer, flags)
	, _fade_in (0.0, 2.0, 1.0, false)
	, _fade_out (0.0, 2.0, 1.0, false)
	, _envelope (0.0, 2.0, 1.0, false)
{
	boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> (src);
	if (afs) {
		afs->HeaderPositionOffsetChanged.connect (mem_fun (*this, &AudioRegion::source_offset_changed));
	}

	_scale_amplitude = 1.0;

	set_default_fades ();
	set_default_envelope ();
	save_state ("initial state");

	_envelope.StateChanged.connect (mem_fun (*this, &AudioRegion::envelope_changed));
}

/* Basic AudioRegion constructor (many channels) */
AudioRegion::AudioRegion (SourceList& srcs, nframes_t start, nframes_t length, const string& name, layer_t layer, Flag flags)
	: Region (srcs, start, length, name, DataType::AUDIO, layer, flags)
	, _fade_in (0.0, 2.0, 1.0, false)
	, _fade_out (0.0, 2.0, 1.0, false)
	, _envelope (0.0, 2.0, 1.0, false)
{
	_scale_amplitude = 1.0;

	set_default_fades ();
	set_default_envelope ();
	save_state ("initial state");

	_envelope.StateChanged.connect (mem_fun (*this, &AudioRegion::envelope_changed));
}


/** Create a new AudioRegion, that is part of an existing one */
AudioRegion::AudioRegion (boost::shared_ptr<const AudioRegion> other, nframes_t offset, nframes_t length, const string& name, layer_t layer, Flag flags)
	: Region (other, offset, length, name, layer, flags),
	  _fade_in (other->_fade_in),
	  _fade_out (other->_fade_out),
	  _envelope (other->_envelope, (double) offset, (double) offset + length) 
{
	/* return to default fades if the existing ones are too long */
	_fade_in_disabled = 0;
	_fade_out_disabled = 0;


	if (_flags & LeftOfSplit) {
		if (_fade_in.back()->when >= _length) {
			set_default_fade_in ();
		} else {
			_fade_in_disabled = other->_fade_in_disabled;
		}
		set_default_fade_out ();
		_flags = Flag (_flags & ~Region::LeftOfSplit);
	}

	if (_flags & RightOfSplit) {
		if (_fade_out.back()->when >= _length) {
			set_default_fade_out ();
		} else {
			_fade_out_disabled = other->_fade_out_disabled;
		}
		set_default_fade_in ();
		_flags = Flag (_flags & ~Region::RightOfSplit);
	}

	_scale_amplitude = other->_scale_amplitude;

	save_state ("initial state");

	_envelope.StateChanged.connect (mem_fun (*this, &AudioRegion::envelope_changed));
	
	assert(_type == DataType::AUDIO);
}

AudioRegion::AudioRegion (boost::shared_ptr<const AudioRegion> other)
	: Region (other),
	  _fade_in (other->_fade_in),
	  _fade_out (other->_fade_out),
	  _envelope (other->_envelope) 
{
	_scale_amplitude = other->_scale_amplitude;
	_envelope = other->_envelope;

	_fade_in_disabled = 0;
	_fade_out_disabled = 0;
	
	save_state ("initial state");

	_envelope.StateChanged.connect (mem_fun (*this, &AudioRegion::envelope_changed));

	assert(_type == DataType::AUDIO);
}

AudioRegion::AudioRegion (boost::shared_ptr<AudioSource> src, const XMLNode& node)
	: Region (src, node)
	, _fade_in (0.0, 2.0, 1.0, false)
	, _fade_out (0.0, 2.0, 1.0, false)
	, _envelope (0.0, 2.0, 1.0, false)
{
	boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> (src);
	if (afs) {
		afs->HeaderPositionOffsetChanged.connect (mem_fun (*this, &AudioRegion::source_offset_changed));
	}

	set_default_fades ();

	if (set_state (node)) {
		throw failed_constructor();
	}

	save_state ("initial state");

	_envelope.StateChanged.connect (mem_fun (*this, &AudioRegion::envelope_changed));

	assert(_type == DataType::AUDIO);
}

AudioRegion::AudioRegion (SourceList& srcs, const XMLNode& node)
	: Region (srcs, node),
	  _fade_in (0.0, 2.0, 1.0, false),
	  _fade_out (0.0, 2.0, 1.0, false),
	  _envelope (0.0, 2.0, 1.0, false)
{
	set_default_fades ();
	_scale_amplitude = 1.0;

	if (set_state (node)) {
		throw failed_constructor();
	}

	save_state ("initial state");

	_envelope.StateChanged.connect (mem_fun (*this, &AudioRegion::envelope_changed));

	assert(_type == DataType::AUDIO);
}

AudioRegion::~AudioRegion ()
{
	notify_callbacks ();
	GoingAway (); /* EMIT SIGNAL */
}

StateManager::State*
AudioRegion::state_factory (std::string why) const
{
	AudioRegionState* state = new AudioRegionState (why);

	Region::store_state (*state);

	state->_fade_in = _fade_in;
	state->_fade_out = _fade_out;
	state->_envelope = _envelope;
	state->_scale_amplitude = _scale_amplitude;
	state->_fade_in_disabled = _fade_in_disabled;
	state->_fade_out_disabled = _fade_out_disabled;

	return state;
}	

Change
AudioRegion::restore_state (StateManager::State& sstate) 
{
	AudioRegionState* state = dynamic_cast<AudioRegionState*> (&sstate);

	Change what_changed = Region::restore_and_return_flags (*state);
	
	if (_flags != Flag (state->_flags)) {
		
		uint32_t old_flags = _flags;
		
		_flags = Flag (state->_flags);
		
		if ((old_flags ^ state->_flags) & EnvelopeActive) {
			what_changed = Change (what_changed|EnvelopeActiveChanged);
		}
	}
		
	if (!(_fade_in == state->_fade_in)) {
		_fade_in = state->_fade_in;
		what_changed = Change (what_changed|FadeInChanged);
	}

	if (!(_fade_out == state->_fade_out)) {
		_fade_out = state->_fade_out;
		what_changed = Change (what_changed|FadeOutChanged);
	}

	if (_scale_amplitude != state->_scale_amplitude) {
		_scale_amplitude = state->_scale_amplitude;
		what_changed = Change (what_changed|ScaleAmplitudeChanged);
	}

	if (_fade_in_disabled != state->_fade_in_disabled) {
		if (_fade_in_disabled == 0 && state->_fade_in_disabled) {
			set_fade_in_active (false);
		} else if (_fade_in_disabled && state->_fade_in_disabled == 0) {
			set_fade_in_active (true);
		}
		_fade_in_disabled = state->_fade_in_disabled;
	}
		
	if (_fade_out_disabled != state->_fade_out_disabled) {
		if (_fade_out_disabled == 0 && state->_fade_out_disabled) {
			set_fade_out_active (false);
		} else if (_fade_out_disabled && state->_fade_out_disabled == 0) {
			set_fade_out_active (true);
		}
		_fade_out_disabled = state->_fade_out_disabled;
	}

	/* XXX need a way to test stored state versus current for envelopes */

	_envelope = state->_envelope;
	what_changed = Change (what_changed);

	return what_changed;
}

UndoAction
AudioRegion::get_memento() const
{
	return sigc::bind (mem_fun (*(const_cast<AudioRegion *> (this)), &StateManager::use_state), _current_state_id);
}

void
AudioRegion::set_envelope_active (bool yn)
{
	if (envelope_active() != yn) {
		char buf[64];
		if (yn) {
			snprintf (buf, sizeof (buf), "envelope active");
			_flags = Flag (_flags|EnvelopeActive);
		} else {
			snprintf (buf, sizeof (buf), "envelope off");
			_flags = Flag (_flags & ~EnvelopeActive);
		}
		if (!_frozen) {
			save_state (buf);
		}
		send_change (EnvelopeActiveChanged);

	}
}

ARDOUR::nframes_t
AudioRegion::read_peaks (PeakData *buf, nframes_t npeaks, nframes_t offset, nframes_t cnt, uint32_t chan_n, double samples_per_unit) const
{
	if (chan_n >= _sources.size()) {
		return 0; 
	}
	
	if (audio_source(chan_n)->read_peaks (buf, npeaks, offset, cnt, samples_per_unit)) {
		return 0;
	} else {
		if (_scale_amplitude != 1.0) {
			for (nframes_t n = 0; n < npeaks; ++n) {
				buf[n].max *= _scale_amplitude;
				buf[n].min *= _scale_amplitude;
			}
		}
		return cnt;
	}
}

ARDOUR::nframes_t
AudioRegion::read_at (Sample *buf, Sample *mixdown_buffer, float *gain_buffer, nframes_t position, 
		      nframes_t cnt, 
		      uint32_t chan_n, nframes_t read_frames, nframes_t skip_frames) const
{
	return _read_at (_sources, buf, mixdown_buffer, gain_buffer, position, cnt, chan_n, read_frames, skip_frames);
}

ARDOUR::nframes_t
AudioRegion::master_read_at (Sample *buf, Sample *mixdown_buffer, float *gain_buffer, nframes_t position, 
			     nframes_t cnt, uint32_t chan_n) const
{
	return _read_at (_master_sources, buf, mixdown_buffer, gain_buffer, position, cnt, chan_n, 0, 0);
}

ARDOUR::nframes_t
AudioRegion::_read_at (const SourceList& srcs, Sample *buf, Sample *mixdown_buffer, float *gain_buffer,
		       nframes_t position, nframes_t cnt, 
		       uint32_t chan_n, nframes_t read_frames, nframes_t skip_frames) const
{
	nframes_t internal_offset;
	nframes_t buf_offset;
	nframes_t to_read;

	/* precondition: caller has verified that we cover the desired section */

	if (chan_n >= _sources.size()) {
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

	boost::shared_ptr<AudioSource> src = audio_source(chan_n);
	if (src->read (mixdown_buffer, _start + internal_offset, to_read) != to_read) {
		return 0; /* "read nothing" */
	}

	_read_data_count += src->read_data_count();

	/* fade in */

	if (_flags & FadeIn) {

		nframes_t fade_in_length = (nframes_t) _fade_in.back()->when;
		
		/* see if this read is within the fade in */

		if (internal_offset < fade_in_length) {
			
			nframes_t limit;

			limit = min (to_read, fade_in_length - internal_offset);

			_fade_in.get_vector (internal_offset, internal_offset+limit, gain_buffer, limit);

			for (nframes_t n = 0; n < limit; ++n) {
				mixdown_buffer[n] *= gain_buffer[n];
			}
		}
	}
	
	/* fade out */

	if (_flags & FadeOut) {
	
		/* see if some part of this read is within the fade out */

		/* .................        >|            REGION
		                            _length
 					    
                                 {           }            FADE
				             fade_out_length
                                 ^					     
                               _length - fade_out_length
                        |--------------|
                        ^internal_offset
                                       ^internal_offset + to_read

                  we need the intersection of [internal_offset,internal_offset+to_read] with
                  [_length - fade_out_length, _length]

		*/

	
		nframes_t fade_out_length = (nframes_t) _fade_out.back()->when;
		nframes_t fade_interval_start = max(internal_offset, _length-fade_out_length);
		nframes_t fade_interval_end   = min(internal_offset + to_read, _length);

		if (fade_interval_end > fade_interval_start) {
			/* (part of the) the fade out is  in this buffer */
			
			nframes_t limit = fade_interval_end - fade_interval_start;
			nframes_t curve_offset = fade_interval_start - (_length-fade_out_length);
			nframes_t fade_offset = fade_interval_start - internal_offset;
								       
			_fade_out.get_vector (curve_offset,curve_offset+limit, gain_buffer, limit);

			for (nframes_t n = 0, m = fade_offset; n < limit; ++n, ++m) {
				mixdown_buffer[m] *= gain_buffer[n];
			}
		} 

	}

	/* Regular gain curves */

	if (envelope_active())  {
		_envelope.get_vector (internal_offset, internal_offset + to_read, gain_buffer, to_read);
		
		if (_scale_amplitude != 1.0f) {
			for (nframes_t n = 0; n < to_read; ++n) {
				mixdown_buffer[n] *= gain_buffer[n] * _scale_amplitude;
			}
		} else {
			for (nframes_t n = 0; n < to_read; ++n) {
				mixdown_buffer[n] *= gain_buffer[n];
			}
		}
	} else if (_scale_amplitude != 1.0f) {
		Session::apply_gain_to_buffer (mixdown_buffer, to_read, _scale_amplitude);
	}

	if (!opaque()) {

		/* gack. the things we do for users.
		 */

		buf += buf_offset;

		for (nframes_t n = 0; n < to_read; ++n) {
			buf[n] += mixdown_buffer[n];
		}
	} 
	
	return to_read;
}
	
XMLNode&
AudioRegion::state (bool full)
{
	XMLNode& node (Region::state (full));
	XMLNode *child;
	char buf[64];
	char buf2[64];
	LocaleGuard lg (X_("POSIX"));
	
	snprintf (buf, sizeof (buf), "0x%x", (int) _flags);
	node.add_property ("flags", buf);
	snprintf (buf, sizeof(buf), "%.12g", _scale_amplitude);
	node.add_property ("scale-gain", buf);

	for (uint32_t n=0; n < _sources.size(); ++n) {
		snprintf (buf2, sizeof(buf2), "source-%d", n);
		_sources[n]->id().print (buf, sizeof (buf));
		node.add_property (buf2, buf);
	}

	snprintf (buf, sizeof (buf), "%u", (uint32_t) _sources.size());
	node.add_property ("channels", buf);

	if (full) {
	
		child = node.add_child (X_("FadeIn"));
		
		if ((_flags & DefaultFadeIn)) {
			child->add_property (X_("default"), X_("yes"));
		} else {
			_fade_in.store_state (*child);
		}
		
		child = node.add_child (X_("FadeOut"));
		
		if ((_flags & DefaultFadeOut)) {
			child->add_property (X_("default"), X_("yes"));
		} else {
			_fade_out.store_state (*child);
		}
	}
	
	child = node.add_child ("Envelope");

	if (full) {
		bool default_env = false;
		
		// If there are only two points, the points are in the start of the region and the end of the region
		// so, if they are both at 1.0f, that means the default region.
		if (_envelope.size() == 2 &&
		    _envelope.front()->value == 1.0f &&
		    _envelope.back()->value==1.0f) {
			if (_envelope.front()->when == 0 && _envelope.back()->when == _length) {
				default_env = true;
			}
		} 
		
		if (default_env) {
			child->add_property ("default", "yes");
		} else {
			_envelope.store_state (*child);
		}
	} else {
		child->add_property ("default", "yes");
	}

	if (full && _extra_xml) {
		node.add_child_copy (*_extra_xml);
	}

	return node;
}

int
AudioRegion::set_state (const XMLNode& node)
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

	if ((prop = node.property ("scale-gain")) != 0) {
		_scale_amplitude = atof (prop->value().c_str());
	} else {
		_scale_amplitude = 1.0;
	}
	
	/* Now find envelope description and other misc child items */
				
	for (XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {
		
		XMLNode *child;
		XMLProperty *prop;
		
		child = (*niter);
		
		if (child->name() == "Envelope") {
			
			_envelope.clear ();

			if ((prop = child->property ("default")) != 0) {
				set_default_envelope ();
			} else {
				_envelope.load_state (*child);
			}

			_envelope.set_max_xval (_length);
			_envelope.truncate_end (_length);
			
		} else if (child->name() == "FadeIn") {
			
			_fade_in.clear ();
			
			if ((prop = child->property ("default")) != 0 || (prop = child->property ("steepness")) != 0) {
				set_default_fade_in ();
			} else {
				
				_fade_in.load_state (*child);
			}

		} else if (child->name() == "FadeOut") {
			
			_fade_out.clear ();

			if ((prop = child->property ("default")) != 0 || (prop = child->property ("steepness")) != 0) {
				set_default_fade_out ();
			} else {
				_fade_out.load_state (*child);
			}
		} 
	}

	return 0;
}

void
AudioRegion::set_fade_in_shape (FadeShape shape)
{
	set_fade_in (shape, (nframes_t) _fade_in.back()->when);
}

void
AudioRegion::set_fade_out_shape (FadeShape shape)
{
	set_fade_out (shape, (nframes_t) _fade_out.back()->when);
}

void
AudioRegion::set_fade_in (FadeShape shape, nframes_t len)
{
	_fade_in.freeze ();
	_fade_in.clear ();

	switch (shape) {
	case Linear:
		_fade_in.add (0.0, 0.0);
		_fade_in.add (len, 1.0);
		break;

	case Fast:
		_fade_in.add (0, 0);
		_fade_in.add (len * 0.389401, 0.0333333);
		_fade_in.add (len * 0.629032, 0.0861111);
		_fade_in.add (len * 0.829493, 0.233333);
		_fade_in.add (len * 0.9447, 0.483333);
		_fade_in.add (len * 0.976959, 0.697222);
		_fade_in.add (len, 1);
		break;

	case Slow:
		_fade_in.add (0, 0);
		_fade_in.add (len * 0.0207373, 0.197222);
		_fade_in.add (len * 0.0645161, 0.525);
		_fade_in.add (len * 0.152074, 0.802778);
		_fade_in.add (len * 0.276498, 0.919444);
		_fade_in.add (len * 0.481567, 0.980556);
		_fade_in.add (len * 0.767281, 1);
		_fade_in.add (len, 1);
		break;

	case LogA:
		_fade_in.add (0, 0);
		_fade_in.add (len * 0.0737327, 0.308333);
		_fade_in.add (len * 0.246544, 0.658333);
		_fade_in.add (len * 0.470046, 0.886111);
		_fade_in.add (len * 0.652074, 0.972222);
		_fade_in.add (len * 0.771889, 0.988889);
		_fade_in.add (len, 1);
		break;

	case LogB:
		_fade_in.add (0, 0);
		_fade_in.add (len * 0.304147, 0.0694444);
		_fade_in.add (len * 0.529954, 0.152778);
		_fade_in.add (len * 0.725806, 0.333333);
		_fade_in.add (len * 0.847926, 0.558333);
		_fade_in.add (len * 0.919355, 0.730556);
		_fade_in.add (len, 1);
		break;
	}

	_fade_in.thaw ();
	_fade_in_shape = shape;

	if (!_frozen) {
		save_state (_("fade in change"));
	}

	send_change (FadeInChanged);
}

void
AudioRegion::set_fade_out (FadeShape shape, nframes_t len)
{
	_fade_out.freeze ();
	_fade_out.clear ();

	switch (shape) {
	case Fast:
		_fade_out.add (len * 0, 1);
		_fade_out.add (len * 0.023041, 0.697222);
		_fade_out.add (len * 0.0553,   0.483333);
		_fade_out.add (len * 0.170507, 0.233333);
		_fade_out.add (len * 0.370968, 0.0861111);
		_fade_out.add (len * 0.610599, 0.0333333);
		_fade_out.add (len * 1, 0);
		break;

	case LogA:
		_fade_out.add (len * 0, 1);
		_fade_out.add (len * 0.228111, 0.988889);
		_fade_out.add (len * 0.347926, 0.972222);
		_fade_out.add (len * 0.529954, 0.886111);
		_fade_out.add (len * 0.753456, 0.658333);
		_fade_out.add (len * 0.9262673, 0.308333);
		_fade_out.add (len * 1, 0);
		break;

	case Slow:
		_fade_out.add (len * 0, 1);
		_fade_out.add (len * 0.305556, 1);
		_fade_out.add (len * 0.548611, 0.991736);
		_fade_out.add (len * 0.759259, 0.931129);
		_fade_out.add (len * 0.918981, 0.68595);
		_fade_out.add (len * 0.976852, 0.22865);
		_fade_out.add (len * 1, 0);
		break;

	case LogB:
		_fade_out.add (len * 0, 1);
		_fade_out.add (len * 0.080645, 0.730556);
		_fade_out.add (len * 0.277778, 0.289256);
		_fade_out.add (len * 0.470046, 0.152778);
		_fade_out.add (len * 0.695853, 0.0694444);
		_fade_out.add (len * 1, 0);
		break;

	case Linear:
		_fade_out.add (len * 0, 1);
		_fade_out.add (len * 1, 0);
		break;
	}

	_fade_out.thaw ();
	_fade_out_shape = shape;

	if (!_frozen) {
		save_state (_("fade in change"));
	}

	send_change (FadeOutChanged);
}

void
AudioRegion::set_fade_in_length (nframes_t len)
{
	bool changed = _fade_in.extend_to (len);

	if (changed) {
		_flags = Flag (_flags & ~DefaultFadeIn);

		if (!_frozen) {
			char buf[64];
			snprintf (buf, sizeof (buf), "fade in length changed to %u", len);
			save_state (buf);
		}
		
		send_change (FadeInChanged);
	}
}

void
AudioRegion::set_fade_out_length (nframes_t len)
{
	bool changed =	_fade_out.extend_to (len);

	if (changed) {
		_flags = Flag (_flags & ~DefaultFadeOut);
		
		if (!_frozen) {
			char buf[64];
			snprintf (buf, sizeof (buf), "fade out length changed to %u", len);
			save_state (buf);
		}
	}

	send_change (FadeOutChanged);
}

void
AudioRegion::set_fade_in_active (bool yn)
{
	if (yn == (_flags & FadeIn)) {
		return;
	}
	if (yn) {
		_flags = Flag (_flags|FadeIn);
	} else {
		_flags = Flag (_flags & ~FadeIn);
	}

	send_change (FadeInActiveChanged);
}

void
AudioRegion::set_fade_out_active (bool yn)
{
	if (yn == (_flags & FadeOut)) {
		return;
	}
	if (yn) {
		_flags = Flag (_flags | FadeOut);
	} else {
		_flags = Flag (_flags & ~FadeOut);
	}

	send_change (FadeOutActiveChanged);
}

void
AudioRegion::set_default_fade_in ()
{
	set_fade_in (Linear, 64);
}

void
AudioRegion::set_default_fade_out ()
{
	set_fade_out (Linear, 64);
}

void
AudioRegion::set_default_fades ()
{
	_fade_in_disabled = 0;
	_fade_out_disabled = 0;
	set_default_fade_in ();
	set_default_fade_out ();
}

void
AudioRegion::set_default_envelope ()
{
	_envelope.freeze ();
	_envelope.clear ();
	_envelope.add (0, 1.0f);
	_envelope.add (_length, 1.0f);
	_envelope.thaw ();
}

void
AudioRegion::recompute_at_end ()
{
	/* our length has changed. recompute a new final point by interpolating 
	   based on the the existing curve.
	*/
	
	_envelope.freeze ();
	_envelope.truncate_end (_length);
	_envelope.set_max_xval (_length);
	_envelope.thaw ();

	if (_fade_in.back()->when > _length) {
		_fade_in.extend_to (_length);
		send_change (FadeInChanged);
	}

	if (_fade_out.back()->when > _length) {
		_fade_out.extend_to (_length);
		send_change (FadeOutChanged);
	}
}	

void
AudioRegion::recompute_at_start ()
{
	/* as above, but the shift was from the front */

	_envelope.truncate_start (_length);

	if (_fade_in.back()->when > _length) {
		_fade_in.extend_to (_length);
		send_change (FadeInChanged);
	}

	if (_fade_out.back()->when > _length) {
		_fade_out.extend_to (_length);
		send_change (FadeOutChanged);
	}
}

int
AudioRegion::separate_by_channel (Session& session, vector<AudioRegion*>& v) const
{
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

		v.push_back (new AudioRegion (srcs, _start, _length, new_name, _layer, _flags));
	}

	return 0;
}

int
AudioRegion::apply (AudioFilter& filter)
{
	return filter.run (boost::shared_ptr<AudioRegion> (this));
}

int
AudioRegion::exportme (Session& session, AudioExportSpecification& spec)
{
	const nframes_t blocksize = 4096;
	nframes_t to_read;
	int status = -1;

	spec.channels = _sources.size();

	if (spec.prepare (blocksize, session.frame_rate())) {
		goto out;
	}

	spec.pos = 0;
	spec.total_frames = _length;

	while (spec.pos < _length && !spec.stop) {
		
		
		/* step 1: interleave */
		
		to_read = min (_length - spec.pos, blocksize);
		
		if (spec.channels == 1) {

			if (audio_source()->read (spec.dataF, _start + spec.pos, to_read) != to_read) {
				goto out;
			}

		} else {

			Sample buf[blocksize];

			for (uint32_t chan = 0; chan < spec.channels; ++chan) {
				
				if (audio_source(chan)->read (buf, _start + spec.pos, to_read) != to_read) {
					goto out;
				}
				
				for (nframes_t x = 0; x < to_read; ++x) {
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

void
AudioRegion::set_scale_amplitude (gain_t g)
{
	_scale_amplitude = g;

	/* tell the diskstream we're in */

	if (_playlist) {
		_playlist->Modified();
	}

	/* tell everybody else */

	send_change (ScaleAmplitudeChanged);
}

void
AudioRegion::normalize_to (float target_dB)
{
	const nframes_t blocksize = 64 * 1024;
	Sample buf[blocksize];
	nframes_t fpos;
	nframes_t fend;
	nframes_t to_read;
	double maxamp = 0;
	gain_t target = dB_to_coefficient (target_dB);

	if (target == 1.0f) {
		/* do not normalize to precisely 1.0 (0 dBFS), to avoid making it appear
		   that we may have clipped.
		*/
		target -= FLT_EPSILON;
	}

	fpos = _start;
	fend = _start + _length;

	/* first pass: find max amplitude */

	while (fpos < fend) {

		uint32_t n;

		to_read = min (fend - fpos, blocksize);

		for (n = 0; n < n_channels(); ++n) {

			/* read it in */

			if (audio_source (n)->read (buf, fpos, to_read) != to_read) {
				return;
			}
			
			maxamp = Session::compute_peak (buf, to_read, maxamp);
		}

		fpos += to_read;
	};

	if (maxamp == 0.0f) {
		/* don't even try */
		return;
	}

	if (maxamp == target) {
		/* we can't do anything useful */
		return;
	}

	/* compute scale factor */

	_scale_amplitude = target/maxamp;

	if (!_frozen) {
		char buf[64];
		snprintf (buf, sizeof (buf), _("normalized to %.2fdB"), target_dB);
		save_state (buf);
	}

	/* tell the diskstream we're in */

	if (_playlist) {
		_playlist->Modified();
	}

	/* tell everybody else */

	send_change (ScaleAmplitudeChanged);
}

void
AudioRegion::envelope_changed (Change ignored)
{
	save_state (_("envelope change"));
	send_change (EnvelopeChanged);
}

void
AudioRegion::suspend_fade_in ()
{
	if (++_fade_in_disabled == 1) {
		set_fade_in_active (false);
	}
}

void
AudioRegion::resume_fade_in ()
{
	if (_fade_in_disabled && --_fade_in_disabled == 0) {
		set_fade_in_active (true);
	}
}

void
AudioRegion::suspend_fade_out ()
{
	if (++_fade_out_disabled == 1) {
		set_fade_out_active (false);
	}
}

void
AudioRegion::resume_fade_out ()
{
	if (_fade_out_disabled && --_fade_out_disabled == 0) {
		set_fade_out_active (true);
	}
}

bool
AudioRegion::speed_mismatch (float sr) const
{
	if (_sources.empty()) {
		/* impossible, but ... */
		return false;
	}

	float fsr = audio_source()->sample_rate();

	return fsr != sr;
}

void
AudioRegion::source_offset_changed ()
{
	if (boost::dynamic_pointer_cast<DestructiveFileSource>(_sources.front())) {
		set_start (source()->natural_position(), this);
		set_position (source()->natural_position(), this);
	} 
}

boost::shared_ptr<AudioSource>
AudioRegion::audio_source (uint32_t n) const
{
	// Guaranteed to succeed (use a static cast?)
	return boost::dynamic_pointer_cast<AudioSource>(source(n));
}

extern "C" {

	int region_read_peaks_from_c (void *arg, uint32_t npeaks, uint32_t start, uint32_t cnt, intptr_t data, uint32_t n_chan, double samples_per_unit) 
{
	return ((AudioRegion *) arg)->read_peaks ((PeakData *) data, (nframes_t) npeaks, (nframes_t) start, (nframes_t) cnt, n_chan,samples_per_unit);
}

uint32_t region_length_from_c (void *arg)
{

	return ((AudioRegion *) arg)->length();
}

uint32_t sourcefile_length_from_c (void *arg, double zoom_factor)
{
	return ( (AudioRegion *) arg)->audio_source()->available_peaks (zoom_factor) ;
}

} /* extern "C" */
