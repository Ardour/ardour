/*
    Copyright (C) 2000-2001 Paul Davis 

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
#include <algorithm>

#include <set>

#include <sigc++/bind.h>
#include <sigc++/class_slot.h>

#include <glibmm/thread.h>

#include <pbd/basename.h>
#include <pbd/xml++.h>
#include <pbd/stacktrace.h>
#include <pbd/enumwriter.h>
#include <pbd/convert.h>

#include <ardour/audioregion.h>
#include <ardour/session.h>
#include <ardour/gain.h>
#include <ardour/dB.h>
#include <ardour/playlist.h>
#include <ardour/audiofilter.h>
#include <ardour/audiofilesource.h>
#include <ardour/region_factory.h>
#include <ardour/transient_detector.h>

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/* a Session will reset these to its chosen defaults by calling AudioRegion::set_default_fade() */

Change AudioRegion::FadeInChanged         = ARDOUR::new_change();
Change AudioRegion::FadeOutChanged        = ARDOUR::new_change();
Change AudioRegion::FadeInActiveChanged   = ARDOUR::new_change();
Change AudioRegion::FadeOutActiveChanged  = ARDOUR::new_change();
Change AudioRegion::EnvelopeActiveChanged = ARDOUR::new_change();
Change AudioRegion::ScaleAmplitudeChanged = ARDOUR::new_change();
Change AudioRegion::EnvelopeChanged       = ARDOUR::new_change();

AudioRegion::AudioRegion (boost::shared_ptr<AudioSource> src, nframes_t start, nframes_t length)
	: Region (start, length, PBD::basename_nosuffix(src->name()), 0,  Region::Flag(Region::DefaultFlags|Region::External)),
	  _fade_in (0.0, 2.0, 1.0, false),
	  _fade_out (0.0, 2.0, 1.0, false),
	  _envelope (0.0, 2.0, 1.0, false)
{
	/* basic AudioRegion constructor */

	sources.push_back (src);
	master_sources.push_back (src);
	src->GoingAway.connect (mem_fun (*this, &AudioRegion::source_deleted));

	boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> (src);
	if (afs) {
		afs->HeaderPositionOffsetChanged.connect (mem_fun (*this, &AudioRegion::source_offset_changed));
	}

	_scale_amplitude = 1.0;

	set_default_fades ();
	set_default_envelope ();

	listen_to_my_curves ();
	listen_to_my_sources ();
}

AudioRegion::AudioRegion (boost::shared_ptr<AudioSource> src, nframes_t start, nframes_t length, const string& name, layer_t layer, Flag flags)
	: Region (start, length, name, layer, flags),
	  _fade_in (0.0, 2.0, 1.0, false),
	  _fade_out (0.0, 2.0, 1.0, false),
	  _envelope (0.0, 2.0, 1.0, false)
{
	/* basic AudioRegion constructor */

	sources.push_back (src);
	master_sources.push_back (src);	
	src->GoingAway.connect (mem_fun (*this, &AudioRegion::source_deleted));

	boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> (src);
	if (afs) {
		afs->HeaderPositionOffsetChanged.connect (mem_fun (*this, &AudioRegion::source_offset_changed));
	}

	_scale_amplitude = 1.0;

	set_default_fades ();
	set_default_envelope ();

	listen_to_my_curves ();
	listen_to_my_sources ();
}

AudioRegion::AudioRegion (const SourceList& srcs, nframes_t start, nframes_t length, const string& name, layer_t layer, Flag flags)
	: Region (start, length, name, layer, flags),
	  _fade_in (0.0, 2.0, 1.0, false),
	  _fade_out (0.0, 2.0, 1.0, false),
	  _envelope (0.0, 2.0, 1.0, false)
{
	/* basic AudioRegion constructor */
	
	for (SourceList::const_iterator i=srcs.begin(); i != srcs.end(); ++i) {
		sources.push_back (*i);
		master_sources.push_back (*i);
		(*i)->GoingAway.connect (mem_fun (*this, &AudioRegion::source_deleted));
		
		boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> ((*i));
		if (afs) {
			afs->HeaderPositionOffsetChanged.connect (mem_fun (*this, &AudioRegion::source_offset_changed));
		}
	}

	_scale_amplitude = 1.0;

	set_default_fades ();
	set_default_envelope ();

	listen_to_my_curves ();
	listen_to_my_sources ();
}


AudioRegion::AudioRegion (boost::shared_ptr<const AudioRegion> other, nframes_t offset, nframes_t length, const string& name, layer_t layer, Flag flags)
	: Region (other, offset, length, name, layer, flags),
	  _fade_in (other->_fade_in),
	  _fade_out (other->_fade_out),
	  _envelope (other->_envelope, (double) offset, (double) offset + length) 
{
	/* create a new AudioRegion, that is part of an existing one */
	
	set<boost::shared_ptr<AudioSource> > unique_srcs;

	for (SourceList::const_iterator i= other->sources.begin(); i != other->sources.end(); ++i) {
		sources.push_back (*i);
		(*i)->GoingAway.connect (mem_fun (*this, &AudioRegion::source_deleted));

		pair<set<boost::shared_ptr<AudioSource> >::iterator,bool> result;

		result = unique_srcs.insert (*i);

		if (result.second) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> (*i);
			if (afs) {
				afs->HeaderPositionOffsetChanged.connect (mem_fun (*this, &AudioRegion::source_offset_changed));
			}
		}
	}

	for (SourceList::const_iterator i = other->master_sources.begin(); i != other->master_sources.end(); ++i) {
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->GoingAway.connect (mem_fun (*this, &AudioRegion::source_deleted));
		}
		master_sources.push_back (*i);

	}

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

	listen_to_my_curves ();
	listen_to_my_sources ();
}

AudioRegion::AudioRegion (boost::shared_ptr<const AudioRegion> other)
	: Region (other),
	  _fade_in (other->_fade_in),
	  _fade_out (other->_fade_out),
	  _envelope (other->_envelope)
	  
{
	/* Pure copy constructor */

	set<boost::shared_ptr<AudioSource> > unique_srcs;

	for (SourceList::const_iterator i = other->sources.begin(); i != other->sources.end(); ++i) {
		sources.push_back (*i);
		(*i)->GoingAway.connect (mem_fun (*this, &AudioRegion::source_deleted));
		pair<set<boost::shared_ptr<AudioSource> >::iterator,bool> result;

		result = unique_srcs.insert (*i);

		if (result.second) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> (*i);
			if (afs) {
				afs->HeaderPositionOffsetChanged.connect (mem_fun (*this, &AudioRegion::source_offset_changed));
			}
		}
	}

	for (SourceList::const_iterator i = other->master_sources.begin(); i != other->master_sources.end(); ++i) {
		master_sources.push_back (*i);
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->GoingAway.connect (mem_fun (*this, &AudioRegion::source_deleted));
		}
	}

	_scale_amplitude = other->_scale_amplitude;

	_fade_in_disabled = 0;
	_fade_out_disabled = 0;

	listen_to_my_curves ();
	listen_to_my_sources ();
}

AudioRegion::AudioRegion (boost::shared_ptr<const AudioRegion> other, const SourceList& srcs,
			  nframes_t length, const string& name, layer_t layer, Flag flags)
	: Region (other, length, name, layer, flags),
	  _fade_in (other->_fade_in),
	  _fade_out (other->_fade_out),
	  _envelope (other->_envelope)
	  
{
	/* make-a-sort-of-copy-with-different-sources constructor (used by audio filter) */

	set<boost::shared_ptr<AudioSource> > unique_srcs;

	for (SourceList::const_iterator i=srcs.begin(); i != srcs.end(); ++i) {
		sources.push_back (*i);
		master_sources.push_back (*i);
		(*i)->GoingAway.connect (mem_fun (*this, &AudioRegion::source_deleted));
		
		boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> ((*i));
		if (afs) {
			afs->HeaderPositionOffsetChanged.connect (mem_fun (*this, &AudioRegion::source_offset_changed));
		}
	}

	_scale_amplitude = other->_scale_amplitude;

	_fade_in_disabled = 0;
	_fade_out_disabled = 0;

	listen_to_my_curves ();
	listen_to_my_sources ();
}

AudioRegion::AudioRegion (boost::shared_ptr<AudioSource> src, const XMLNode& node)
	: Region (node),
	  _fade_in (0.0, 2.0, 1.0, false),
	  _fade_out (0.0, 2.0, 1.0, false),
	  _envelope (0.0, 2.0, 1.0, false)
{
	sources.push_back (src);
	master_sources.push_back (src);
	src->GoingAway.connect (mem_fun (*this, &AudioRegion::source_deleted));

	boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> (src);
	if (afs) {
		afs->HeaderPositionOffsetChanged.connect (mem_fun (*this, &AudioRegion::source_offset_changed));
	}

	set_default_fades ();

	if (set_state (node)) {
		throw failed_constructor();
	}

	listen_to_my_curves ();
	listen_to_my_sources ();
}

AudioRegion::AudioRegion (SourceList& srcs, const XMLNode& node)
	: Region (node),
	  _fade_in (0.0, 2.0, 1.0, false),
	  _fade_out (0.0, 2.0, 1.0, false),
	  _envelope (0.0, 2.0, 1.0, false)
{
	set<boost::shared_ptr<AudioSource> > unique_srcs;

	for (SourceList::iterator i=srcs.begin(); i != srcs.end(); ++i) {
		sources.push_back (*i);
		(*i)->GoingAway.connect (mem_fun (*this, &AudioRegion::source_deleted));
		pair<set<boost::shared_ptr<AudioSource> >::iterator,bool> result;

		result = unique_srcs.insert (*i);

		if (result.second) {
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> (*i);
			if (afs) {
				afs->HeaderPositionOffsetChanged.connect (mem_fun (*this, &AudioRegion::source_offset_changed));
			}
		}
	}

	for (SourceList::iterator i = srcs.begin(); i != srcs.end(); ++i) {
		master_sources.push_back (*i);
		if (unique_srcs.find (*i) == unique_srcs.end()) {
			(*i)->GoingAway.connect (mem_fun (*this, &AudioRegion::source_deleted));
		}
	}

	set_default_fades ();
	_scale_amplitude = 1.0;

	if (set_state (node)) {
		throw failed_constructor();
	}

	listen_to_my_curves ();
	listen_to_my_sources ();
}

AudioRegion::~AudioRegion ()
{
	boost::shared_ptr<Playlist> pl (playlist());

	if (pl) {
		for (SourceList::const_iterator i = sources.begin(); i != sources.end(); ++i) {
			(*i)->remove_playlist (pl);
		}
		for (SourceList::const_iterator i = master_sources.begin(); i != master_sources.end(); ++i) {
			(*i)->remove_playlist (pl);
		}
	}

	notify_callbacks ();
	GoingAway (); /* EMIT SIGNAL */
}

void
AudioRegion::listen_to_my_sources ()
{
	for (SourceList::const_iterator i = sources.begin(); i != sources.end(); ++i) {
		(*i)->AnalysisChanged.connect (mem_fun (*this, &AudioRegion::invalidate_transients));
	}
}

void
AudioRegion::listen_to_my_curves ()
{
	_envelope.StateChanged.connect (mem_fun (*this, &AudioRegion::envelope_changed));
	_fade_in.StateChanged.connect (mem_fun (*this, &AudioRegion::fade_in_changed));
	_fade_out.StateChanged.connect (mem_fun (*this, &AudioRegion::fade_out_changed));
}

bool
AudioRegion::verify_length (nframes_t& len)
{
	boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(source());

	if (afs && afs->destructive()) {
		return true;
	}

	nframes_t maxlen = 0;

	for (uint32_t n=0; n < sources.size(); ++n) {
		maxlen = max (maxlen, sources[n]->length() - _start);
	}
	
	len = min (len, maxlen);
	
	return true;
}

bool
AudioRegion::verify_start_and_length (nframes_t new_start, nframes_t& new_length)
{
	boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(source());

	if (afs && afs->destructive()) {
		return true;
	}

	nframes_t maxlen = 0;

	for (uint32_t n=0; n < sources.size(); ++n) {
		maxlen = max (maxlen, sources[n]->length() - new_start);
	}

	new_length = min (new_length, maxlen);

	return true;
}

bool
AudioRegion::verify_start (nframes_t pos)
{
	boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(source());

	if (afs && afs->destructive()) {
		return true;
	}

	for (uint32_t n=0; n < sources.size(); ++n) {
		if (pos > sources[n]->length() - _length) {
			return false;
		}
	}
	return true;
}

bool
AudioRegion::verify_start_mutable (nframes_t& new_start)
{
	boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(source());

	if (afs && afs->destructive()) {
		return true;
	}

	for (uint32_t n=0; n < sources.size(); ++n) {
		if (new_start > sources[n]->length() - _length) {
			new_start = sources[n]->length() - _length;
		}
	}
	return true;
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
		send_change (EnvelopeActiveChanged);
	}
}

nframes_t
AudioRegion::read_peaks (PeakData *buf, nframes_t npeaks, nframes_t offset, nframes_t cnt, uint32_t chan_n, double samples_per_unit) const
{
	if (chan_n >= sources.size()) {
		return 0; 
	}

	if (sources[chan_n]->read_peaks (buf, npeaks, offset, cnt, samples_per_unit)) {
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

nframes64_t
AudioRegion::read (Sample* buf, nframes64_t timeline_position, nframes64_t cnt, int channel) const
{
	/* raw read, no fades, no gain, nada */
	return _read_at (sources, _length, buf, 0, 0, _position + timeline_position, cnt, channel, 0, 0, ReadOps (0));
}

nframes64_t
AudioRegion::read_with_ops (Sample* buf, nframes64_t file_position, nframes64_t cnt, int channel, ReadOps rops) const
{
	return _read_at (sources, _length, buf, 0, 0, file_position, cnt, channel, 0, 0, rops);
}

nframes_t
AudioRegion::read_at (Sample *buf, Sample *mixdown_buffer, float *gain_buffer, nframes_t file_position, 
		      nframes_t cnt, 
		      uint32_t chan_n, nframes_t read_frames, nframes_t skip_frames) const
{
	/* regular diskstream/butler read complete with fades etc */
	return _read_at (sources, _length, buf, mixdown_buffer, gain_buffer, file_position, cnt, 
			 chan_n, read_frames, skip_frames, ReadOps (~0));
}

nframes_t
AudioRegion::master_read_at (Sample *buf, Sample *mixdown_buffer, float *gain_buffer, nframes_t position, 
			     nframes_t cnt, uint32_t chan_n) const
{
	/* do not read gain/scaling/fades and do not count this disk i/o in statistics */

	return _read_at (master_sources, master_sources.front()->length(), buf, mixdown_buffer, 
			 gain_buffer, position, cnt, chan_n, 0, 0, ReadOps (0));
}

nframes_t
AudioRegion::_read_at (const SourceList& srcs, nframes_t limit,
		       Sample *buf, Sample *mixdown_buffer, float *gain_buffer,
		       nframes_t position, nframes_t cnt, 
		       uint32_t chan_n, 
		       nframes_t read_frames, 
		       nframes_t skip_frames,
		       ReadOps rops) const
{
	nframes_t internal_offset;
	nframes_t buf_offset;
	nframes_t to_read;
	bool raw = (rops == ReadOpsNone);

	if (muted() && !raw) {
		return 0; /* read nothing */
	}

	/* precondition: caller has verified that we cover the desired section */

	if (position < _position) {
		internal_offset = 0;
		buf_offset = _position - position;
		cnt -= buf_offset;
	} else {
		internal_offset = position - _position;
		buf_offset = 0;
	}

	if (internal_offset >= limit) {
		return 0; /* read nothing */
	}

	if ((to_read = min (cnt, limit - internal_offset)) == 0) {
		return 0; /* read nothing */
	}

	if (opaque() || raw) {
		/* overwrite whatever is there */
		mixdown_buffer = buf + buf_offset;
	} else {
		mixdown_buffer += buf_offset;
	}

	if (rops & ReadOpsCount) {
		_read_data_count = 0;
	}

	if (chan_n < n_channels()) {

		if (srcs[chan_n]->read (mixdown_buffer, _start + internal_offset, to_read) != to_read) {
			return 0; /* "read nothing" */
		}
		
		if (rops & ReadOpsCount) {
			_read_data_count += srcs[chan_n]->read_data_count();
		}

	} else {
		
		/* track is N-channel, this region has less channels; silence the ones
		   we don't have.
		*/

		memset (mixdown_buffer, 0, sizeof (Sample) * cnt);
	}

	if (rops & ReadOpsFades) {
	
		/* fade in */

		if ((_flags & FadeIn) && Config->get_use_region_fades()) {
			
			nframes_t fade_in_length = (nframes_t) _fade_in.back()->when;
			
			/* see if this read is within the fade in */
			
			if (internal_offset < fade_in_length) {
				
				nframes_t fi_limit;
				
				fi_limit = min (to_read, fade_in_length - internal_offset);
				
				_fade_in.get_vector (internal_offset, internal_offset+fi_limit, gain_buffer, fi_limit);
				
				for (nframes_t n = 0; n < fi_limit; ++n) {
					mixdown_buffer[n] *= gain_buffer[n];
				}
			}
		}
		
		/* fade out */
		
		if ((_flags & FadeOut) && Config->get_use_region_fades()) {

			/* see if some part of this read is within the fade out */
			
		/* .................        >|            REGION
		                             limit
 					    
                                 {           }            FADE
				             fade_out_length
                                 ^					     
                                 limit - fade_out_length
                        |--------------|
                        ^internal_offset
                                       ^internal_offset + to_read
				       
				       we need the intersection of [internal_offset,internal_offset+to_read] with
				       [limit - fade_out_length, limit]
				       
		*/

	
			nframes_t fade_out_length = (nframes_t) _fade_out.back()->when;
			nframes_t fade_interval_start = max(internal_offset, limit-fade_out_length);
			nframes_t fade_interval_end   = min(internal_offset + to_read, limit);

			if (fade_interval_end > fade_interval_start) {
				/* (part of the) the fade out is  in this buffer */

				nframes_t fo_limit = fade_interval_end - fade_interval_start;
				nframes_t curve_offset = fade_interval_start - (limit-fade_out_length);
				nframes_t fade_offset = fade_interval_start - internal_offset;
				
				_fade_out.get_vector (curve_offset,curve_offset+fo_limit, gain_buffer, fo_limit);
				
				for (nframes_t n = 0, m = fade_offset; n < fo_limit; ++n, ++m) {
					mixdown_buffer[m] *= gain_buffer[n];
				}
			} 
			
		}
	}
		
        /* Regular gain curves and scaling */
	
	if ((rops & ReadOpsOwnAutomation) && envelope_active())  {
		_envelope.get_vector (internal_offset, internal_offset + to_read, gain_buffer, to_read);
		
		if ((rops & ReadOpsOwnScaling) && _scale_amplitude != 1.0f) {
			for (nframes_t n = 0; n < to_read; ++n) {
				mixdown_buffer[n] *= gain_buffer[n] * _scale_amplitude;
			}
		} else {
			for (nframes_t n = 0; n < to_read; ++n) {
				mixdown_buffer[n] *= gain_buffer[n];
			}
		}
	} else if ((rops & ReadOpsOwnScaling) && _scale_amplitude != 1.0f) {
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
	
	node.add_property ("flags", enum_2_string (_flags));

	snprintf (buf, sizeof(buf), "%.12g", _scale_amplitude);
	node.add_property ("scale-gain", buf);

	for (uint32_t n=0; n < sources.size(); ++n) {
		snprintf (buf2, sizeof(buf2), "source-%d", n);
		sources[n]->id().print (buf, sizeof (buf));
		node.add_property (buf2, buf);
	}

	snprintf (buf, sizeof (buf), "%u", (uint32_t) sources.size());
	node.add_property ("channels", buf);

	if (full) {
	
		child = node.add_child (X_("FadeIn"));
		
		if ((_flags & DefaultFadeIn)) {
			child->add_property (X_("default"), X_("yes"));
		} else {
			child->add_child_nocopy (_fade_in.get_state ());
		}

		child->add_property (X_("active"), _fade_in_disabled ? X_("no") : X_("yes"));
		
		child = node.add_child (X_("FadeOut"));
		
		if ((_flags & DefaultFadeOut)) {
			child->add_property (X_("default"), X_("yes"));
		} else {
			child->add_child_nocopy (_fade_out.get_state ());
		}
		
		child->add_property (X_("active"), _fade_out_disabled ? X_("no") : X_("yes"));
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
			child->add_child_nocopy (_envelope.get_state ());
		}

	} else {
		child->add_property ("default", "yes");
	}

	for (uint32_t n=0; n < master_sources.size(); ++n) {
		snprintf (buf2, sizeof(buf2), "master-source-%d", n);
		master_sources[n]->id().print (buf, sizeof (buf));
		node.add_property (buf2, buf);
	}

	if (full && _extra_xml) {
		node.add_child_copy (*_extra_xml);
	}

	return node;
}

int
AudioRegion::set_live_state (const XMLNode& node, Change& what_changed, bool send)
{
	const XMLNodeList& nlist = node.children();
	const XMLProperty *prop;
	LocaleGuard lg (X_("POSIX"));

	Region::set_live_state (node, what_changed, false);

	uint32_t old_flags = _flags;

	if ((prop = node.property ("flags")) != 0) {
		_flags = Flag (string_2_enum (prop->value(), _flags));

		//_flags = Flag (strtol (prop->value().c_str(), (char **) 0, 16));

		_flags = Flag (_flags & ~Region::LeftOfSplit);
		_flags = Flag (_flags & ~Region::RightOfSplit);
	}

	if ((old_flags ^ _flags) & Muted) {
		what_changed = Change (what_changed|MuteChanged);
	}
	if ((old_flags ^ _flags) & Opaque) {
		what_changed = Change (what_changed|OpacityChanged);
	}
	if ((old_flags ^ _flags) & Locked) {
		what_changed = Change (what_changed|LockChanged);
	}

	if ((prop = node.property ("scale-gain")) != 0) {
		_scale_amplitude = atof (prop->value().c_str());
		what_changed = Change (what_changed|ScaleAmplitudeChanged);
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

			if ((prop = child->property ("default")) != 0 || _envelope.set_state (*child)) {
				set_default_envelope ();
			}

			_envelope.set_max_xval (_length);
			_envelope.truncate_end (_length);

		} else if (child->name() == "FadeIn") {
			
			_fade_in.clear ();
			
			if ((prop = child->property ("default")) != 0 || (prop = child->property ("steepness")) != 0) {
				set_default_fade_in ();
			} else {
				XMLNode* grandchild = child->child ("AutomationList");
				if (grandchild) {
					_fade_in.set_state (*grandchild);
				}
			}

			if ((prop = child->property ("active")) != 0) {
				if (string_is_affirmative (prop->value())) {
					set_fade_in_active (true);
				} else {
					set_fade_in_active (true);
				}
			}

		} else if (child->name() == "FadeOut") {
			
			_fade_out.clear ();

			if ((prop = child->property ("default")) != 0 || (prop = child->property ("steepness")) != 0) {
				set_default_fade_out ();
			} else {
				XMLNode* grandchild = child->child ("AutomationList");
				if (grandchild) {
					_fade_out.set_state (*grandchild);
				} 
			}

			if ((prop = child->property ("active")) != 0) {
				if (string_is_affirmative (prop->value())) {
					set_fade_out_active (true);
				} else {
					set_fade_out_active (false);
				}
			}

		} 
	}

	if (send) {
		send_change (what_changed);
	}

	return 0;
}

int
AudioRegion::set_state (const XMLNode& node)
{
	/* Region::set_state() calls the virtual set_live_state(),
	   which will get us back to AudioRegion::set_live_state()
	   to handle the relevant stuff.
	*/

	return Region::set_state (node);
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
		_fade_in.fast_simple_add (0.0, 0.0);
		_fade_in.fast_simple_add (len, 1.0);
		break;

	case Fast:
		_fade_in.fast_simple_add (0, 0);
		_fade_in.fast_simple_add (len * 0.389401, 0.0333333);
		_fade_in.fast_simple_add (len * 0.629032, 0.0861111);
		_fade_in.fast_simple_add (len * 0.829493, 0.233333);
		_fade_in.fast_simple_add (len * 0.9447, 0.483333);
		_fade_in.fast_simple_add (len * 0.976959, 0.697222);
		_fade_in.fast_simple_add (len, 1);
		break;

	case Slow:
		_fade_in.fast_simple_add (0, 0);
		_fade_in.fast_simple_add (len * 0.0207373, 0.197222);
		_fade_in.fast_simple_add (len * 0.0645161, 0.525);
		_fade_in.fast_simple_add (len * 0.152074, 0.802778);
		_fade_in.fast_simple_add (len * 0.276498, 0.919444);
		_fade_in.fast_simple_add (len * 0.481567, 0.980556);
		_fade_in.fast_simple_add (len * 0.767281, 1);
		_fade_in.fast_simple_add (len, 1);
		break;

	case LogA:
		_fade_in.fast_simple_add (0, 0);
		_fade_in.fast_simple_add (len * 0.0737327, 0.308333);
		_fade_in.fast_simple_add (len * 0.246544, 0.658333);
		_fade_in.fast_simple_add (len * 0.470046, 0.886111);
		_fade_in.fast_simple_add (len * 0.652074, 0.972222);
		_fade_in.fast_simple_add (len * 0.771889, 0.988889);
		_fade_in.fast_simple_add (len, 1);
		break;

	case LogB:
		_fade_in.fast_simple_add (0, 0);
		_fade_in.fast_simple_add (len * 0.304147, 0.0694444);
		_fade_in.fast_simple_add (len * 0.529954, 0.152778);
		_fade_in.fast_simple_add (len * 0.725806, 0.333333);
		_fade_in.fast_simple_add (len * 0.847926, 0.558333);
		_fade_in.fast_simple_add (len * 0.919355, 0.730556);
		_fade_in.fast_simple_add (len, 1);
		break;
	}

	_fade_in.thaw ();
	_fade_in_shape = shape;

	send_change (FadeInChanged);
}

void
AudioRegion::set_fade_out (FadeShape shape, nframes_t len)
{
	_fade_out.freeze ();
	_fade_out.clear ();

	switch (shape) {
	case Fast:
		_fade_out.fast_simple_add (len * 0, 1);
		_fade_out.fast_simple_add (len * 0.023041, 0.697222);
		_fade_out.fast_simple_add (len * 0.0553,   0.483333);
		_fade_out.fast_simple_add (len * 0.170507, 0.233333);
		_fade_out.fast_simple_add (len * 0.370968, 0.0861111);
		_fade_out.fast_simple_add (len * 0.610599, 0.0333333);
		_fade_out.fast_simple_add (len * 1, 0);
		break;

	case LogA:
		_fade_out.fast_simple_add (len * 0, 1);
		_fade_out.fast_simple_add (len * 0.228111, 0.988889);
		_fade_out.fast_simple_add (len * 0.347926, 0.972222);
		_fade_out.fast_simple_add (len * 0.529954, 0.886111);
		_fade_out.fast_simple_add (len * 0.753456, 0.658333);
		_fade_out.fast_simple_add (len * 0.9262673, 0.308333);
		_fade_out.fast_simple_add (len * 1, 0);
		break;

	case Slow:
		_fade_out.fast_simple_add (len * 0, 1);
		_fade_out.fast_simple_add (len * 0.305556, 1);
		_fade_out.fast_simple_add (len * 0.548611, 0.991736);
		_fade_out.fast_simple_add (len * 0.759259, 0.931129);
		_fade_out.fast_simple_add (len * 0.918981, 0.68595);
		_fade_out.fast_simple_add (len * 0.976852, 0.22865);
		_fade_out.fast_simple_add (len * 1, 0);
		break;

	case LogB:
		_fade_out.fast_simple_add (len * 0, 1);
		_fade_out.fast_simple_add (len * 0.080645, 0.730556);
		_fade_out.fast_simple_add (len * 0.277778, 0.289256);
		_fade_out.fast_simple_add (len * 0.470046, 0.152778);
		_fade_out.fast_simple_add (len * 0.695853, 0.0694444);
		_fade_out.fast_simple_add (len * 1, 0);
		break;

	case Linear:
		_fade_out.fast_simple_add (len * 0, 1);
		_fade_out.fast_simple_add (len * 1, 0);
		break;
	}

	_fade_out.thaw ();
	_fade_out_shape = shape;

	send_change (FadeOutChanged);
}

void
AudioRegion::set_fade_in_length (nframes_t len)
{
	if (len > _length) {
		len = _length - 1;
	}

	bool changed = _fade_in.extend_to (len);

	if (changed) {
		_flags = Flag (_flags & ~DefaultFadeIn);
		send_change (FadeInChanged);
	}
}

void
AudioRegion::set_fade_out_length (nframes_t len)
{
	if (len > _length) {
		len = _length - 1;
	}

	bool changed =	_fade_out.extend_to (len);

	if (changed) {
		_flags = Flag (_flags & ~DefaultFadeOut);
		send_change (FadeOutChanged);
	}
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

bool
AudioRegion::fade_in_is_default () const
{
	return _fade_in_shape == Linear && _fade_in.back()->when == 64;
}

bool
AudioRegion::fade_out_is_default () const
{
	return _fade_out_shape == Linear && _fade_out.back()->when == 64;
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
	_envelope.fast_simple_add (0, 1.0f);
	_envelope.fast_simple_add (_length, 1.0f);
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
AudioRegion::separate_by_channel (Session& session, vector<boost::shared_ptr<AudioRegion> >& v) const
{
	SourceList srcs;
	string new_name;
	int n;

	if (sources.size() < 2) {
		return 0;
	}

	n = 0;

	for (SourceList::const_iterator i = sources.begin(); i != sources.end(); ++i) {

		srcs.clear ();
		srcs.push_back (*i);

		new_name = _name;

		if (sources.size() == 2) {
			if (n == 0) {
				new_name += "-L";
			} else {
				new_name += "-R";
			}
		} else {
			new_name += '-';
			new_name += ('0' + n + 1);
		}

		/* create a copy with just one source. prevent if from being thought of as "whole file" even if 
		   it covers the entire source file(s).
		 */

		Flag f = Flag (_flags & ~WholeFile);

		boost::shared_ptr<Region> r = RegionFactory::create (srcs, _start, _length, new_name, _layer, f);
		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);

		v.push_back (ar);
		
		++n;
	}

	return 0;
}

void
AudioRegion::source_deleted ()
{
	sources.clear ();
	drop_references ();
}

vector<string>
AudioRegion::master_source_names ()
{
	SourceList::iterator i;

	vector<string> names;
	for (i = master_sources.begin(); i != master_sources.end(); ++i) {
		names.push_back((*i)->name());
	}

	return names;
}

void
AudioRegion::set_master_sources (const SourceList& srcs)
{
	master_sources = srcs;
}

bool
AudioRegion::source_equivalent (boost::shared_ptr<const Region> o) const
{
	boost::shared_ptr<const AudioRegion> other = boost::dynamic_pointer_cast<const AudioRegion>(o);

	if (!other)
		return false;

	SourceList::const_iterator i;
	SourceList::const_iterator io;

	for (i = sources.begin(), io = other->sources.begin(); i != sources.end() && io != other->sources.end(); ++i, ++io) {
		if ((*i)->id() != (*io)->id()) {
			return false;
		}
	}

	for (i = master_sources.begin(), io = other->master_sources.begin(); i != master_sources.end() && io != other->master_sources.end(); ++i, ++io) {
		if ((*i)->id() != (*io)->id()) {
			return false;
		}
	}

	return true;
}

int
AudioRegion::apply (AudioFilter& filter)
{
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (shared_from_this());
	return filter.run (ar);
}

int
AudioRegion::exportme (Session& session, AudioExportSpecification& spec)
{
	const nframes_t blocksize = 4096;
	nframes_t to_read;
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

			if (sources.front()->read (spec.dataF, _start + spec.pos, to_read) != to_read) {
				goto out;
			}

		} else {

			Sample buf[blocksize];

			for (uint32_t chan = 0; chan < spec.channels; ++chan) {
				
				if (sources[chan]->read (buf, _start + spec.pos, to_read) != to_read) {
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

boost::shared_ptr<Region>
AudioRegion::get_parent() const
{
	boost::shared_ptr<Playlist> pl (playlist());

	if (pl) {
		boost::shared_ptr<AudioRegion> ar;
		boost::shared_ptr<AudioRegion const> grrr2 = boost::dynamic_pointer_cast<AudioRegion const> (shared_from_this());
		
		if (grrr2 && (ar = pl->session().find_whole_file_parent (grrr2))) {
			return boost::static_pointer_cast<Region> (ar);
		}
	}
	
	return boost::shared_ptr<Region>();
}

void
AudioRegion::set_scale_amplitude (gain_t g)
{
	boost::shared_ptr<Playlist> pl (playlist());

	_scale_amplitude = g;

	/* tell the diskstream we're in */
	
	if (pl) {
		pl->Modified();
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

			if (source (n)->read (buf, fpos, to_read) != to_read) {
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

	/* tell the diskstream we're in */

	boost::shared_ptr<Playlist> pl (playlist());

	if (pl) {
		pl->Modified();
	}

	/* tell everybody else */

	send_change (ScaleAmplitudeChanged);
}

void
AudioRegion::fade_in_changed ()
{
	send_change (FadeInChanged);
}

void
AudioRegion::fade_out_changed ()
{
	send_change (FadeOutChanged);
}

void
AudioRegion::envelope_changed ()
{
	send_change (EnvelopeChanged);
}

void
AudioRegion::suspend_fade_in ()
{
	if (++_fade_in_disabled == 1) {
		if (fade_in_is_default()) {
			set_fade_in_active (false);
		}
	}
}

void
AudioRegion::resume_fade_in ()
{
	if (--_fade_in_disabled == 0 && _fade_in_disabled) {
		set_fade_in_active (true);
	}
}

void
AudioRegion::suspend_fade_out ()
{
	if (++_fade_out_disabled == 1) {
		if (fade_out_is_default()) {
			set_fade_out_active (false);
		}
	}
}

void
AudioRegion::resume_fade_out ()
{
	if (--_fade_out_disabled == 0 &&_fade_out_disabled) {
		set_fade_out_active (true);
	}
}

bool
AudioRegion::speed_mismatch (float sr) const
{
	if (sources.empty()) {
		/* impossible, but ... */
		return false;
	}

	float fsr = sources.front()->sample_rate();

	return fsr != sr;
}

void
AudioRegion::source_offset_changed ()
{
	/* XXX this fixes a crash that should not occur. It does occur
	   becauses regions are not being deleted when a session
	   is unloaded. That bug must be fixed.
	*/

	if (sources.empty()) {
		return;
	}

	boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(sources.front());

	if (afs && afs->destructive()) {
		// set_start (source()->natural_position(), this);
		set_position (source()->natural_position(), this);
	} 
}

void
AudioRegion::set_playlist (boost::weak_ptr<Playlist> wpl)
{
	boost::shared_ptr<Playlist> old_playlist = (_playlist.lock());

	boost::shared_ptr<Playlist> pl (wpl.lock());

	if (old_playlist == pl) {
		return;
	}

	Region::set_playlist (wpl);

	if (pl) {
		if (old_playlist) {
			for (SourceList::const_iterator i = sources.begin(); i != sources.end(); ++i) {
				(*i)->remove_playlist (_playlist);	
				(*i)->add_playlist (pl);
			}
			for (SourceList::const_iterator i = master_sources.begin(); i != master_sources.end(); ++i) {
				(*i)->remove_playlist (_playlist);	
				(*i)->add_playlist (pl);
			}
		} else {
			for (SourceList::const_iterator i = sources.begin(); i != sources.end(); ++i) {
				(*i)->add_playlist (pl);
			}
			for (SourceList::const_iterator i = master_sources.begin(); i != master_sources.end(); ++i) {
				(*i)->add_playlist (pl);
			}
		}
	} else {
		if (old_playlist) {
			for (SourceList::const_iterator i = sources.begin(); i != sources.end(); ++i) {
				(*i)->remove_playlist (old_playlist);
			}
			for (SourceList::const_iterator i = master_sources.begin(); i != master_sources.end(); ++i) {
				(*i)->remove_playlist (old_playlist);
			}
		}
	}
}

int
AudioRegion::get_transients (AnalysisFeatureList& results, bool force_new)
{
	boost::shared_ptr<Playlist> pl = playlist();

	if (!pl) {
		return -1;
	}

	if (valid_transients && !force_new) {
		results = _transients;
		return 0;
	}

	SourceList::iterator s;
	
	for (s = sources.begin() ; s != sources.end(); ++s) {
		if (!(*s)->has_been_analysed()) {
			cerr << "For " << name() << " source " << (*s)->name() << " has not been analyzed\n";
			break;
		}
	}
	
	if (s == sources.end()) {
		/* all sources are analyzed, merge data from each one */

		for (s = sources.begin() ; s != sources.end(); ++s) {

			/* find the set of transients within the bounds of this region */

			AnalysisFeatureList::iterator low = lower_bound ((*s)->transients.begin(),
									 (*s)->transients.end(),
									 _start);

			AnalysisFeatureList::iterator high = upper_bound ((*s)->transients.begin(),
									  (*s)->transients.end(),
									  _start + _length);
									 
			/* and add them */

			results.insert (results.end(), low, high);
		}

		TransientDetector::cleanup_transients (results, pl->session().frame_rate(), 3.0);
		
		/* translate all transients to current position */

		for (AnalysisFeatureList::iterator x = results.begin(); x != results.end(); ++x) {
			(*x) -= _start;
			(*x) += _position;
		}

		_transients = results;
		valid_transients = true;

		return 0;
	}

	/* no existing/complete transient info */

	static bool analyse_dialog_shown = false;
	if (!Config->get_auto_analyse_audio()) {
		if ( !analyse_dialog_shown ) {
			pl->session().Dialog (_("\
You have requested an operation that requires audio analysis.\n\n\
You currently have \"auto-analyse-audio\" disabled, which means\n\
that transient data must be generated every time it is required.\n\n\
If you are doing work that will require transient data on a\n\
regular basis, you should probably enable \"auto-analyse-audio\"\n\
then quit ardour and restart.\n\n\
This dialog will not display again.  But you may notice a slight delay\n\
in this and future transient-detection operations.\n\
"));
			analyse_dialog_shown = true;  //only show this dialog once 
			}
	}

	TransientDetector t (pl->session().frame_rate());
	bool existing_results = !results.empty();

	_transients.clear ();
	valid_transients = false;

	for (uint32_t i = 0; i < n_channels(); ++i) {

		AnalysisFeatureList these_results;

		t.reset ();

		if (t.run ("", this, i, these_results)) {
			return -1;
		}

		/* translate all transients to give absolute position */
		
		for (AnalysisFeatureList::iterator i = these_results.begin(); i != these_results.end(); ++i) {
			(*i) += _position;
		}

		/* merge */
		
		_transients.insert (_transients.end(), these_results.begin(), these_results.end());
	}
	
	if (!results.empty()) {
		if (existing_results) {
			
			/* merge our transients into the existing ones, then clean up
			   those.
			*/

			results.insert (results.end(), _transients.begin(), _transients.end());
			TransientDetector::cleanup_transients (results, pl->session().frame_rate(), 3.0);
		}

		/* make sure ours are clean too */

		TransientDetector::cleanup_transients (_transients, pl->session().frame_rate(), 3.0);

	} else {

		TransientDetector::cleanup_transients (_transients, pl->session().frame_rate(), 3.0);
		results = _transients;
	}

	valid_transients = true;

	return 0;
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
	return ( (AudioRegion *) arg)->source()->available_peaks (zoom_factor) ;
}

} /* extern "C" */
