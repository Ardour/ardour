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

*/

#include <cmath>
#include <climits>
#include <cfloat>
#include <algorithm>

#include <set>

#include <boost/scoped_array.hpp>

#include <glibmm/threads.h>

#include "pbd/basename.h"
#include "pbd/xml++.h"
#include "pbd/stacktrace.h"
#include "pbd/enumwriter.h"
#include "pbd/convert.h"

#include "evoral/Curve.hpp"

#include "ardour/audioregion.h"
#include "ardour/session.h"
#include "ardour/dB.h"
#include "ardour/debug.h"
#include "ardour/playlist.h"
#include "ardour/audiofilesource.h"
#include "ardour/region_factory.h"
#include "ardour/runtime_functions.h"
#include "ardour/transient_detector.h"
#include "ardour/progress.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace ARDOUR {
	namespace Properties {
		PBD::PropertyDescriptor<bool> envelope_active;
		PBD::PropertyDescriptor<bool> default_fade_in;
		PBD::PropertyDescriptor<bool> default_fade_out;
		PBD::PropertyDescriptor<bool> fade_in_active;
		PBD::PropertyDescriptor<bool> fade_out_active;
		PBD::PropertyDescriptor<float> scale_amplitude;
		PBD::PropertyDescriptor<boost::shared_ptr<AutomationList> > fade_in;
		PBD::PropertyDescriptor<boost::shared_ptr<AutomationList> > inverse_fade_in;
		PBD::PropertyDescriptor<boost::shared_ptr<AutomationList> > fade_out;
		PBD::PropertyDescriptor<boost::shared_ptr<AutomationList> > inverse_fade_out;
		PBD::PropertyDescriptor<boost::shared_ptr<AutomationList> > envelope;
	}
}

static const double VERY_SMALL_SIGNAL = 0.0000001;  //-140dB

/* Curve manipulations */

static void
reverse_curve (boost::shared_ptr<Evoral::ControlList> dst, boost::shared_ptr<const Evoral::ControlList> src)
{
	size_t len = src->back()->when;
	for (Evoral::ControlList::const_reverse_iterator it = src->rbegin(); it!=src->rend(); it++) {
		dst->fast_simple_add (len - (*it)->when, (*it)->value);
	}
}

static void
generate_inverse_power_curve (boost::shared_ptr<Evoral::ControlList> dst, boost::shared_ptr<const Evoral::ControlList> src)
{
	// calc inverse curve using sum of squares
	for (Evoral::ControlList::const_iterator it = src->begin(); it!=src->end(); ++it ) {
		float value = (*it)->value;
		value = 1 - powf(value,2);
		value = sqrtf(value);
		dst->fast_simple_add ( (*it)->when, value );
	}
}

static void
generate_db_fade (boost::shared_ptr<Evoral::ControlList> dst, double len, int num_steps, float dB_drop)
{
	dst->clear ();
	dst->fast_simple_add (0, 1);

	//generate a fade-out curve by successively applying a gain drop
	float fade_speed = dB_to_coefficient(dB_drop / (float) num_steps);
	for (int i = 1; i < (num_steps-1); i++) {
		float coeff = 1.0;
		for (int j = 0; j < i; j++) {
			coeff *= fade_speed;
		}
		dst->fast_simple_add (len*(double)i/(double)num_steps, coeff);
	}

	dst->fast_simple_add (len, VERY_SMALL_SIGNAL);
}

static void
merge_curves (boost::shared_ptr<Evoral::ControlList> dst, 
	      boost::shared_ptr<const Evoral::ControlList> curve1, 
	      boost::shared_ptr<const Evoral::ControlList> curve2)
{
	Evoral::ControlList::EventList::size_type size = curve1->size();

	//curve lengths must match for now
	if (size != curve2->size()) {
		return;
	}
	
	Evoral::ControlList::const_iterator c1 = curve1->begin();
	int count = 0;
	for (Evoral::ControlList::const_iterator c2 = curve2->begin(); c2!=curve2->end(); c2++ ) {
		float v1 = accurate_coefficient_to_dB((*c1)->value);
		float v2 = accurate_coefficient_to_dB((*c2)->value);
		
		double interp = v1 * ( 1.0-( (double)count / (double)size) );
		interp += v2 * ( (double)count / (double)size );

		interp = dB_to_coefficient(interp);
		dst->fast_simple_add ( (*c1)->when, interp );
		c1++;
		count++;
	}
}

void
AudioRegion::make_property_quarks ()
{
	Properties::envelope_active.property_id = g_quark_from_static_string (X_("envelope-active"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for envelope-active = %1\n", 	Properties::envelope_active.property_id));
	Properties::default_fade_in.property_id = g_quark_from_static_string (X_("default-fade-in"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for default-fade-in = %1\n", 	Properties::default_fade_in.property_id));
	Properties::default_fade_out.property_id = g_quark_from_static_string (X_("default-fade-out"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for default-fade-out = %1\n", 	Properties::default_fade_out.property_id));
	Properties::fade_in_active.property_id = g_quark_from_static_string (X_("fade-in-active"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for fade-in-active = %1\n", 	Properties::fade_in_active.property_id));
	Properties::fade_out_active.property_id = g_quark_from_static_string (X_("fade-out-active"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for fade-out-active = %1\n", 	Properties::fade_out_active.property_id));
	Properties::scale_amplitude.property_id = g_quark_from_static_string (X_("scale-amplitude"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for scale-amplitude = %1\n", 	Properties::scale_amplitude.property_id));
	Properties::fade_in.property_id = g_quark_from_static_string (X_("FadeIn"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for FadeIn = %1\n",		Properties::fade_in.property_id));
	Properties::inverse_fade_in.property_id = g_quark_from_static_string (X_("InverseFadeIn"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for InverseFadeIn = %1\n",	Properties::inverse_fade_in.property_id));
	Properties::fade_out.property_id = g_quark_from_static_string (X_("FadeOut"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for FadeOut = %1\n",		Properties::fade_out.property_id));
	Properties::inverse_fade_out.property_id = g_quark_from_static_string (X_("InverseFadeOut"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for InverseFadeOut = %1\n",	Properties::inverse_fade_out.property_id));
	Properties::envelope.property_id = g_quark_from_static_string (X_("Envelope"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for Envelope = %1\n",		Properties::envelope.property_id));
}

void
AudioRegion::register_properties ()
{
	/* no need to register parent class properties */

	add_property (_envelope_active);
	add_property (_default_fade_in);
	add_property (_default_fade_out);
	add_property (_fade_in_active);
	add_property (_fade_out_active);
	add_property (_scale_amplitude);
	add_property (_fade_in);
	add_property (_inverse_fade_in);
	add_property (_fade_out);
	add_property (_inverse_fade_out);
	add_property (_envelope);
}

#define AUDIOREGION_STATE_DEFAULT \
	_envelope_active (Properties::envelope_active, false) \
	, _default_fade_in (Properties::default_fade_in, true) \
	, _default_fade_out (Properties::default_fade_out, true) \
	, _fade_in_active (Properties::fade_in_active, true) \
	, _fade_out_active (Properties::fade_out_active, true) \
	, _scale_amplitude (Properties::scale_amplitude, 1.0) \
	, _fade_in (Properties::fade_in, boost::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (FadeInAutomation)))) \
	, _inverse_fade_in (Properties::inverse_fade_in, boost::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (FadeInAutomation)))) \
	, _fade_out (Properties::fade_out, boost::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (FadeOutAutomation)))) \
	, _inverse_fade_out (Properties::inverse_fade_out, boost::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (FadeOutAutomation))))

#define AUDIOREGION_COPY_STATE(other) \
	_envelope_active (Properties::envelope_active, other->_envelope_active) \
	, _default_fade_in (Properties::default_fade_in, other->_default_fade_in) \
	, _default_fade_out (Properties::default_fade_out, other->_default_fade_out) \
	, _fade_in_active (Properties::fade_in_active, other->_fade_in_active) \
	, _fade_out_active (Properties::fade_out_active, other->_fade_out_active) \
	, _scale_amplitude (Properties::scale_amplitude, other->_scale_amplitude) \
	, _fade_in (Properties::fade_in, boost::shared_ptr<AutomationList> (new AutomationList (*other->_fade_in.val()))) \
	, _inverse_fade_in (Properties::fade_in, boost::shared_ptr<AutomationList> (new AutomationList (*other->_inverse_fade_in.val()))) \
	, _fade_out (Properties::fade_in, boost::shared_ptr<AutomationList> (new AutomationList (*other->_fade_out.val()))) \
	, _inverse_fade_out (Properties::fade_in, boost::shared_ptr<AutomationList> (new AutomationList (*other->_inverse_fade_out.val())))
/* a Session will reset these to its chosen defaults by calling AudioRegion::set_default_fade() */

void
AudioRegion::init ()
{
	register_properties ();

	suspend_property_changes();
	set_default_fades ();
	set_default_envelope ();
	resume_property_changes();

	listen_to_my_curves ();
	connect_to_analysis_changed ();
	connect_to_header_position_offset_changed ();
}

/** Constructor for use by derived types only */
AudioRegion::AudioRegion (Session& s, framepos_t start, framecnt_t len, std::string name)
	: Region (s, start, len, name, DataType::AUDIO)
	, AUDIOREGION_STATE_DEFAULT
	, _envelope (Properties::envelope, boost::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter(EnvelopeAutomation))))
	, _automatable (s)
	, _fade_in_suspended (0)
	, _fade_out_suspended (0)
{
	init ();
	assert (_sources.size() == _master_sources.size());
}

/** Basic AudioRegion constructor */
AudioRegion::AudioRegion (const SourceList& srcs)
	: Region (srcs)
	, AUDIOREGION_STATE_DEFAULT
	, _envelope (Properties::envelope, boost::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter(EnvelopeAutomation))))
	, _automatable(srcs[0]->session())
	, _fade_in_suspended (0)
	, _fade_out_suspended (0)
{
	init ();
	assert (_sources.size() == _master_sources.size());
}

AudioRegion::AudioRegion (boost::shared_ptr<const AudioRegion> other)
	: Region (other)
	, AUDIOREGION_COPY_STATE (other)
	  /* As far as I can see, the _envelope's times are relative to region position, and have nothing
	     to do with sources (and hence _start).  So when we copy the envelope, we just use the supplied offset.
	  */
	, _envelope (Properties::envelope, boost::shared_ptr<AutomationList> (new AutomationList (*other->_envelope.val(), 0, other->_length)))
	, _automatable (other->session())
	, _fade_in_suspended (0)
	, _fade_out_suspended (0)
{
	/* don't use init here, because we got fade in/out from the other region
	*/
	register_properties ();
	listen_to_my_curves ();
	connect_to_analysis_changed ();
	connect_to_header_position_offset_changed ();

	assert(_type == DataType::AUDIO);
	assert (_sources.size() == _master_sources.size());
}

AudioRegion::AudioRegion (boost::shared_ptr<const AudioRegion> other, framecnt_t offset)
	: Region (other, offset)
	, AUDIOREGION_COPY_STATE (other)
	  /* As far as I can see, the _envelope's times are relative to region position, and have nothing
	     to do with sources (and hence _start).  So when we copy the envelope, we just use the supplied offset.
	  */
	, _envelope (Properties::envelope, boost::shared_ptr<AutomationList> (new AutomationList (*other->_envelope.val(), offset, other->_length)))
	, _automatable (other->session())
	, _fade_in_suspended (0)
	, _fade_out_suspended (0)
{
	/* don't use init here, because we got fade in/out from the other region
	*/
	register_properties ();
	listen_to_my_curves ();
	connect_to_analysis_changed ();
	connect_to_header_position_offset_changed ();

	assert(_type == DataType::AUDIO);
	assert (_sources.size() == _master_sources.size());
}

AudioRegion::AudioRegion (boost::shared_ptr<const AudioRegion> other, const SourceList& srcs)
	: Region (boost::static_pointer_cast<const Region>(other), srcs)
	, AUDIOREGION_COPY_STATE (other)
	, _envelope (Properties::envelope, boost::shared_ptr<AutomationList> (new AutomationList (*other->_envelope.val())))
	, _automatable (other->session())
	, _fade_in_suspended (0)
	, _fade_out_suspended (0)
{
	/* make-a-sort-of-copy-with-different-sources constructor (used by audio filter) */

	register_properties ();

	listen_to_my_curves ();
	connect_to_analysis_changed ();
	connect_to_header_position_offset_changed ();

	assert (_sources.size() == _master_sources.size());
}

AudioRegion::AudioRegion (SourceList& srcs)
	: Region (srcs)
	, AUDIOREGION_STATE_DEFAULT
	, _envelope (Properties::envelope, boost::shared_ptr<AutomationList> (new AutomationList(Evoral::Parameter(EnvelopeAutomation))))
	, _automatable(srcs[0]->session())
	, _fade_in_suspended (0)
	, _fade_out_suspended (0)
{
	init ();

	assert(_type == DataType::AUDIO);
	assert (_sources.size() == _master_sources.size());
}

AudioRegion::~AudioRegion ()
{
}

void
AudioRegion::post_set (const PropertyChange& /*ignored*/)
{
	if (!_sync_marked) {
		_sync_position = _start;
	}

	/* return to default fades if the existing ones are too long */

	if (_left_of_split) {
		if (_fade_in->back()->when >= _length) {
			set_default_fade_in ();
		}
		set_default_fade_out ();
		_left_of_split = false;
	}

	if (_right_of_split) {
		if (_fade_out->back()->when >= _length) {
			set_default_fade_out ();
		}

		set_default_fade_in ();
		_right_of_split = false;
	}

	/* If _length changed, adjust our gain envelope accordingly */
	_envelope->truncate_end (_length);
}

void
AudioRegion::connect_to_analysis_changed ()
{
	for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
		(*i)->AnalysisChanged.connect_same_thread (*this, boost::bind (&AudioRegion::invalidate_transients, this));
	}
}

void
AudioRegion::connect_to_header_position_offset_changed ()
{
	set<boost::shared_ptr<Source> > unique_srcs;

	for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {

		/* connect only once to HeaderPositionOffsetChanged, even if sources are replicated
		 */

		if (unique_srcs.find (*i) == unique_srcs.end ()) {
			unique_srcs.insert (*i);
			boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> (*i);
			if (afs) {
				afs->HeaderPositionOffsetChanged.connect_same_thread (*this, boost::bind (&AudioRegion::source_offset_changed, this));
			}
		}
	}
}

void
AudioRegion::listen_to_my_curves ()
{
	_envelope->StateChanged.connect_same_thread (*this, boost::bind (&AudioRegion::envelope_changed, this));
	_fade_in->StateChanged.connect_same_thread (*this, boost::bind (&AudioRegion::fade_in_changed, this));
	_fade_out->StateChanged.connect_same_thread (*this, boost::bind (&AudioRegion::fade_out_changed, this));
}

void
AudioRegion::set_envelope_active (bool yn)
{
	if (envelope_active() != yn) {
		_envelope_active = yn;
		send_change (PropertyChange (Properties::envelope_active));
	}
}

/** @param buf Buffer to put peak data in.
 *  @param npeaks Number of peaks to read (ie the number of PeakDatas in buf)
 *  @param offset Start position, as an offset from the start of this region's source.
 *  @param cnt Number of samples to read.
 *  @param chan_n Channel.
 *  @param frames_per_pixel Number of samples to use to generate one peak value.
 */
 
ARDOUR::framecnt_t
AudioRegion::read_peaks (PeakData *buf, framecnt_t npeaks, framecnt_t offset, framecnt_t cnt, uint32_t chan_n, double frames_per_pixel) const
{
	if (chan_n >= _sources.size()) {
		return 0;
	}

	if (audio_source(chan_n)->read_peaks (buf, npeaks, offset, cnt, frames_per_pixel)) {
		return 0;
	} else {
		if (_scale_amplitude != 1.0f) {
			for (framecnt_t n = 0; n < npeaks; ++n) {
				buf[n].max *= _scale_amplitude;
				buf[n].min *= _scale_amplitude;
			}
		}
		return cnt;
	}
}

/** @param buf Buffer to write data to (existing data will be overwritten).
 *  @param pos Position to read from as an offset from the region position.
 *  @param cnt Number of frames to read.
 *  @param channel Channel to read from.
 */
framecnt_t
AudioRegion::read (Sample* buf, framepos_t pos, framecnt_t cnt, int channel) const
{
	/* raw read, no fades, no gain, nada */
	return read_from_sources (_sources, _length, buf, _position + pos, cnt, channel);
}

framecnt_t
AudioRegion::master_read_at (Sample *buf, Sample* /*mixdown_buffer*/, float* /*gain_buffer*/,
			     framepos_t position, framecnt_t cnt, uint32_t chan_n) const
{
	/* do not read gain/scaling/fades and do not count this disk i/o in statistics */

	assert (cnt >= 0);
	return read_from_sources (
		_master_sources, _master_sources.front()->length (_master_sources.front()->timeline_position()),
		buf, position, cnt, chan_n
		);
}

/** @param buf Buffer to mix data into.
 *  @param mixdown_buffer Scratch buffer for audio data.
 *  @param gain_buffer Scratch buffer for gain data.
 *  @param position Position within the session to read from.
 *  @param cnt Number of frames to read.
 *  @param chan_n Channel number to read.
 */
framecnt_t
AudioRegion::read_at (Sample *buf, Sample *mixdown_buffer, float *gain_buffer,
		      framepos_t position,
		      framecnt_t cnt,
		      uint32_t chan_n) const
{
	/* We are reading data from this region into buf (possibly via mixdown_buffer).
	   The caller has verified that we cover the desired section.
	*/

	/* See doc/region_read.svg for a drawing which might help to explain
	   what is going on.
	*/

	assert (cnt >= 0);
	
	if (n_channels() == 0) {
		return 0;
	}

	/* WORK OUT WHERE TO GET DATA FROM */

	framecnt_t to_read;

	assert (position >= _position);
	frameoffset_t const internal_offset = position - _position;

	if (internal_offset >= _length) {
		return 0; /* read nothing */
	}

	if ((to_read = min (cnt, _length - internal_offset)) == 0) {
		return 0; /* read nothing */
	}


	/* COMPUTE DETAILS OF ANY FADES INVOLVED IN THIS READ */

	/* Amount (length) of fade in that we are dealing with in this read */
	framecnt_t fade_in_limit = 0;

	/* Offset from buf / mixdown_buffer of the start
	   of any fade out that we are dealing with
	*/
	frameoffset_t fade_out_offset = 0;
	
	/* Amount (length) of fade out that we are dealing with in this read */
	framecnt_t fade_out_limit = 0;

	framecnt_t fade_interval_start = 0;

	/* Fade in */
	
	if (_fade_in_active && _session.config.get_use_region_fades()) {
		
		framecnt_t fade_in_length = (framecnt_t) _fade_in->back()->when;

		/* see if this read is within the fade in */
		
		if (internal_offset < fade_in_length) {
			fade_in_limit = min (to_read, fade_in_length - internal_offset);
		}
	}
	
	/* Fade out */
	
	if (_fade_out_active && _session.config.get_use_region_fades()) {
		
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


		fade_interval_start = max (internal_offset, _length - framecnt_t (_fade_out->back()->when));
		framecnt_t fade_interval_end = min(internal_offset + to_read, _length.val());
		
		if (fade_interval_end > fade_interval_start) {
			/* (part of the) the fade out is in this buffer */
			fade_out_limit = fade_interval_end - fade_interval_start;
			fade_out_offset = fade_interval_start - internal_offset;
		}
	}

	/* READ DATA FROM THE SOURCE INTO mixdown_buffer.
	   We can never read directly into buf, since it may contain data
	   from a region `below' this one in the stack, and our fades (if they exist)
	   may need to mix with the existing data.
	*/

	if (read_from_sources (_sources, _length, mixdown_buffer, position, to_read, chan_n) != to_read) {
		return 0;
	}

	/* APPLY REGULAR GAIN CURVES AND SCALING TO mixdown_buffer */

	if (envelope_active())  {
		_envelope->curve().get_vector (internal_offset, internal_offset + to_read, gain_buffer, to_read);

		if (_scale_amplitude != 1.0f) {
			for (framecnt_t n = 0; n < to_read; ++n) {
				mixdown_buffer[n] *= gain_buffer[n] * _scale_amplitude;
			}
		} else {
			for (framecnt_t n = 0; n < to_read; ++n) {
				mixdown_buffer[n] *= gain_buffer[n];
			}
		}
	} else if (_scale_amplitude != 1.0f) {
		apply_gain_to_buffer (mixdown_buffer, to_read, _scale_amplitude);
	}

	/* APPLY FADES TO THE DATA IN mixdown_buffer AND MIX THE RESULTS INTO
	 * buf. The key things to realize here: (1) the fade being applied is
	 * (as of April 26th 2012) just the inverse of the fade in curve (2) 
	 * "buf" contains data from lower regions already. So this operation
	 * fades out the existing material.
	 */

	if (fade_in_limit != 0) {

		if (opaque()) {
			if (_inverse_fade_in) {

				/* explicit inverse fade in curve (e.g. for constant
				 * power), so we have to fetch it.
				 */
				
				_inverse_fade_in->curve().get_vector (internal_offset, internal_offset + fade_in_limit, gain_buffer, fade_in_limit);
				
				/* Fade the data from lower layers out */
				for (framecnt_t n = 0; n < fade_in_limit; ++n) {
					buf[n] *= gain_buffer[n];
				}
				
				/* refill gain buffer with the fade in */
				
				_fade_in->curve().get_vector (internal_offset, internal_offset + fade_in_limit, gain_buffer, fade_in_limit);
				
			} else {
				
				/* no explicit inverse fade in, so just use (1 - fade
				 * in) for the fade out of lower layers
				 */
				
				_fade_in->curve().get_vector (internal_offset, internal_offset + fade_in_limit, gain_buffer, fade_in_limit);
				
				for (framecnt_t n = 0; n < fade_in_limit; ++n) {
					buf[n] *= 1 - gain_buffer[n];
				}
			}
		} else {
			_fade_in->curve().get_vector (internal_offset, internal_offset + fade_in_limit, gain_buffer, fade_in_limit);
		}

		/* Mix our newly-read data in, with the fade */
		for (framecnt_t n = 0; n < fade_in_limit; ++n) {
			buf[n] += mixdown_buffer[n] * gain_buffer[n];
		}
	}

	if (fade_out_limit != 0) {

		framecnt_t const curve_offset = fade_interval_start - (_length - _fade_out->back()->when);

		if (opaque()) {
			if (_inverse_fade_out) {
				
				_inverse_fade_out->curve().get_vector (curve_offset, curve_offset + fade_out_limit, gain_buffer, fade_out_limit);
				
				/* Fade the data from lower levels in */
				for (framecnt_t n = 0, m = fade_out_offset; n < fade_out_limit; ++n, ++m) {
					buf[m] *= gain_buffer[n];
				}
				
				/* fetch the actual fade out */

				_fade_out->curve().get_vector (curve_offset, curve_offset + fade_out_limit, gain_buffer, fade_out_limit);
				
			} else {

				/* no explicit inverse fade out (which is
				 * actually a fade in), so just use (1 - fade
				 * out) for the fade in of lower layers
				 */
				
				_fade_out->curve().get_vector (curve_offset, curve_offset + fade_out_limit, gain_buffer, fade_out_limit);
				
				for (framecnt_t n = 0, m = fade_out_offset; n < fade_out_limit; ++n, ++m) {
					buf[m] *= 1 - gain_buffer[n];
				}
			}
		} else {
			_fade_out->curve().get_vector (curve_offset, curve_offset + fade_out_limit, gain_buffer, fade_out_limit);
		}

		/* Mix our newly-read data with whatever was already there,
		   with the fade out applied to our data.
		*/
		for (framecnt_t n = 0, m = fade_out_offset; n < fade_out_limit; ++n, ++m) {
			buf[m] += mixdown_buffer[m] * gain_buffer[n];
		}
	}
	
	/* MIX OR COPY THE REGION BODY FROM mixdown_buffer INTO buf */

	framecnt_t const N = to_read - fade_in_limit - fade_out_limit;
	if (N > 0) {
		if (opaque ()) {
			DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("Region %1 memcpy into buf @ %2 + %3, from mixdown buffer @ %4 + %5, len = %6 cnt was %7\n",
									   name(), buf, fade_in_limit, mixdown_buffer, fade_in_limit, N, cnt));
			memcpy (buf + fade_in_limit, mixdown_buffer + fade_in_limit, N * sizeof (Sample));
		} else {
			mix_buffers_no_gain (buf + fade_in_limit, mixdown_buffer + fade_in_limit, N);
		}
	}

	return to_read;
}

/** Read data directly from one of our sources, accounting for the situation when the track has a different channel
 *  count to the region.
 *
 *  @param srcs Source list to get our source from.
 *  @param limit Furthest that we should read, as an offset from the region position.
 *  @param buf Buffer to write data into (existing contents of the buffer will be overwritten)
 *  @param position Position to read from, in session frames.
 *  @param cnt Number of frames to read.
 *  @param chan_n Channel to read from.
 *  @return Number of frames read.
 */

framecnt_t
AudioRegion::read_from_sources (SourceList const & srcs, framecnt_t limit, Sample* buf, framepos_t position, framecnt_t cnt, uint32_t chan_n) const
{
	frameoffset_t const internal_offset = position - _position;
	if (internal_offset >= limit) {
		return 0;
	}

	framecnt_t const to_read = min (cnt, limit - internal_offset);
	if (to_read == 0) {
		return 0;
	}
	
	if (chan_n < n_channels()) {

		boost::shared_ptr<AudioSource> src = boost::dynamic_pointer_cast<AudioSource> (srcs[chan_n]);
		if (src->read (buf, _start + internal_offset, to_read) != to_read) {
			return 0; /* "read nothing" */
		}

	} else {

		/* track is N-channel, this region has fewer channels; silence the ones
		   we don't have.
		*/

		if (Config->get_replicate_missing_region_channels()) {

			/* copy an existing channel's data in for this non-existant one */

			uint32_t channel = chan_n % n_channels();
			boost::shared_ptr<AudioSource> src = boost::dynamic_pointer_cast<AudioSource> (srcs[channel]);

			if (src->read (buf, _start + internal_offset, to_read) != to_read) {
				return 0; /* "read nothing" */
			}

		} else {
			
			/* use silence */
			memset (buf, 0, sizeof (Sample) * to_read);
		}
	}

	return to_read;
}

XMLNode&
AudioRegion::get_basic_state ()
{
	XMLNode& node (Region::state ());
	char buf[64];
	LocaleGuard lg (X_("POSIX"));

	snprintf (buf, sizeof (buf), "%u", (uint32_t) _sources.size());
	node.add_property ("channels", buf);

	return node;
}

XMLNode&
AudioRegion::state ()
{
	XMLNode& node (get_basic_state());
	XMLNode *child;
	LocaleGuard lg (X_("POSIX"));

	child = node.add_child ("Envelope");

	bool default_env = false;

	// If there are only two points, the points are in the start of the region and the end of the region
	// so, if they are both at 1.0f, that means the default region.

	if (_envelope->size() == 2 &&
	    _envelope->front()->value == 1.0f &&
	    _envelope->back()->value==1.0f) {
		if (_envelope->front()->when == 0 && _envelope->back()->when == _length) {
			default_env = true;
		}
	}

	if (default_env) {
		child->add_property ("default", "yes");
	} else {
		child->add_child_nocopy (_envelope->get_state ());
	}

	child = node.add_child (X_("FadeIn"));

	if (_default_fade_in) {
		child->add_property ("default", "yes");
	} else {
		child->add_child_nocopy (_fade_in->get_state ());
	}

	if (_inverse_fade_in) {
		child = node.add_child (X_("InverseFadeIn"));
		child->add_child_nocopy (_inverse_fade_in->get_state ());
	}

	child = node.add_child (X_("FadeOut"));

	if (_default_fade_out) {
		child->add_property ("default", "yes");
	} else {
		child->add_child_nocopy (_fade_out->get_state ());
	}

	if (_inverse_fade_out) {
		child = node.add_child (X_("InverseFadeOut"));
		child->add_child_nocopy (_inverse_fade_out->get_state ());
	}

	return node;
}

int
AudioRegion::_set_state (const XMLNode& node, int version, PropertyChange& what_changed, bool send)
{
	const XMLNodeList& nlist = node.children();
	const XMLProperty *prop;
	LocaleGuard lg (X_("POSIX"));
	boost::shared_ptr<Playlist> the_playlist (_playlist.lock());

	suspend_property_changes ();

	if (the_playlist) {
		the_playlist->freeze ();
	}


	/* this will set all our State members and stuff controlled by the Region.
	   It should NOT send any changed signals - that is our responsibility.
	*/

	Region::_set_state (node, version, what_changed, false);

	if ((prop = node.property ("scale-gain")) != 0) {
		float a = atof (prop->value().c_str());
		if (a != _scale_amplitude) {
			_scale_amplitude = a;
			what_changed.add (Properties::scale_amplitude);
		}
	}

	/* Now find envelope description and other related child items */

	_envelope->freeze ();

	for (XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {
		XMLNode *child;
		XMLProperty *prop;

		child = (*niter);

		if (child->name() == "Envelope") {

			_envelope->clear ();

			if ((prop = child->property ("default")) != 0 || _envelope->set_state (*child, version)) {
				set_default_envelope ();
			}

			_envelope->truncate_end (_length);


		} else if (child->name() == "FadeIn") {

			_fade_in->clear ();

			if (((prop = child->property ("default")) != 0 && string_is_affirmative (prop->value())) || (prop = child->property ("steepness")) != 0) {
				set_default_fade_in ();
			} else {
				XMLNode* grandchild = child->child ("AutomationList");
				if (grandchild) {
					_fade_in->set_state (*grandchild, version);
				}
			}

			if ((prop = child->property ("active")) != 0) {
				if (string_is_affirmative (prop->value())) {
					set_fade_in_active (true);
				} else {
					set_fade_in_active (false);
				}
			}

		} else if (child->name() == "FadeOut") {

			_fade_out->clear ();

			if (((prop = child->property ("default")) != 0 && (string_is_affirmative (prop->value()))) || (prop = child->property ("steepness")) != 0) {
				set_default_fade_out ();
			} else {
				XMLNode* grandchild = child->child ("AutomationList");
				if (grandchild) {
					_fade_out->set_state (*grandchild, version);
				}
			}
			
			if ((prop = child->property ("active")) != 0) {
				if (string_is_affirmative (prop->value())) {
					set_fade_out_active (true);
				} else {
					set_fade_out_active (false);
				}
			}
	
		} else if (child->name() == "InverseFadeIn") {
			XMLNode* grandchild = child->child ("AutomationList");
			if (grandchild) {
				_inverse_fade_in->set_state (*grandchild, version);
			}
		} else if (child->name() == "InverseFadeOut") {
			XMLNode* grandchild = child->child ("AutomationList");
			if (grandchild) {
				_inverse_fade_out->set_state (*grandchild, version);
			}
		}
	}

	_envelope->thaw ();
	resume_property_changes ();

	if (send) {
		send_change (what_changed);
	}

	if (the_playlist) {
		the_playlist->thaw ();
	}

	return 0;
}

int
AudioRegion::set_state (const XMLNode& node, int version)
{
	PropertyChange what_changed;
	return _set_state (node, version, what_changed, true);
}

void
AudioRegion::set_fade_in_shape (FadeShape shape)
{
	set_fade_in (shape, (framecnt_t) _fade_in->back()->when);
}

void
AudioRegion::set_fade_out_shape (FadeShape shape)
{
	set_fade_out (shape, (framecnt_t) _fade_out->back()->when);
}

void
AudioRegion::set_fade_in (boost::shared_ptr<AutomationList> f)
{
	_fade_in->freeze ();
	*(_fade_in.val()) = *f;
	_fade_in->thaw ();
	_default_fade_in = false;

	send_change (PropertyChange (Properties::fade_in));
}

void
AudioRegion::set_fade_in (FadeShape shape, framecnt_t len)
{
	boost::shared_ptr<Evoral::ControlList> c1 (new Evoral::ControlList (FadeInAutomation));
	boost::shared_ptr<Evoral::ControlList> c2 (new Evoral::ControlList (FadeInAutomation));
	boost::shared_ptr<Evoral::ControlList> c3 (new Evoral::ControlList (FadeInAutomation));

	_fade_in->freeze ();
	_fade_in->clear ();
	_inverse_fade_in->clear ();

	switch (shape) {
	case FadeLinear:
		_fade_in->fast_simple_add (0.0, 0.0);
		_fade_in->fast_simple_add (len, 1.0);
		reverse_curve (_inverse_fade_in.val(), _fade_in.val());
		break;

	case FadeFast:
		generate_db_fade (_fade_in.val(), len, 10, -60);
		reverse_curve (c1, _fade_in.val());
		_fade_in->copy_events (*c1);
		generate_inverse_power_curve (_inverse_fade_in.val(), _fade_in.val());
		break;

	case FadeSlow:
		generate_db_fade (c1, len, 10, -1);  // start off with a slow fade
		generate_db_fade (c2, len, 10, -80); // end with a fast fade
		merge_curves (_fade_in.val(), c1, c2);
		reverse_curve (c3, _fade_in.val());
		_fade_in->copy_events (*c3);
		generate_inverse_power_curve (_inverse_fade_in.val(), _fade_in.val());
		break;

	case FadeConstantPower:
		for (int i = 0; i < 9; ++i) {
			float dist = (float) i / 10.0f;
			_fade_in->fast_simple_add (len*dist, sin (dist*M_PI/2));
		}
		_fade_in->fast_simple_add (len, 1.0);
		reverse_curve (_inverse_fade_in.val(), _fade_in.val());
		break;
		
	case FadeSymmetric:
		//start with a nearly linear cuve
		_fade_in->fast_simple_add (0, 1);
		_fade_in->fast_simple_add (0.5*len, 0.6);
		//now generate a fade-out curve by successively applying a gain drop
		const float breakpoint = 0.7;  //linear for first 70%
		const int num_steps = 9;
		for (int i = 2; i < num_steps; i++) {
			float coeff = (1.0-breakpoint);
			for (int j = 0; j < i; j++) {
				coeff *= 0.5;  //6dB drop per step
			}
			_fade_in->fast_simple_add (len* (breakpoint+((1.0-breakpoint)*(double)i/(double)num_steps)), coeff);
		}
		_fade_in->fast_simple_add (len, VERY_SMALL_SIGNAL);
		reverse_curve (c3, _fade_in.val());
		_fade_in->copy_events (*c3);
		reverse_curve (_inverse_fade_in.val(), _fade_in.val());
		break;
	}

	_default_fade_in = false;
	_fade_in->thaw ();
	send_change (PropertyChange (Properties::fade_in));
}

void
AudioRegion::set_fade_out (boost::shared_ptr<AutomationList> f)
{
	_fade_out->freeze ();
	*(_fade_out.val()) = *f;
	_fade_out->thaw ();
	_default_fade_out = false;

	send_change (PropertyChange (Properties::fade_in));
}

void
AudioRegion::set_fade_out (FadeShape shape, framecnt_t len)
{
	boost::shared_ptr<Evoral::ControlList> c1 (new Evoral::ControlList (FadeOutAutomation));
	boost::shared_ptr<Evoral::ControlList> c2 (new Evoral::ControlList (FadeOutAutomation));

	_fade_out->freeze ();
	_fade_out->clear ();
	_inverse_fade_out->clear ();

	switch (shape) {
	case FadeLinear:
		_fade_out->fast_simple_add (0.0, 1.0);
		_fade_out->fast_simple_add (len, VERY_SMALL_SIGNAL);
		reverse_curve (_inverse_fade_out.val(), _fade_out.val());
		break;
		
	case FadeFast: 
		generate_db_fade (_fade_out.val(), len, 10, -60);
		generate_inverse_power_curve (_inverse_fade_out.val(), _fade_out.val());
		break;
		
	case FadeSlow: 
		generate_db_fade (c1, len, 10, -1);  //start off with a slow fade
		generate_db_fade (c2, len, 10, -80);  //end with a fast fade
		merge_curves (_fade_out.val(), c1, c2);
		generate_inverse_power_curve (_inverse_fade_out.val(), _fade_out.val());
		break;

	case FadeConstantPower:
		//constant-power fades use a sin/cos relationship
		//the cutoff is abrupt but it has the benefit of being symmetrical
		_fade_out->fast_simple_add (0.0, 1.0);
		for (int i = 1; i < 9; i++ ) {
			float dist = (float)i/10.0;
			_fade_out->fast_simple_add ((len * dist), cos(dist*M_PI/2));
		}
		_fade_out->fast_simple_add (len, VERY_SMALL_SIGNAL);
		reverse_curve (_inverse_fade_out.val(), _fade_out.val());
		break;
		
	case FadeSymmetric:
		//start with a nearly linear cuve
		_fade_out->fast_simple_add (0, 1);
		_fade_out->fast_simple_add (0.5*len, 0.6);

		//now generate a fade-out curve by successively applying a gain drop
		const float breakpoint = 0.7;  //linear for first 70%
		const int num_steps = 9;
		for (int i = 2; i < num_steps; i++) {
			float coeff = (1.0-breakpoint);
			for (int j = 0; j < i; j++) {
				coeff *= 0.5;  //6dB drop per step
			}
			_fade_out->fast_simple_add (len* (breakpoint+((1.0-breakpoint)*(double)i/(double)num_steps)), coeff);
		}
		_fade_out->fast_simple_add (len, VERY_SMALL_SIGNAL);
		reverse_curve (_inverse_fade_out.val(), _fade_out.val());
		break;
	}

	_default_fade_out = false;
	_fade_out->thaw ();
	send_change (PropertyChange (Properties::fade_out));
}

void
AudioRegion::set_fade_in_length (framecnt_t len)
{
	if (len > _length) {
		len = _length - 1;
	}
	
	if (len < 64) {
		len = 64;
	}

	bool changed = _fade_in->extend_to (len);

	if (changed) {
		if (_inverse_fade_in) {
			_inverse_fade_in->extend_to (len);
		}

		_default_fade_in = false;
		send_change (PropertyChange (Properties::fade_in));
	}
}

void
AudioRegion::set_fade_out_length (framecnt_t len)
{
	if (len > _length) {
		len = _length - 1;
	}

	if (len < 64) {
		len = 64;
	}

	bool changed =	_fade_out->extend_to (len);

	if (changed) {
		
		if (_inverse_fade_out) {
			_inverse_fade_out->extend_to (len);
		}
		_default_fade_out = false;

		send_change (PropertyChange (Properties::fade_out));
	}
}

void
AudioRegion::set_fade_in_active (bool yn)
{
	if (yn == _fade_in_active) {
		return;
	}

	_fade_in_active = yn;
	send_change (PropertyChange (Properties::fade_in_active));
}

void
AudioRegion::set_fade_out_active (bool yn)
{
	if (yn == _fade_out_active) {
		return;
	}
	_fade_out_active = yn;
	send_change (PropertyChange (Properties::fade_out_active));
}

bool
AudioRegion::fade_in_is_default () const
{
	return _fade_in->size() == 2 && _fade_in->front()->when == 0 && _fade_in->back()->when == 64;
}

bool
AudioRegion::fade_out_is_default () const
{
	return _fade_out->size() == 2 && _fade_out->front()->when == 0 && _fade_out->back()->when == 64;
}

void
AudioRegion::set_default_fade_in ()
{
	_fade_in_suspended = 0;
	set_fade_in (FadeLinear, 64);
}

void
AudioRegion::set_default_fade_out ()
{
	_fade_out_suspended = 0;
	set_fade_out (FadeLinear, 64);
}

void
AudioRegion::set_default_fades ()
{
	set_default_fade_in ();
	set_default_fade_out ();
}

void
AudioRegion::set_default_envelope ()
{
	_envelope->freeze ();
	_envelope->clear ();
	_envelope->fast_simple_add (0, 1.0f);
	_envelope->fast_simple_add (_length, 1.0f);
	_envelope->thaw ();
}

void
AudioRegion::recompute_at_end ()
{
	/* our length has changed. recompute a new final point by interpolating
	   based on the the existing curve.
	*/

	_envelope->freeze ();
	_envelope->truncate_end (_length);
	_envelope->thaw ();

	suspend_property_changes();

	if (_left_of_split) {
		set_default_fade_out ();
		_left_of_split = false;
	} else if (_fade_out->back()->when > _length) {
		_fade_out->extend_to (_length);
		send_change (PropertyChange (Properties::fade_out));
	}

	if (_fade_in->back()->when > _length) {
		_fade_in->extend_to (_length);
		send_change (PropertyChange (Properties::fade_in));
	}

	resume_property_changes();
}

void
AudioRegion::recompute_at_start ()
{
	/* as above, but the shift was from the front */

	_envelope->truncate_start (_length);

	suspend_property_changes();

	if (_right_of_split) {
		set_default_fade_in ();
		_right_of_split = false;
	} else if (_fade_in->back()->when > _length) {
		_fade_in->extend_to (_length);
		send_change (PropertyChange (Properties::fade_in));
	}

	if (_fade_out->back()->when > _length) {
		_fade_out->extend_to (_length);
		send_change (PropertyChange (Properties::fade_out));
	}

	resume_property_changes();
}

int
AudioRegion::separate_by_channel (Session& /*session*/, vector<boost::shared_ptr<Region> >& v) const
{
	SourceList srcs;
	string new_name;
	int n = 0;

	if (_sources.size() < 2) {
		return 0;
	}

	for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
		srcs.clear ();
		srcs.push_back (*i);

		new_name = _name;

		if (_sources.size() == 2) {
			if (n == 0) {
				new_name += "-L";
			} else {
				new_name += "-R";
			}
		} else {
			new_name += '-';
			new_name += ('0' + n + 1);
		}

		/* create a copy with just one source. prevent if from being thought of as
		   "whole file" even if it covers the entire source file(s).
		 */

		PropertyList plist;

		plist.add (Properties::start, _start.val());
		plist.add (Properties::length, _length.val());
		plist.add (Properties::name, new_name);
		plist.add (Properties::layer, layer ());

		v.push_back(RegionFactory::create (srcs, plist));
		v.back()->set_whole_file (false);

		++n;
	}

	return 0;
}

framecnt_t
AudioRegion::read_raw_internal (Sample* buf, framepos_t pos, framecnt_t cnt, int channel) const
{
	return audio_source(channel)->read (buf, pos, cnt);
}

void
AudioRegion::set_scale_amplitude (gain_t g)
{
	boost::shared_ptr<Playlist> pl (playlist());

	_scale_amplitude = g;

	/* tell the diskstream we're in */

	if (pl) {
		pl->ContentsChanged();
	}

	/* tell everybody else */

	send_change (PropertyChange (Properties::scale_amplitude));
}

/** @return the maximum (linear) amplitude of the region, or a -ve
 *  number if the Progress object reports that the process was cancelled.
 */
double
AudioRegion::maximum_amplitude (Progress* p) const
{
	framepos_t fpos = _start;
	framepos_t const fend = _start + _length;
	double maxamp = 0;

	framecnt_t const blocksize = 64 * 1024;
	Sample buf[blocksize];

	while (fpos < fend) {

		uint32_t n;

		framecnt_t const to_read = min (fend - fpos, blocksize);

		for (n = 0; n < n_channels(); ++n) {

			/* read it in */

			if (read_raw_internal (buf, fpos, to_read, n) != to_read) {
				return 0;
			}

			maxamp = compute_peak (buf, to_read, maxamp);
		}

		fpos += to_read;
		if (p) {
			p->set_progress (float (fpos - _start) / _length);
			if (p->cancelled ()) {
				return -1;
			}
		}
	}

	return maxamp;
}

/** Normalize using a given maximum amplitude and target, so that region
 *  _scale_amplitude becomes target / max_amplitude.
 */
void
AudioRegion::normalize (float max_amplitude, float target_dB)
{
	gain_t target = dB_to_coefficient (target_dB);

	if (target == 1.0f) {
		/* do not normalize to precisely 1.0 (0 dBFS), to avoid making it appear
		   that we may have clipped.
		*/
		target -= FLT_EPSILON;
	}

	if (max_amplitude == 0.0f) {
		/* don't even try */
		return;
	}

	if (max_amplitude == target) {
		/* we can't do anything useful */
		return;
	}

	set_scale_amplitude (target / max_amplitude);
}

void
AudioRegion::fade_in_changed ()
{
	send_change (PropertyChange (Properties::fade_in));
}

void
AudioRegion::fade_out_changed ()
{
	send_change (PropertyChange (Properties::fade_out));
}

void
AudioRegion::envelope_changed ()
{
	send_change (PropertyChange (Properties::envelope));
}

void
AudioRegion::suspend_fade_in ()
{
	if (++_fade_in_suspended == 1) {
		if (fade_in_is_default()) {
			set_fade_in_active (false);
		}
	}
}

void
AudioRegion::resume_fade_in ()
{
	if (--_fade_in_suspended == 0 && _fade_in_suspended) {
		set_fade_in_active (true);
	}
}

void
AudioRegion::suspend_fade_out ()
{
	if (++_fade_out_suspended == 1) {
		if (fade_out_is_default()) {
			set_fade_out_active (false);
		}
	}
}

void
AudioRegion::resume_fade_out ()
{
	if (--_fade_out_suspended == 0 &&_fade_out_suspended) {
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
	/* XXX this fixes a crash that should not occur. It does occur
	   becauses regions are not being deleted when a session
	   is unloaded. That bug must be fixed.
	*/

	if (_sources.empty()) {
		return;
	}

	boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource>(_sources.front());

	if (afs && afs->destructive()) {
		// set_start (source()->natural_position(), this);
		set_position (source()->natural_position());
	}
}

boost::shared_ptr<AudioSource>
AudioRegion::audio_source (uint32_t n) const
{
	// Guaranteed to succeed (use a static cast for speed?)
	return boost::dynamic_pointer_cast<AudioSource>(source(n));
}

int
AudioRegion::adjust_transients (frameoffset_t delta)
{
	for (AnalysisFeatureList::iterator x = _transients.begin(); x != _transients.end(); ++x) {
		(*x) = (*x) + delta;
	}

	send_change (PropertyChange (Properties::valid_transients));

	return 0;
}

int
AudioRegion::update_transient (framepos_t old_position, framepos_t new_position)
{
	for (AnalysisFeatureList::iterator x = _transients.begin(); x != _transients.end(); ++x) {
		if ((*x) == old_position) {
			(*x) = new_position;
			send_change (PropertyChange (Properties::valid_transients));

			break;
		}
	}

	return 0;
}

void
AudioRegion::add_transient (framepos_t where)
{
	_transients.push_back(where);
	_valid_transients = true;

	send_change (PropertyChange (Properties::valid_transients));
}

void
AudioRegion::remove_transient (framepos_t where)
{
	_transients.remove(where);
	_valid_transients = true;

	send_change (PropertyChange (Properties::valid_transients));
}

int
AudioRegion::set_transients (AnalysisFeatureList& results)
{
	_transients.clear();
	_transients = results;
	_valid_transients = true;

	send_change (PropertyChange (Properties::valid_transients));

	return 0;
}

int
AudioRegion::get_transients (AnalysisFeatureList& results, bool force_new)
{
	boost::shared_ptr<Playlist> pl = playlist();

	if (!pl) {
		return -1;
	}

	if (_valid_transients && !force_new) {
		results = _transients;
		return 0;
	}

	SourceList::iterator s;

	for (s = _sources.begin() ; s != _sources.end(); ++s) {
		if (!(*s)->has_been_analysed()) {
			cerr << "For " << name() << " source " << (*s)->name() << " has not been analyzed\n";
			break;
		}
	}

	if (s == _sources.end()) {
		/* all sources are analyzed, merge data from each one */

		for (s = _sources.begin() ; s != _sources.end(); ++s) {

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
		_valid_transients = true;

		return 0;
	}

	/* no existing/complete transient info */

	static bool analyse_dialog_shown = false; /* global per instance of Ardour */

	if (!Config->get_auto_analyse_audio()) {
		if (!analyse_dialog_shown) {
			pl->session().Dialog (_("\
You have requested an operation that requires audio analysis.\n\n\
You currently have \"auto-analyse-audio\" disabled, which means \
that transient data must be generated every time it is required.\n\n\
If you are doing work that will require transient data on a \
regular basis, you should probably enable \"auto-analyse-audio\" \
then quit ardour and restart.\n\n\
This dialog will not display again.  But you may notice a slight delay \
in this and future transient-detection operations.\n\
"));
			analyse_dialog_shown = true;
		}
	}

	TransientDetector t (pl->session().frame_rate());
	bool existing_results = !results.empty();

	_transients.clear ();
	_valid_transients = false;

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

	_valid_transients = true;

	return 0;
}

/** Find areas of `silence' within a region.
 *
 *  @param threshold Threshold below which signal is considered silence (as a sample value)
 *  @param min_length Minimum length of silent period to be reported.
 *  @return Silent intervals, measured relative to the region start in the source
 */

AudioIntervalResult
AudioRegion::find_silence (Sample threshold, framecnt_t min_length, InterThreadInfo& itt) const
{
	framecnt_t const block_size = 64 * 1024;
	boost::scoped_array<Sample> loudest (new Sample[block_size]);
	boost::scoped_array<Sample> buf (new Sample[block_size]);

	framepos_t pos = _start;
	framepos_t const end = _start + _length - 1;

	AudioIntervalResult silent_periods;

	bool in_silence = false;
	frameoffset_t silence_start = 0;

	while (pos < end && !itt.cancel) {

		/* fill `loudest' with the loudest absolute sample at each instant, across all channels */
		memset (loudest.get(), 0, sizeof (Sample) * block_size);
		for (uint32_t n = 0; n < n_channels(); ++n) {

			read_raw_internal (buf.get(), pos, block_size, n);
			for (framecnt_t i = 0; i < block_size; ++i) {
				loudest[i] = max (loudest[i], abs (buf[i]));
			}
		}

		/* now look for silence */
		for (framecnt_t i = 0; i < block_size; ++i) {
			bool const silence = abs (loudest[i]) < threshold;
			if (silence && !in_silence) {
				/* non-silence to silence */
				in_silence = true;
				silence_start = pos + i;
			} else if (!silence && in_silence) {
				/* silence to non-silence */
				in_silence = false;
				if (pos + i - 1 - silence_start >= min_length) {
					silent_periods.push_back (std::make_pair (silence_start, pos + i - 1));
				}
			}
		}

		pos += block_size;
		itt.progress = (end-pos)/(double)_length;
	}

	if (in_silence && end - 1 - silence_start >= min_length) {
		/* last block was silent, so finish off the last period */
		silent_periods.push_back (std::make_pair (silence_start, end));
	}

	itt.done = true;

	return silent_periods;
}

Evoral::Range<framepos_t>
AudioRegion::body_range () const
{
	return Evoral::Range<framepos_t> (first_frame() + _fade_in->back()->when + 1, last_frame() - _fade_out->back()->when);
}

boost::shared_ptr<Region>
AudioRegion::get_single_other_xfade_region (bool start) const
{
	boost::shared_ptr<Playlist> pl (playlist());

	if (!pl) {
		/* not currently in a playlist - xfade length is unbounded
		   (and irrelevant)
		*/
		return boost::shared_ptr<AudioRegion> ();
	}

	boost::shared_ptr<RegionList> rl;

	if (start) {
		rl = pl->regions_at (position());
	} else {
		rl = pl->regions_at (last_frame());
	}
	
	RegionList::iterator i;
	boost::shared_ptr<Region> other;
	uint32_t n = 0;

	/* count and find the other region in a single pass through the list */

	for (i = rl->begin(); i != rl->end(); ++i) {
		if ((*i).get() != this) {
			other = *i;
		}
		++n;
	}

	if (n != 2) {
		/* zero or multiple regions stacked here - don't care about xfades */
		return boost::shared_ptr<AudioRegion> ();
	}

	return other;
}

framecnt_t
AudioRegion::verify_xfade_bounds (framecnt_t len, bool start)
{
	/* this is called from a UI to check on whether a new proposed
	   length for an xfade is legal or not. it returns the legal
	   length corresponding to @a len which may be shorter than or
	   equal to @a len itself.
	*/

	boost::shared_ptr<Region> other = get_single_other_xfade_region (start);
	framecnt_t maxlen;

	if (!other) {
		/* zero or > 2 regions here, don't care about len, but
		   it can't be longer than the region itself.
		 */
		return min (length(), len);
	}

	/* we overlap a single region. clamp the length of an xfade to
	   the maximum possible duration of the overlap (if the other
	   region were trimmed appropriately).
	*/

	if (start) {
		maxlen = other->latest_possible_frame() - position();
	} else {
		maxlen = last_frame() - other->earliest_possible_position();
	}

	return min (length(), min (maxlen, len));
		
}

