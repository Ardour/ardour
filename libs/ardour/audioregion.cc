/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
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

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <memory>
#include <set>

#include <boost/scoped_array.hpp>

#include <glibmm/fileutils.h>
#include <glibmm/threads.h>

#include "pbd/gstdio_compat.h"
#include "pbd/basename.h"
#include "pbd/xml++.h"
#include "pbd/enumwriter.h"
#include "pbd/convert.h"
#include "pbd/progress.h"

#include "evoral/Curve.h"

#include "ardour/audioengine.h"
#include "ardour/analysis_graph.h"
#include "ardour/audioregion.h"
#include "ardour/buffer_manager.h"
#include "ardour/session.h"
#include "ardour/dB.h"
#include "ardour/debug.h"
#include "ardour/event_type_map.h"
#include "ardour/playlist.h"
#include "ardour/audiofilesource.h"
#include "ardour/region_factory.h"
#include "ardour/region_fx_plugin.h"
#include "ardour/runtime_functions.h"
#include "ardour/sndfilesource.h"
#include "ardour/transient_detector.h"
#include "ardour/parameter_descriptor.h"

#include "audiographer/general/interleaver.h"
#include "audiographer/general/sample_format_converter.h"
#include "audiographer/sndfile/sndfile_writer.h"

#include "pbd/i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

#define S2SC(s) Temporal::samples_to_superclock (s, TEMPORAL_SAMPLE_RATE)
#define SC2S(s) Temporal::superclock_to_samples (s, TEMPORAL_SAMPLE_RATE)

namespace ARDOUR {
	namespace Properties {
		PBD::PropertyDescriptor<bool> envelope_active;
		PBD::PropertyDescriptor<bool> default_fade_in;
		PBD::PropertyDescriptor<bool> default_fade_out;
		PBD::PropertyDescriptor<bool> fade_in_active;
		PBD::PropertyDescriptor<bool> fade_out_active;
		PBD::PropertyDescriptor<float> scale_amplitude;
		PBD::PropertyDescriptor<std::shared_ptr<AutomationList> > fade_in;
		PBD::PropertyDescriptor<std::shared_ptr<AutomationList> > inverse_fade_in;
		PBD::PropertyDescriptor<std::shared_ptr<AutomationList> > fade_out;
		PBD::PropertyDescriptor<std::shared_ptr<AutomationList> > inverse_fade_out;
		PBD::PropertyDescriptor<std::shared_ptr<AutomationList> > envelope;
	}
}

/* Curve manipulations */

static void
reverse_curve (std::shared_ptr<Evoral::ControlList> dst, std::shared_ptr<const Evoral::ControlList> src)
{
	const timepos_t end = src->when(false);
	// TODO read-lock of src (!)
	for (Evoral::ControlList::const_reverse_iterator it = src->rbegin(); it!=src->rend(); it++) {
		/* ugh ... the double "distance" calls (with totally different
		   semantics ... horrible
		*/
		dst->fast_simple_add (timepos_t ((*it)->when.distance (end)), (*it)->value);
	}
}

static void
generate_inverse_power_curve (std::shared_ptr<Evoral::ControlList> dst, std::shared_ptr<const Evoral::ControlList> src)
{
	// calc inverse curve using sum of squares
	for (Evoral::ControlList::const_iterator it = src->begin(); it!=src->end(); ++it ) {
		float value = (*it)->value;
		value = 1 - powf(value,2);
		value = sqrtf(value);
		dst->fast_simple_add ((*it)->when, value );
	}
}

static void
generate_db_fade (std::shared_ptr<Evoral::ControlList> dst, double len, int num_steps, float dB_drop)
{
	dst->clear ();
	dst->fast_simple_add (timepos_t (Temporal::AudioTime), 1);

	//generate a fade-out curve by successively applying a gain drop
	float fade_speed = dB_to_coefficient(dB_drop / (float) num_steps);
	float coeff = GAIN_COEFF_UNITY;
	for (int i = 1; i < (num_steps-1); i++) {
		coeff *= fade_speed;
		dst->fast_simple_add (timepos_t (samplepos_t (len*(double)i/(double)num_steps)), coeff);
	}

	dst->fast_simple_add (timepos_t ((samplepos_t)len), GAIN_COEFF_SMALL);
}

static void
merge_curves (std::shared_ptr<Evoral::ControlList> dst,
	      std::shared_ptr<const Evoral::ControlList> curve1,
	      std::shared_ptr<const Evoral::ControlList> curve2)
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
		dst->fast_simple_add ((*c1)->when, interp );
		c1++;
		count++;
	}
}

void
AudioRegion::make_property_quarks ()
{
	Properties::envelope_active.property_id = g_quark_from_static_string (X_("envelope-active"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for envelope-active = %1\n", Properties::envelope_active.property_id));
	Properties::default_fade_in.property_id = g_quark_from_static_string (X_("default-fade-in"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for default-fade-in = %1\n", Properties::default_fade_in.property_id));
	Properties::default_fade_out.property_id = g_quark_from_static_string (X_("default-fade-out"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for default-fade-out = %1\n", Properties::default_fade_out.property_id));
	Properties::fade_in_active.property_id = g_quark_from_static_string (X_("fade-in-active"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for fade-in-active = %1\n", Properties::fade_in_active.property_id));
	Properties::fade_out_active.property_id = g_quark_from_static_string (X_("fade-out-active"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for fade-out-active = %1\n", Properties::fade_out_active.property_id));
	Properties::scale_amplitude.property_id = g_quark_from_static_string (X_("scale-amplitude"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for scale-amplitude = %1\n", Properties::scale_amplitude.property_id));
	Properties::fade_in.property_id = g_quark_from_static_string (X_("FadeIn"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for FadeIn = %1\n", Properties::fade_in.property_id));
	Properties::inverse_fade_in.property_id = g_quark_from_static_string (X_("InverseFadeIn"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for InverseFadeIn = %1\n", Properties::inverse_fade_in.property_id));
	Properties::fade_out.property_id = g_quark_from_static_string (X_("FadeOut"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for FadeOut = %1\n", Properties::fade_out.property_id));
	Properties::inverse_fade_out.property_id = g_quark_from_static_string (X_("InverseFadeOut"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for InverseFadeOut = %1\n", Properties::inverse_fade_out.property_id));
	Properties::envelope.property_id = g_quark_from_static_string (X_("Envelope"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for Envelope = %1\n", Properties::envelope.property_id));
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

#define AUDIOREGION_STATE_DEFAULT(tdp) \
	_envelope_active (Properties::envelope_active, false) \
	, _default_fade_in (Properties::default_fade_in, true) \
	, _default_fade_out (Properties::default_fade_out, true) \
	, _fade_in_active (Properties::fade_in_active, true) \
	, _fade_out_active (Properties::fade_out_active, true) \
	, _scale_amplitude (Properties::scale_amplitude, 1.0) \
	, _fade_in (Properties::fade_in, std::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (FadeInAutomation), tdp))) \
	, _inverse_fade_in (Properties::inverse_fade_in, std::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (FadeInAutomation), tdp))) \
	, _fade_out (Properties::fade_out, std::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (FadeOutAutomation), tdp))) \
	, _inverse_fade_out (Properties::inverse_fade_out, std::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (FadeOutAutomation), tdp)))

#define AUDIOREGION_COPY_STATE(other) \
	_envelope_active (Properties::envelope_active, other->_envelope_active) \
	, _default_fade_in (Properties::default_fade_in, other->_default_fade_in) \
	, _default_fade_out (Properties::default_fade_out, other->_default_fade_out) \
	, _fade_in_active (Properties::fade_in_active, other->_fade_in_active) \
	, _fade_out_active (Properties::fade_out_active, other->_fade_out_active) \
	, _scale_amplitude (Properties::scale_amplitude, other->_scale_amplitude) \
	, _fade_in (Properties::fade_in, std::shared_ptr<AutomationList> (new AutomationList (*other->_fade_in.val()))) \
	, _inverse_fade_in (Properties::fade_in, std::shared_ptr<AutomationList> (new AutomationList (*other->_inverse_fade_in.val()))) \
	, _fade_out (Properties::fade_in, std::shared_ptr<AutomationList> (new AutomationList (*other->_fade_out.val()))) \
	, _inverse_fade_out (Properties::fade_in, std::shared_ptr<AutomationList> (new AutomationList (*other->_inverse_fade_out.val())))
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

	_fx_pos = _cache_start = _cache_end = -1;
	_fx_block_size = 0;
	_fx_latent_read = false;
}

void
AudioRegion::send_change (const PropertyChange& what_changed)
{

	PropertyChange our_interests;

	our_interests.add (Properties::fade_in_active);
	our_interests.add (Properties::fade_out_active);
	our_interests.add (Properties::scale_amplitude);
	our_interests.add (Properties::envelope_active);
	our_interests.add (Properties::envelope);
	our_interests.add (Properties::fade_in);
	our_interests.add (Properties::fade_out);

	if (what_changed.contains (our_interests)) {
		_invalidated.exchange (true);
	}

	Region::send_change (what_changed);
}

void
AudioRegion::copy_plugin_state (std::shared_ptr<const AudioRegion> other)
{
	/* state cannot copied in Region, because when running Region's c'tor
	 * the AudioRegion does not yet exist, and virtual _add_plugin
	 * of the parent class is called
	 */
	Glib::Threads::RWLock::ReaderLock lm (other->_fx_lock);
	for (auto const& i : other->_plugins) {
		XMLNode& state = i->get_state ();
		state.remove_property ("count");
		PBD::Stateful::ForceIDRegeneration force_ids;
		std::shared_ptr<RegionFxPlugin> rfx (new RegionFxPlugin (_session, Temporal::AudioTime));
		rfx->set_state (state, Stateful::current_state_version);
		if (!_add_plugin (rfx, std::shared_ptr<RegionFxPlugin>(), true)) {
			continue;
		}
		_plugins.push_back (rfx);
		delete &state;
	}
	fx_latency_changed (true);
}

/** Constructor for use by derived types only */
AudioRegion::AudioRegion (Session& s, timepos_t const &  start, timecnt_t const & len, std::string name)
	: Region (s, start, len, name, DataType::AUDIO)
	, AUDIOREGION_STATE_DEFAULT(Temporal::TimeDomainProvider (Temporal::AudioTime))
	, _envelope (Properties::envelope, std::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter(EnvelopeAutomation), Temporal::TimeDomainProvider (Temporal::AudioTime))))
	, _automatable (s, Temporal::TimeDomainProvider (Temporal::AudioTime))
	, _fade_in_suspended (0)
	, _fade_out_suspended (0)
{
	init ();
	assert (_sources.size() == _master_sources.size());
}

/** Basic AudioRegion constructor */
AudioRegion::AudioRegion (const SourceList& srcs)
	: Region (srcs)
	, AUDIOREGION_STATE_DEFAULT(Temporal::TimeDomainProvider (Temporal::AudioTime))
	, _envelope (Properties::envelope, std::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter(EnvelopeAutomation), Temporal::TimeDomainProvider (Temporal::AudioTime))))
	, _automatable(srcs[0]->session(), Temporal::TimeDomainProvider (Temporal::AudioTime))
	, _fade_in_suspended (0)
	, _fade_out_suspended (0)
{
	init ();
	assert (_sources.size() == _master_sources.size());
}

AudioRegion::AudioRegion (std::shared_ptr<const AudioRegion> other)
	: Region (other)
	, AUDIOREGION_COPY_STATE (other)
	  /* As far as I can see, the _envelope's times are relative to region position, and have nothing
		 * to do with sources (and hence _start).  So when we copy the envelope, we just use the supplied offset.
		 */
	, _envelope (Properties::envelope, std::shared_ptr<AutomationList> (new AutomationList (*other->_envelope.val(), timepos_t (Temporal::AudioTime), other->len_as_tpos ())))
	, _automatable (other->session(), Temporal::TimeDomainProvider (Temporal::AudioTime))
	, _fade_in_suspended (0)
	, _fade_out_suspended (0)
{
	/* don't use init here, because we got fade in/out from the other region
	*/
	register_properties ();
	listen_to_my_curves ();
	connect_to_analysis_changed ();
	connect_to_header_position_offset_changed ();

	_fx_pos = _cache_start = _cache_end = -1;
	_fx_block_size = 0;
	_fx_latent_read = false;

	copy_plugin_state (other);

	assert(_type == DataType::AUDIO);
	assert (_sources.size() == _master_sources.size());
}

AudioRegion::AudioRegion (std::shared_ptr<const AudioRegion> other, timecnt_t const & offset)
	: Region (other, offset)
	, AUDIOREGION_COPY_STATE (other)
	  /* As far as I can see, the _envelope's times are relative to region position, and have nothing
	     to do with sources (and hence _start).  So when we copy the envelope, we just use the supplied offset.
	  */
	, _envelope (Properties::envelope, std::shared_ptr<AutomationList> (new AutomationList (*other->_envelope.val(), timepos_t (offset.samples()), other->len_as_tpos ())))
	, _automatable (other->session(), Temporal::TimeDomainProvider (Temporal::AudioTime))
	, _fade_in_suspended (0)
	, _fade_out_suspended (0)
{
	/* don't use init here, because we got fade in/out from the other region
	*/
	register_properties ();
	listen_to_my_curves ();
	connect_to_analysis_changed ();
	connect_to_header_position_offset_changed ();

	_fx_pos = _cache_start = _cache_end = -1;
	_fx_block_size = 0;
	_fx_latent_read = false;

	copy_plugin_state (other);

	assert(_type == DataType::AUDIO);
	assert (_sources.size() == _master_sources.size());
}

AudioRegion::AudioRegion (std::shared_ptr<const AudioRegion> other, const SourceList& srcs)
	: Region (std::static_pointer_cast<const Region>(other), srcs)
	, AUDIOREGION_COPY_STATE (other)
	, _envelope (Properties::envelope, std::shared_ptr<AutomationList> (new AutomationList (*other->_envelope.val())))
	, _automatable (other->session(), Temporal::TimeDomainProvider (Temporal::AudioTime))
	, _fade_in_suspended (0)
	, _fade_out_suspended (0)
{
	/* make-a-sort-of-copy-with-different-sources constructor (used by audio filter) */

	register_properties ();

	listen_to_my_curves ();
	connect_to_analysis_changed ();
	connect_to_header_position_offset_changed ();

	_fx_pos = _cache_start = _cache_end = -1;
	_fx_block_size = 0;
	_fx_latent_read = false;

	copy_plugin_state (other);

	assert (_sources.size() == _master_sources.size());
}

AudioRegion::AudioRegion (SourceList& srcs)
	: Region (srcs)
	, AUDIOREGION_STATE_DEFAULT(srcs[0]->session())
	, _envelope (Properties::envelope, std::shared_ptr<AutomationList> (new AutomationList(Evoral::Parameter(EnvelopeAutomation), Temporal::TimeDomainProvider (Temporal::AudioTime))))
	, _automatable(srcs[0]->session(), Temporal::TimeDomainProvider (Temporal::AudioTime))
	, _fade_in_suspended (0)
	, _fade_out_suspended (0)
{
	init ();

	assert(_type == DataType::AUDIO);
	assert (_sources.size() == _master_sources.size());
}

AudioRegion::~AudioRegion ()
{
	for (auto const& rfx : _plugins) {
		rfx->drop_references ();
	}
}

void
AudioRegion::post_set (const PropertyChange& /*ignored*/)
{
	if (!_sync_marked) {
		_sync_position = _start;
	}

	/* return to default fades if the existing ones are too long */

	if (_left_of_split) {
		if (_fade_in->when(false) >= len_as_tpos ()) {
			set_default_fade_in ();
		}
		set_default_fade_out ();
		_left_of_split = false;
	}

	if (_right_of_split) {
		if (_fade_out->when(false) >= len_as_tpos ()) {
			set_default_fade_out ();
		}

		set_default_fade_in ();
		_right_of_split = false;
	}

	/* If _length changed, adjust our gain envelope accordingly */
	_envelope->truncate_end (len_as_tpos ());
}

void
AudioRegion::connect_to_analysis_changed ()
{
	for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {
		(*i)->AnalysisChanged.connect_same_thread (*this, boost::bind (&AudioRegion::maybe_invalidate_transients, this));
	}
}

void
AudioRegion::connect_to_header_position_offset_changed ()
{
	set<std::shared_ptr<Source> > unique_srcs;

	for (SourceList::const_iterator i = _sources.begin(); i != _sources.end(); ++i) {

		/* connect only once to HeaderPositionOffsetChanged, even if sources are replicated
		 */

		if (unique_srcs.find (*i) == unique_srcs.end ()) {
			unique_srcs.insert (*i);
			std::shared_ptr<AudioFileSource> afs = std::dynamic_pointer_cast<AudioFileSource> (*i);
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
 *  @param samples_per_pixel Number of samples to use to generate one peak value.
 */

ARDOUR::samplecnt_t
AudioRegion::read_peaks (PeakData *buf, samplecnt_t npeaks, samplecnt_t offset, samplecnt_t cnt, uint32_t chan_n, double samples_per_pixel) const
{
	if (chan_n >= _sources.size()) {
		return 0;
	}

	if (audio_source(chan_n)->read_peaks (buf, npeaks, offset, cnt, samples_per_pixel)) {
		return 0;
	}

	if (_scale_amplitude < 0.f) {
		for (samplecnt_t n = 0; n < npeaks; ++n) {
			const float tmp = buf[n].max;
			buf[n].max = _scale_amplitude * buf[n].min;
			buf[n].min = _scale_amplitude * tmp;
		}
	} else if (_scale_amplitude != 1.0f) {
		for (samplecnt_t n = 0; n < npeaks; ++n) {
			buf[n].max *= _scale_amplitude;
			buf[n].min *= _scale_amplitude;
		}
	}

	return npeaks;
}

/** @param buf Buffer to write data to (existing data will be overwritten).
 *  @param pos Position to read from as an offset from the region position.
 *  @param cnt Number of samples to read.
 *  @param channel Channel to read from.
 */
samplecnt_t
AudioRegion::read (Sample* buf, samplepos_t pos, samplecnt_t cnt, int channel) const
{
	/* raw read, no fades, no gain, nada */
	return read_from_sources (_sources, _length.val().samples(), buf, position().samples() + pos, cnt, channel);
}

samplecnt_t
AudioRegion::master_read_at (Sample* buf, samplepos_t position, samplecnt_t cnt, uint32_t chan_n) const
{
	/* do not read gain/scaling/fades and do not count this disk i/o in statistics */

	assert (cnt >= 0);
	return read_from_sources (_master_sources, _master_sources.front()->length ().samples(), buf, position, cnt, chan_n);
}

/** @param buf Buffer to mix data into.
 *  @param mixdown_buffer Scratch buffer for audio data.
 *  @param gain_buffer Scratch buffer for gain data.
 *  @param pos Position within the session to read from.
 *  @param cnt Number of samples to read.
 *  @param chan_n Channel number to read.
 */
samplecnt_t
AudioRegion::read_at (Sample*     buf,
                      Sample*     mixdown_buffer,
                      gain_t*     gain_buffer,
                      samplepos_t pos,
                      samplecnt_t cnt,
                      uint32_t    chan_n) const
{
	/* We are reading data from this region into buf (possibly via mixdown_buffer).
	   The caller has verified that we cover the desired section.
	*/

	/* See doc/region_read.svg for a drawing which might help to explain
	   what is going on.
	*/

	assert (cnt >= 0);
	uint32_t const n_chn = n_channels ();

	if (n_chn == 0) {
		return 0;
	}

	/* WORK OUT WHERE TO GET DATA FROM */

	samplecnt_t to_read;
	const samplepos_t psamples = position().samples();
	const samplecnt_t lsamples = _length.val().samples();

	assert (pos >= psamples);
	sampleoffset_t const internal_offset = pos - psamples;

	if (internal_offset >= lsamples) {
		return 0; /* read nothing */
	}

	const samplecnt_t esamples = lsamples - internal_offset;
	assert (esamples >= 0);

	if ((to_read = min (cnt, esamples)) == 0) {
		return 0; /* read nothing */
	}

	std::shared_ptr<Playlist> pl (playlist());
	if (!pl){
		return 0;
	}

	/* COMPUTE DETAILS OF ANY FADES INVOLVED IN THIS READ */

	/* Amount (length) of fade in that we are dealing with in this read */
	samplecnt_t fade_in_limit = 0;

	/* Offset from buf / mixdown_buffer of the start
	   of any fade out that we are dealing with
	*/
	sampleoffset_t fade_out_offset = 0;

	/* Amount (length) of fade out that we are dealing with in this read */
	samplecnt_t fade_out_limit = 0;

	samplecnt_t fade_interval_start = 0;

	/* Fade in */

	if (_fade_in_active && _session.config.get_use_region_fades()) {

		samplecnt_t fade_in_length = _fade_in->when(false).samples();

		/* see if this read is within the fade in */

		if (internal_offset < fade_in_length) {
			fade_in_limit = min (to_read, fade_in_length - internal_offset);
		}
	}

	/* Fade out */

	if (_fade_out_active && _session.config.get_use_region_fades()) {

		/* see if some part of this read is within the fade out */

		/* .................        >|            REGION
		 *                           _length
		 *
		 *               {           }            FADE
		 *                           fade_out_length
		 *               ^
		 *               _length - fade_out_length
		 *
		 *      |--------------|
		 *      ^internal_offset
		 *                     ^internal_offset + to_read
		 *
		 *                     we need the intersection of [internal_offset,internal_offset+to_read] with
		 *                     [_length - fade_out_length, _length]
		 *
		 */

		fade_interval_start = max (internal_offset, lsamples - _fade_out->when(false).samples());
		samplecnt_t fade_interval_end = min(internal_offset + to_read, lsamples);

		if (fade_interval_end > fade_interval_start) {
			/* (part of the) the fade out is in this buffer */
			fade_out_limit = fade_interval_end - fade_interval_start;
			fade_out_offset = fade_interval_start - internal_offset;
		}
	}

	Glib::Threads::Mutex::Lock cl (_cache_lock);
	if (chan_n == 0 && _invalidated.exchange (false)) {
		_cache_start = _cache_end = -1;
	}

	boost::scoped_array<gain_t> gain_array;
	boost::scoped_array<Sample> mixdown_array;

	// TODO optimize mono reader, w/o plugins -> old code
	if (n_chn > 1 && _cache_start < _cache_end && internal_offset >= _cache_start && internal_offset + to_read <= _cache_end) {
		DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("Region '%1' channel: %2 copy from cache %3 - %4 to_read: %5\n",
		             name(), chan_n, internal_offset, internal_offset + to_read, to_read));
		copy_vector (mixdown_buffer, _readcache.get_audio (chan_n).data (internal_offset - _cache_start), to_read);
		cl.release ();
	} else {
		Glib::Threads::RWLock::ReaderLock lm (_fx_lock);
		bool have_fx        = !_plugins.empty ();
		uint32_t fx_latency = _fx_latency;
		lm.release ();

		ChanCount cc (DataType::AUDIO, n_channels ());
		_readcache.ensure_buffers (cc, to_read + _fx_latency);

		samplecnt_t    n_read = to_read; //< data to read from disk
		samplecnt_t    n_proc = to_read; //< silence pad data to process
		samplepos_t    readat = pos;
		sampleoffset_t offset = internal_offset;

		//printf ("READ Cache end %ld pos %ld\n", _cache_end, readat);
		if (_cache_end != readat && fx_latency > 0) {
			_fx_latent_read = true;
			n_proc += fx_latency;
			n_read = min (to_read + fx_latency, esamples);

			mixdown_array.reset (new Sample[n_proc]);
			mixdown_buffer = mixdown_array.get ();
			gain_array.reset (new gain_t[n_proc]);
			gain_buffer = gain_array.get ();
		}

		if (!_fx_latent_read && fx_latency > 0) {
			offset += fx_latency;
			readat += fx_latency;
			n_read = max<samplecnt_t> (0, min (to_read, lsamples - offset));
		}

		DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("Region '%1' channel: %2 read: %3 - %4 (%5) to_read: %6 offset: %7 with fx: %8 fx_latency: %9\n",
		             name(), chan_n, readat, readat + n_read, n_read, to_read, internal_offset, have_fx, fx_latency));

		_readcache.ensure_buffers (cc, n_proc);

		if (n_read < n_proc) {
			//printf ("SILENCE PAD rd: %ld proc: %ld\n", n_read, n_proc);
			/* silence pad, process tail of latent effects */
			memset (&mixdown_buffer[n_read], 0, sizeof (Sample)* (n_proc - n_read));
			_readcache.silence (n_proc - n_read, n_read);
		}

		/* reset in case read fails we return early */
		_cache_start = _cache_end = -1;

		for (uint32_t chn = 0; chn < n_chn; ++chn) {
			/* READ DATA FROM THE SOURCE INTO mixdown_buffer.
			 * We can never read directly into buf, since it may contain data
			 * from a region `below' this one in the stack, and our fades (if they exist)
			 * may need to mix with the existing data.
			 */

			if (read_from_sources (_sources, lsamples, mixdown_buffer, readat, n_read, chn) != n_read) {
				return 0; // XXX
			}

			/* APPLY REGULAR GAIN CURVES AND SCALING TO mixdown_buffer */
			if (envelope_active())  {
				_envelope->curve().get_vector (timepos_t (offset), timepos_t (offset + n_read), gain_buffer, n_read);

				if (_scale_amplitude != 1.0f) {
					for (samplecnt_t n = 0; n < n_read; ++n) {
						mixdown_buffer[n] *= gain_buffer[n] * _scale_amplitude;
					}
				} else {
					for (samplecnt_t n = 0; n < n_read; ++n) {
						mixdown_buffer[n] *= gain_buffer[n];
					}
				}
			} else if (_scale_amplitude != 1.0f) {
				apply_gain_to_buffer (mixdown_buffer, n_read, _scale_amplitude);
			}

			/* for mono regions no cache is required, unless there are
			 * regionFX, which use the _readcache BufferSet.
			 */
			if (n_chn > 1 || have_fx) {
				_readcache.get_audio (chn).read_from (mixdown_buffer, n_proc);
			}
		}

		/* apply region FX to all channels */
		if (have_fx) {
			const_cast<AudioRegion*>(this)->apply_region_fx (_readcache, offset, offset + n_proc, n_proc);
		}

		/* for mono regions without plugins, mixdown_buffer is valid as-is */
		if (n_chn > 1 || have_fx) {
			/* copy data for current channel */
			copy_vector (mixdown_buffer, _readcache.get_audio (chan_n).data (), to_read);
		}

		_cache_start = internal_offset;
		_cache_end = internal_offset + to_read;
		cl.release ();
	}

	/* APPLY FADES TO THE DATA IN mixdown_buffer AND MIX THE RESULTS INTO
	 * buf. The key things to realize here: (1) the fade being applied is
	 * (as of April 26th 2012) just the inverse of the fade in curve (2)
	 * "buf" contains data from lower regions already. So this operation
	 * fades out the existing material.
	 */

	bool is_opaque = opaque();

	if (fade_in_limit != 0) {

		if (is_opaque) {
			if (_inverse_fade_in) {

				/* explicit inverse fade in curve (e.g. for constant
				 * power), so we have to fetch it.
				 */

				_inverse_fade_in->curve().get_vector (timepos_t (internal_offset), timepos_t (internal_offset + fade_in_limit), gain_buffer, fade_in_limit);

				/* Fade the data from lower layers out */
				for (samplecnt_t n = 0; n < fade_in_limit; ++n) {
					buf[n] *= gain_buffer[n];
				}

				/* refill gain buffer with the fade in */

				_fade_in->curve().get_vector (timepos_t (internal_offset), timepos_t (internal_offset + fade_in_limit), gain_buffer, fade_in_limit);

			} else {

				/* no explicit inverse fade in, so just use (1 - fade
				 * in) for the fade out of lower layers
				 */

				_fade_in->curve().get_vector (timepos_t (internal_offset), timepos_t (internal_offset + fade_in_limit), gain_buffer, fade_in_limit);

				for (samplecnt_t n = 0; n < fade_in_limit; ++n) {
					buf[n] *= 1 - gain_buffer[n];
				}
			}
		} else {
			_fade_in->curve().get_vector (timepos_t (internal_offset), timepos_t (internal_offset + fade_in_limit), gain_buffer, fade_in_limit);
		}

		/* Mix our newly-read data in, with the fade */
		for (samplecnt_t n = 0; n < fade_in_limit; ++n) {
			buf[n] += mixdown_buffer[n] * gain_buffer[n];
		}
	}

	if (fade_out_limit != 0) {

		samplecnt_t const curve_offset = fade_interval_start - _fade_out->when(false).distance (len_as_tpos ()).samples();

		if (is_opaque) {
			if (_inverse_fade_out) {

				_inverse_fade_out->curve().get_vector (timepos_t (curve_offset), timepos_t (curve_offset + fade_out_limit), gain_buffer, fade_out_limit);

				/* Fade the data from lower levels in */
				for (samplecnt_t n = 0, m = fade_out_offset; n < fade_out_limit; ++n, ++m) {
					buf[m] *= gain_buffer[n];
				}

				/* fetch the actual fade out */

				_fade_out->curve().get_vector (timepos_t (curve_offset), timepos_t (curve_offset + fade_out_limit), gain_buffer, fade_out_limit);

			} else {

				/* no explicit inverse fade out (which is
				 * actually a fade in), so just use (1 - fade
				 * out) for the fade in of lower layers
				 */

				_fade_out->curve().get_vector (timepos_t (curve_offset), timepos_t (curve_offset + fade_out_limit), gain_buffer, fade_out_limit);

				for (samplecnt_t n = 0, m = fade_out_offset; n < fade_out_limit; ++n, ++m) {
					buf[m] *= 1 - gain_buffer[n];
				}
			}
		} else {
			_fade_out->curve().get_vector (timepos_t (curve_offset), timepos_t (curve_offset + fade_out_limit), gain_buffer, fade_out_limit);
		}

		/* Mix our newly-read data with whatever was already there,
		   with the fade out applied to our data.
		*/
		for (samplecnt_t n = 0, m = fade_out_offset; n < fade_out_limit; ++n, ++m) {
			buf[m] += mixdown_buffer[m] * gain_buffer[n];
		}
	}

	/* MIX OR COPY THE REGION BODY FROM mixdown_buffer INTO buf */

	samplecnt_t const N = to_read - fade_in_limit - fade_out_limit;

	if (N > 0) {
		if (is_opaque) {
			DEBUG_TRACE (DEBUG::AudioPlayback, string_compose ("Region %1 memcpy into buf @ %2 + %3, from mixdown buffer @ %4 + %5, len = %6 cnt was %7\n",
									   name(), buf, fade_in_limit, mixdown_buffer, fade_in_limit, N, cnt));
			copy_vector (buf + fade_in_limit, mixdown_buffer + fade_in_limit, N);
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
 *  @param pos Position to read from, in session samples.
 *  @param cnt Number of samples to read.
 *  @param chan_n Channel to read from.
 *  @return Number of samples read.
 */

samplecnt_t
AudioRegion::read_from_sources (SourceList const & srcs, samplecnt_t limit, Sample* buf, samplepos_t pos, samplecnt_t cnt, uint32_t chan_n) const
{
	sampleoffset_t const internal_offset = pos - position().samples();

	if (internal_offset >= limit) {
		return 0;
	}

	samplecnt_t const to_read = min (cnt, limit - internal_offset);
	if (to_read == 0) {
		return 0;
	}

	if (chan_n < n_channels()) {

		std::shared_ptr<AudioSource> src = std::dynamic_pointer_cast<AudioSource> (srcs[chan_n]);

		if (src->read (buf, _start.val().samples() + internal_offset, to_read) != to_read) {
			return 0; /* "read nothing" */
		}

	} else {

		/* track is N-channel, this region has fewer channels; silence the ones
		   we don't have.
		*/

		if (Config->get_replicate_missing_region_channels()) {

			/* copy an existing channel's data in for this non-existant one */

			uint32_t channel = chan_n % n_channels();
			std::shared_ptr<AudioSource> src = std::dynamic_pointer_cast<AudioSource> (srcs[channel]);

			if (src->read (buf, _start.val().samples() + internal_offset, to_read) != to_read) {
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
AudioRegion::get_basic_state () const
{
	XMLNode& node (Region::state ());

	node.set_property ("channels", (uint32_t)_sources.size());

	return node;
}

XMLNode&
AudioRegion::state () const
{
	XMLNode& node (get_basic_state());
	XMLNode *child;

	child = node.add_child ("Envelope");

	bool default_env = false;

	// If there are only two points, the points are in the start of the region and the end of the region
	// so, if they are both at 1.0f, that means the default region.

	if (_envelope->size() == 2 &&
	    _envelope->front()->value == GAIN_COEFF_UNITY &&
	    _envelope->back()->value==GAIN_COEFF_UNITY) {
		if (_envelope->front()->when == 0 && _envelope->back()->when == len_as_tpos ()) {
			default_env = true;
		}
	}

	if (default_env) {
		child->set_property ("default", "yes");
	} else {
		child->add_child_nocopy (_envelope->get_state ());
	}

	child = node.add_child (X_("FadeIn"));

	if (_default_fade_in) {
		child->set_property ("default", "yes");
	} else {
		child->add_child_nocopy (_fade_in->get_state ());
	}

	if (_inverse_fade_in) {
		child = node.add_child (X_("InverseFadeIn"));
		child->add_child_nocopy (_inverse_fade_in->get_state ());
	}

	child = node.add_child (X_("FadeOut"));

	if (_default_fade_out) {
		child->set_property ("default", "yes");
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
	std::shared_ptr<Playlist> the_playlist (_playlist.lock());

	suspend_property_changes ();

	if (the_playlist) {
		the_playlist->freeze ();
	}


	/* this will set all our State members and stuff controlled by the Region.
	   It should NOT send any changed signals - that is our responsibility.
	*/

	Region::_set_state (node, version, what_changed, false);

	float val;
	if (node.get_property ("scale-gain", val)) {
		if (val != _scale_amplitude) {
			_scale_amplitude = val;
			what_changed.add (Properties::scale_amplitude);
		}
	}

	/* Now find envelope description and other related child items */

	_envelope->freeze ();

	for (XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {
		XMLNode *child;
		XMLProperty const * prop;

		child = (*niter);

		if (child->name() == "Envelope") {

			_envelope->clear ();

			if ((prop = child->property ("default")) != 0 || _envelope->set_state (*child, version)) {
				set_default_envelope ();
			}

			_envelope->truncate_end (len_as_tpos ());


		} else if (child->name() == "FadeIn") {

			_fade_in->clear ();

			bool is_default;
			if ((child->get_property ("default", is_default) && is_default) || (prop = child->property ("steepness")) != 0) {
				set_default_fade_in ();
			} else {
				XMLNode* grandchild = child->child ("AutomationList");
				if (grandchild) {
					_fade_in->set_state (*grandchild, version);
				}
			}

			bool is_active;
			if (child->get_property ("active", is_active)) {
				set_fade_in_active (is_active);
			}

		} else if (child->name() == "FadeOut") {

			_fade_out->clear ();

			bool is_default;
			if ((child->get_property ("default", is_default) && is_default) || (prop = child->property ("steepness")) != 0) {
				set_default_fade_out ();
			} else {
				XMLNode* grandchild = child->child ("AutomationList");
				if (grandchild) {
					_fade_out->set_state (*grandchild, version);
				}
			}

			bool is_active;
			if (child->get_property ("active", is_active)) {
				set_fade_out_active (is_active);
			}

		} else if ( (child->name() == "InverseFadeIn") || (child->name() == "InvFadeIn")  ) {
			XMLNode* grandchild = child->child ("AutomationList");
			if (grandchild) {
				_inverse_fade_in->set_state (*grandchild, version);
			}
		} else if ( (child->name() == "InverseFadeOut") || (child->name() == "InvFadeOut") ) {
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
AudioRegion::fade_range (samplepos_t start, samplepos_t end)
{
	samplepos_t s, e;

	switch (coverage (timepos_t (start), timepos_t (end))) {
	case Temporal::OverlapStart:
		trim_front (timepos_t (start));
		s = position().samples();
		e = end;
		set_fade_in (FadeConstantPower, e - s);
		break;
	case Temporal::OverlapEnd:
		trim_end(timepos_t (end));
		s = start;
		e = (position() + timepos_t (_length)).samples();
		set_fade_out (FadeConstantPower, e - s);
		break;
	case Temporal::OverlapInternal:
		/* needs addressing, perhaps. Difficult to do if we can't
		 * control one edge of the fade relative to the relevant edge
		 * of the region, which we cannot - fades are currently assumed
		 * to start/end at the start/end of the region
		 */
		break;
	default:
		return;
	}
}

void
AudioRegion::set_fade_in_shape (FadeShape shape)
{
	set_fade_in (shape, _fade_in->when(false).samples());
}

void
AudioRegion::set_fade_out_shape (FadeShape shape)
{
	set_fade_out (shape, _fade_out->when(false).samples());
}

void
AudioRegion::set_fade_in (std::shared_ptr<AutomationList> f)
{
	_fade_in->freeze ();
	*(_fade_in.val()) = *f;
	_fade_in->thaw ();
	_default_fade_in = false;

	send_change (PropertyChange (Properties::fade_in));
}

void
AudioRegion::set_fade_in (FadeShape shape, samplecnt_t len)
{
	const ARDOUR::ParameterDescriptor desc(FadeInAutomation);
	std::shared_ptr<Evoral::ControlList> c1 (new Evoral::ControlList (FadeInAutomation, desc, Temporal::TimeDomainProvider (Temporal::AudioTime)));
	std::shared_ptr<Evoral::ControlList> c2 (new Evoral::ControlList (FadeInAutomation, desc, Temporal::TimeDomainProvider (Temporal::AudioTime)));
	std::shared_ptr<Evoral::ControlList> c3 (new Evoral::ControlList (FadeInAutomation, desc, Temporal::TimeDomainProvider (Temporal::AudioTime)));

	_fade_in->freeze ();
	_fade_in->clear ();
	_inverse_fade_in->clear ();

	const int num_steps = 32;

	switch (shape) {
	case FadeLinear:
		_fade_in->fast_simple_add (timepos_t (Temporal::AudioTime), GAIN_COEFF_SMALL);
		_fade_in->fast_simple_add (timepos_t ((samplepos_t)len), GAIN_COEFF_UNITY);
		reverse_curve (_inverse_fade_in.val(), _fade_in.val());
		break;

	case FadeFast:
		generate_db_fade (_fade_in.val(), len, num_steps, -60);
		reverse_curve (c1, _fade_in.val());
		_fade_in->copy_events (*c1);
		generate_inverse_power_curve (_inverse_fade_in.val(), _fade_in.val());
		break;

	case FadeSlow:
		generate_db_fade (c1, len, num_steps, -1);  // start off with a slow fade
		generate_db_fade (c2, len, num_steps, -80); // end with a fast fade
		merge_curves (_fade_in.val(), c1, c2);
		reverse_curve (c3, _fade_in.val());
		_fade_in->copy_events (*c3);
		generate_inverse_power_curve (_inverse_fade_in.val(), _fade_in.val());
		break;

	case FadeConstantPower:
		_fade_in->fast_simple_add (timepos_t (Temporal::AudioTime), GAIN_COEFF_SMALL);
		for (int i = 1; i < num_steps; ++i) {
			const float dist = i / (num_steps + 1.f);
			_fade_in->fast_simple_add (timepos_t ((samplepos_t)(len * dist)), sin (dist * M_PI / 2.0));
		}
		_fade_in->fast_simple_add (timepos_t ((samplepos_t)len), GAIN_COEFF_UNITY);
		reverse_curve (_inverse_fade_in.val(), _fade_in.val());
		break;

	case FadeSymmetric:
		//start with a nearly linear cuve
		_fade_in->fast_simple_add (timepos_t (Temporal::AudioTime), 1);
		_fade_in->fast_simple_add (timepos_t ((samplepos_t)(0.5 * len)), 0.6);
		//now generate a fade-out curve by successively applying a gain drop
		const double breakpoint = 0.7;  //linear for first 70%
		for (int i = 2; i < 9; ++i) {
			const float coeff = (1.f - breakpoint) * powf (0.5, i);
			_fade_in->fast_simple_add (timepos_t ((samplepos_t)(len * (breakpoint + ((GAIN_COEFF_UNITY - breakpoint) * (double)i / 9.0)))), coeff);
		}
		_fade_in->fast_simple_add (timepos_t ((samplepos_t)len), GAIN_COEFF_SMALL);
		reverse_curve (c3, _fade_in.val());
		_fade_in->copy_events (*c3);
		reverse_curve (_inverse_fade_in.val(), _fade_in.val());
		break;
	}

	_fade_in->set_interpolation(Evoral::ControlList::Curved);
	_inverse_fade_in->set_interpolation(Evoral::ControlList::Curved);

	_default_fade_in = false;
	_fade_in->thaw ();
	send_change (PropertyChange (Properties::fade_in));
}

void
AudioRegion::set_fade_out (std::shared_ptr<AutomationList> f)
{
	_fade_out->freeze ();
	*(_fade_out.val()) = *f;
	_fade_out->thaw ();
	_default_fade_out = false;

	send_change (PropertyChange (Properties::fade_out));
}

void
AudioRegion::set_fade_out (FadeShape shape, samplecnt_t len)
{
	const ARDOUR::ParameterDescriptor desc(FadeOutAutomation);
	std::shared_ptr<Evoral::ControlList> c1 (new Evoral::ControlList (FadeOutAutomation, desc, Temporal::TimeDomainProvider (Temporal::AudioTime)));
	std::shared_ptr<Evoral::ControlList> c2 (new Evoral::ControlList (FadeOutAutomation, desc, Temporal::TimeDomainProvider (Temporal::AudioTime)));

	_fade_out->freeze ();
	_fade_out->clear ();
	_inverse_fade_out->clear ();

	const int num_steps = 32;

	switch (shape) {
	case FadeLinear:
		_fade_out->fast_simple_add (timepos_t (Temporal::AudioTime), GAIN_COEFF_UNITY);
		_fade_out->fast_simple_add (timepos_t ((samplepos_t)len), GAIN_COEFF_SMALL);
		reverse_curve (_inverse_fade_out.val(), _fade_out.val());
		break;

	case FadeFast:
		generate_db_fade (_fade_out.val(), len, num_steps, -60);
		generate_inverse_power_curve (_inverse_fade_out.val(), _fade_out.val());
		break;

	case FadeSlow:
		generate_db_fade (c1, len, num_steps, -1);  //start off with a slow fade
		generate_db_fade (c2, len, num_steps, -80);  //end with a fast fade
		merge_curves (_fade_out.val(), c1, c2);
		generate_inverse_power_curve (_inverse_fade_out.val(), _fade_out.val());
		break;

	case FadeConstantPower:
		//constant-power fades use a sin/cos relationship
		//the cutoff is abrupt but it has the benefit of being symmetrical
		_fade_out->fast_simple_add (timepos_t (Temporal::AudioTime), GAIN_COEFF_UNITY);
		for (int i = 1; i < num_steps; ++i) {
			const float dist = i / (num_steps + 1.f);
			_fade_out->fast_simple_add (timepos_t ((samplepos_t)(len * dist)), cos (dist * M_PI / 2.0));
		}
		_fade_out->fast_simple_add (timepos_t (len), GAIN_COEFF_SMALL);
		reverse_curve (_inverse_fade_out.val(), _fade_out.val());
		break;

	case FadeSymmetric:
		//start with a nearly linear cuve
		_fade_out->fast_simple_add (timepos_t (Temporal::AudioTime), 1);
		_fade_out->fast_simple_add (timepos_t ((samplepos_t)(0.5 * len)), 0.6);
		//now generate a fade-out curve by successively applying a gain drop
		const double breakpoint = 0.7;  //linear for first 70%
		for (int i = 2; i < 9; ++i) {
			const float coeff = (1.f - breakpoint) * powf (0.5, i);
			_fade_out->fast_simple_add (timepos_t ((samplepos_t)(len * (breakpoint + ((GAIN_COEFF_UNITY - breakpoint) * (double)i / 9.0)))), coeff);
		}
		_fade_out->fast_simple_add (timepos_t ((samplepos_t)len), GAIN_COEFF_SMALL);
		reverse_curve (_inverse_fade_out.val(), _fade_out.val());
		break;
	}

	_fade_out->set_interpolation(Evoral::ControlList::Curved);
	_inverse_fade_out->set_interpolation(Evoral::ControlList::Curved);

	_default_fade_out = false;
	_fade_out->thaw ();
	send_change (PropertyChange (Properties::fade_out));
}

void
AudioRegion::set_fade_in_length (samplecnt_t len)
{
	if (len > length_samples()) {
		len = length_samples() - 1;
	}

	if (len < 64) {
		len = 64;
	}

	timepos_t const tlen = timepos_t ((samplepos_t)len);

	bool changed = _fade_in->extend_to (tlen);

	if (changed) {
		if (_inverse_fade_in) {
			_inverse_fade_in->extend_to (tlen);
		}

		_default_fade_in = false;
		send_change (PropertyChange (Properties::fade_in));
	}
}

void
AudioRegion::set_fade_out_length (samplecnt_t len)
{
	if (len > length_samples()) {
		len = length_samples() - 1;
	}

	if (len < 64) {
		len = 64;
	}

	timepos_t const tlen = timepos_t ((samplepos_t)len);

	bool changed = _fade_out->extend_to (tlen);

	if (changed) {

		if (_inverse_fade_out) {
			_inverse_fade_out->extend_to (tlen);
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
	return _fade_in->size() == 2 && _fade_in->when(true) == 0 && _fade_in->when(false).samples () == 64;
}

bool
AudioRegion::fade_out_is_default () const
{
	return _fade_out->size() == 2 && _fade_out->when(true) == 0 && _fade_out->when(false).samples () == 64;
}

samplecnt_t
AudioRegion::fade_in_length ()
{
	samplecnt_t fade_in_length = (samplecnt_t) _fade_in->when(false);
	return fade_in_length;
}

samplecnt_t
AudioRegion::fade_out_length ()
{
	samplecnt_t fade_out_length = (samplecnt_t) _fade_out->when(false);
	return fade_out_length;
}

void
AudioRegion::set_default_fade_in ()
{
	_fade_in_suspended = 0;
	set_fade_in (Config->get_default_fade_shape(), 64);
}

void
AudioRegion::set_default_fade_out ()
{
	_fade_out_suspended = 0;
	set_fade_out (Config->get_default_fade_shape(), 64);
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
	_envelope->fast_simple_add (timepos_t (Temporal::AudioTime), GAIN_COEFF_UNITY);

	/* Force length into audio time domain. If we don't do this, the
	 * envelope (which uses the AudioTime domain) will have problems when
	 * we call its fast_simple_add() mechanism and it discovers that the
	 * time is not AudioTime.
	 *
	 * XXX this needs some thought
	 */

	_envelope->fast_simple_add (len_as_tpos (), GAIN_COEFF_UNITY);
	_envelope->thaw ();
}

void
AudioRegion::recompute_at_end ()
{
	/* our length has changed. recompute a new final point by interpolating
	   based on the the existing curve.
	*/

	timepos_t tend (len_as_tpos ());

	_envelope->freeze ();
	_envelope->truncate_end (tend);
	_envelope->thaw ();

	foreach_plugin ([tend](std::weak_ptr<RegionFxPlugin> wfx)
	{
		shared_ptr<RegionFxPlugin> rfx = wfx.lock ();
		if (rfx) {
			rfx->truncate_automation_end (tend);
		}
	});

	suspend_property_changes();

	if (_left_of_split) {
		set_default_fade_out ();
		_left_of_split = false;
	} else if (_fade_out->when(false) > _length) {
		_fade_out->extend_to (len_as_tpos ());
		send_change (PropertyChange (Properties::fade_out));
	}

	if (_fade_in->when(false) > _length) {
		_fade_in->extend_to (len_as_tpos ());
		send_change (PropertyChange (Properties::fade_in));
	}

	resume_property_changes();
}

void
AudioRegion::recompute_at_start ()
{
	/* as above, but the shift was from the front */

	timecnt_t tas (timecnt_t::from_samples (length().samples ()));
	_envelope->truncate_start (tas);

	foreach_plugin ([tas](std::weak_ptr<RegionFxPlugin> wfx)
	{
		shared_ptr<RegionFxPlugin> rfx = wfx.lock ();
		if (rfx) {
			rfx->truncate_automation_start (tas);
		}
	});

	suspend_property_changes();

	if (_right_of_split) {
		set_default_fade_in ();
		_right_of_split = false;
	} else if (_fade_in->when(false) > len_as_tpos ()) {
		_fade_in->extend_to (len_as_tpos ());
		send_change (PropertyChange (Properties::fade_in));
	}

	if (_fade_out->when(false) > len_as_tpos ()) {
		_fade_out->extend_to (len_as_tpos ());
		send_change (PropertyChange (Properties::fade_out));
	}

	resume_property_changes();
}

int
AudioRegion::separate_by_channel (vector<std::shared_ptr<Region> >& v) const
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

		PropertyList plist (properties ());

		plist.add (Properties::name, new_name);
		plist.add (Properties::whole_file, true);

		v.push_back(RegionFactory::create (srcs, plist));

		++n;
	}

	return 0;
}

samplecnt_t
AudioRegion::read_raw_internal (Sample* buf, samplepos_t pos, samplecnt_t cnt, int channel) const
{
	return audio_source(channel)->read (buf, pos, cnt);
}

void
AudioRegion::set_scale_amplitude (gain_t g)
{
	std::shared_ptr<Playlist> pl (playlist());

	_scale_amplitude = g;

	send_change (PropertyChange (Properties::scale_amplitude));
}

double
AudioRegion::maximum_amplitude (Progress* p) const
{
	samplepos_t fpos = start_sample();;
	samplepos_t const fend = start_sample() + length_samples();
	double maxamp = 0;

	samplecnt_t const blocksize = 64 * 1024;
	Sample buf[blocksize];

	while (fpos < fend) {

		uint32_t n;

		samplecnt_t const to_read = min (fend - fpos, blocksize);

		for (n = 0; n < n_channels(); ++n) {

			/* read it in */

			if (read_raw_internal (buf, fpos, to_read, n) != to_read) {
#ifndef NDEBUG
				cerr << "AudioRegion::maximum_amplitude read failed for '" << _name << "'\n";
#endif
				return 0;
			}

			maxamp = compute_peak (buf, to_read, maxamp);
		}

		fpos += to_read;
		if (p) {
			p->set_progress (float (fpos - start_sample()) / length_samples());
			if (p->cancelled ()) {
				return -1;
			}
		}
	}

	return maxamp;
}

double
AudioRegion::rms (Progress* p) const
{
	samplepos_t fpos = start_sample();
	samplepos_t const fend = start_sample() + length_samples();
	uint32_t const n_chan = n_channels ();
	double rms = 0;

	samplecnt_t const blocksize = 64 * 1024;
	Sample buf[blocksize];

	samplecnt_t total = 0;

	if (n_chan == 0 || fend == fpos) {
		return 0;
	}

	while (fpos < fend) {
		samplecnt_t const to_read = min (fend - fpos, blocksize);
		for (uint32_t c = 0; c < n_chan; ++c) {
			if (read_raw_internal (buf, fpos, to_read, c) != to_read) {
				return 0;
			}
			for (samplepos_t i = 0; i < to_read; ++i) {
				rms += buf[i] * buf[i];
			}
		}
		total += to_read;
		fpos += to_read;
		if (p) {
			p->set_progress (float (fpos - start_sample()) / length_samples());
			if (p->cancelled ()) {
				return -1;
			}
		}
	}
	return sqrt (2. * rms / (double)(total * n_chan));
}

bool
AudioRegion::loudness (float& tp, float& i, float& s, float& m, Progress* p) const
{
	ARDOUR::AnalysisGraph ag (&_session);
	tp = i = s = m = -200;

	ag.set_total_samples (length_samples());
	ag.analyze_region (this, true, p);

	if (p && p->cancelled ()) {
		return false;
	}

	AnalysisResults const& ar (ag.results ());
	if (ar.size() != 1) {
		return false;
	}
	ExportAnalysisPtr eap (ar.begin ()->second);

	if (eap->have_dbtp) {
		tp = eap->truepeak;
	}
	if (eap->have_loudness) {
		i = eap->integrated_loudness;
		s = eap->max_loudness_short;
		m = eap->max_loudness_momentary;
	}

	return eap->have_dbtp || eap->have_loudness;
}

/** Normalize using a given maximum amplitude and target, so that region
 *  _scale_amplitude becomes target / max_amplitude.
 */
void
AudioRegion::normalize (float max_amplitude, float target_dB)
{
	gain_t target = dB_to_coefficient (target_dB);

	if (target == GAIN_COEFF_UNITY) {
		/* do not normalize to precisely 1.0 (0 dBFS), to avoid making it appear
		   that we may have clipped.
		*/
		target -= FLT_EPSILON;
	}

	if (max_amplitude < GAIN_COEFF_SMALL) {
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
	   because regions are not being deleted when a session
	   is unloaded. That bug must be fixed.
	*/

	if (_sources.empty()) {
		return;
	}

	std::shared_ptr<AudioFileSource> afs = std::dynamic_pointer_cast<AudioFileSource>(_sources.front());
}

std::shared_ptr<AudioSource>
AudioRegion::audio_source (uint32_t n) const
{
	// Guaranteed to succeed (use a static cast for speed?)
	return std::dynamic_pointer_cast<AudioSource>(source(n));
}

void
AudioRegion::clear_transients () // yet unused
{
	_user_transients.clear ();
	_valid_transients = false;
	send_change (PropertyChange (Properties::valid_transients));
}

void
AudioRegion::add_transient (samplepos_t where)
{
	if (where < first_sample () || where >= last_sample ()) {
		return;
	}
	where -= position_sample();

	if (!_valid_transients) {
		_transient_user_start = start_sample();
		_valid_transients = true;
	}
	sampleoffset_t offset = _transient_user_start - start_sample();;

	if (where < offset) {
		if (offset <= 0) {
			return;
		}
		// region start changed (extend to front), shift points and offset
		for (AnalysisFeatureList::iterator x = _transients.begin(); x != _transients.end(); ++x) {
			(*x) += offset;
		}
		_transient_user_start -= offset;
		offset = 0;
	}

	const samplepos_t p = where - offset;
	_user_transients.push_back(p);
	send_change (PropertyChange (Properties::valid_transients));
}

void
AudioRegion::update_transient (samplepos_t old_position, samplepos_t new_position)
{
	bool changed = false;
	if (!_onsets.empty ()) {
		const samplepos_t p = old_position - position_sample();
		AnalysisFeatureList::iterator x = std::find (_onsets.begin (), _onsets.end (), p);
		if (x != _transients.end ()) {
			(*x) = new_position - position_sample();
			changed = true;
		}
	}

	if (_valid_transients) {
		const sampleoffset_t offset = position_sample() + _transient_user_start - start_sample();
		const samplepos_t p = old_position - offset;
		AnalysisFeatureList::iterator x = std::find (_user_transients.begin (), _user_transients.end (), p);
		if (x != _transients.end ()) {
			(*x) = new_position - offset;
			changed = true;
		}
	}

	if (changed) {
		send_change (PropertyChange (Properties::valid_transients));
	}
}

void
AudioRegion::remove_transient (samplepos_t where)
{
	bool changed = false;
	if (!_onsets.empty ()) {
		const samplepos_t p = where - position_sample();
		AnalysisFeatureList::iterator i = std::find (_onsets.begin (), _onsets.end (), p);
		if (i != _onsets.end ()) {
			_onsets.erase (i);
			changed = true;
		}
	}

	if (_valid_transients) {
		const samplepos_t p = where - (position_sample() + _transient_user_start - start_sample());
		AnalysisFeatureList::iterator i = std::find (_user_transients.begin (), _user_transients.end (), p);
		if (i != _user_transients.end ()) {
			_user_transients.erase (i);
			changed = true;
		}
	}

	if (changed) {
		send_change (PropertyChange (Properties::valid_transients));
	}
}

void
AudioRegion::set_onsets (AnalysisFeatureList& results)
{
	_onsets.clear();
	_onsets = results;
	send_change (PropertyChange (Properties::valid_transients));
}

void
AudioRegion::build_transients ()
{
	_transients.clear ();
	_transient_analysis_start = _transient_analysis_end = 0;

	std::shared_ptr<Playlist> pl = playlist();

	if (!pl) {
		return;
	}

	/* check analyzed sources first */
	SourceList::iterator s;
	for (s = _sources.begin() ; s != _sources.end(); ++s) {
		if (!(*s)->has_been_analysed()) {
#ifndef NDEBUG
			cerr << "For " << name() << " source " << (*s)->name() << " has not been analyzed\n";
#endif
			break;
		}
	}

	if (s == _sources.end()) {
		/* all sources are analyzed, merge data from each one */
		for (s = _sources.begin() ; s != _sources.end(); ++s) {

			/* find the set of transients within the bounds of this region */
			AnalysisFeatureList::iterator low = lower_bound ((*s)->transients.begin(),
									 (*s)->transients.end(),
			                                                 start_sample());

			AnalysisFeatureList::iterator high = upper_bound ((*s)->transients.begin(),
									  (*s)->transients.end(),
			                                                  start_sample() + length_samples());

			/* and add them */
			_transients.insert (_transients.end(), low, high);
		}

		TransientDetector::cleanup_transients (_transients, pl->session().sample_rate(), 3.0);

		/* translate all transients to current position */
		for (AnalysisFeatureList::iterator x = _transients.begin(); x != _transients.end(); ++x) {
			(*x) -= start_sample();
		}

		_transient_analysis_start = start_sample();
		_transient_analysis_end = start_sample() + length_samples();
		return;
	}

	/* no existing/complete transient info */

	static bool analyse_dialog_shown = false; /* global per instance of Ardour */

	if (!Config->get_auto_analyse_audio()) {
		if (!analyse_dialog_shown) {
			pl->session().Dialog (string_compose (_("\
You have requested an operation that requires audio analysis.\n\n\
You currently have \"auto-analyse-audio\" disabled, which means \
that transient data must be generated every time it is required.\n\n\
If you are doing work that will require transient data on a \
regular basis, you should probably enable \"auto-analyse-audio\" \
in Preferences > Metering, then quit %1 and restart.\n\n\
This dialog will not display again.  But you may notice a slight delay \
in this and future transient-detection operations.\n\
"), PROGRAM_NAME));
			analyse_dialog_shown = true;
		}
	}

	try {
		TransientDetector t (pl->session().sample_rate());
		for (uint32_t i = 0; i < n_channels(); ++i) {

			AnalysisFeatureList these_results;

			t.reset ();

			/* this produces analysis result relative to current position
			 * ::read() sample 0 is at _position */
			if (t.run ("", this, i, these_results)) {
				return;
			}

			/* merge */
			_transients.insert (_transients.end(), these_results.begin(), these_results.end());
		}
	} catch (...) {
		error << string_compose(_("Transient Analysis failed for %1."), _("Audio Region")) << endmsg;
		return;
	}

	TransientDetector::cleanup_transients (_transients, pl->session().sample_rate(), 3.0);
	_transient_analysis_start = start_sample();
	_transient_analysis_end = start_sample() + length_samples();
}

/* Transient analysis uses ::read() which is relative to _start,
 * at the time of analysis and spans _length samples.
 *
 * This is true for RhythmFerret::run_analysis and the
 * TransientDetector here.
 *
 * We store _start and length in _transient_analysis_start,
 * _transient_analysis_end in case the region is trimmed or split after analysis.
 *
 * Various methods (most notably Playlist::find_next_transient and
 * RhythmFerret::do_split_action) span multiple regions and *merge/combine*
 * Analysis results.
 * We therefore need to translate the analysis timestamps to absolute session-time
 * and include the _position of the region.
 *
 * Note: we should special case the AudioRegionView. The region-view itself
 * is located at _position (currently ARV subtracts _position again)
 */
void
AudioRegion::get_transients (AnalysisFeatureList& results)
{
	std::shared_ptr<Playlist> pl = playlist();
	if (!playlist ()) {
		return;
	}

	Region::merge_features (results, _user_transients, position_sample() + _transient_user_start - start_sample());

	if (!_onsets.empty ()) {
		// onsets are invalidated when start or length changes
		merge_features (results, _onsets, position_sample());
		return;
	}

	if ((_transient_analysis_start == _transient_analysis_end)
	    || _transient_analysis_start > start_sample()
	    || _transient_analysis_end < start_sample() + length_samples()) {
		build_transients ();
	}

	merge_features (results, _transients, position_sample() + _transient_analysis_start - start_sample());
}

/** Find areas of `silence' within a region.
 *
 *  @param threshold Threshold below which signal is considered silence (as a sample value)
 *  @param min_length Minimum length of silent period to be reported.
 *  @return Silent intervals, measured relative to the region start in the source
 */

AudioIntervalResult
AudioRegion::find_silence (Sample threshold, samplecnt_t min_length, samplecnt_t fade_length, InterThreadInfo& itt) const
{
	samplecnt_t const block_size = 64 * 1024;
	boost::scoped_array<Sample> loudest (new Sample[block_size]);
	boost::scoped_array<Sample> buf (new Sample[block_size]);

	assert (fade_length >= 0);
	assert (min_length > 0);

	samplepos_t pos = start_sample();
	samplepos_t const end = start_sample() + length_samples();

	AudioIntervalResult silent_periods;

	bool in_silence = true;
	sampleoffset_t silence_start = start_sample();

	while (pos < end && !itt.cancel) {

		samplecnt_t cur_samples = 0;
		samplecnt_t const to_read = min (end - pos, block_size);
		/* fill `loudest' with the loudest absolute sample at each instant, across all channels */
		memset (loudest.get(), 0, sizeof (Sample) * block_size);

		for (uint32_t n = 0; n < n_channels(); ++n) {

			cur_samples = read_raw_internal (buf.get(), pos, to_read, n);
			for (samplecnt_t i = 0; i < cur_samples; ++i) {
				loudest[i] = max (loudest[i], abs (buf[i]));
			}
		}

		/* now look for silence */
		for (samplecnt_t i = 0; i < cur_samples; ++i) {
			bool const silence = abs (loudest[i]) < threshold;
			if (silence && !in_silence) {
				/* non-silence to silence */
				in_silence = true;
				silence_start = pos + i + fade_length;
			} else if (!silence && in_silence) {
				/* silence to non-silence */
				in_silence = false;
				sampleoffset_t silence_end = pos + i - 1 - fade_length;

				if (silence_end - silence_start >= min_length) {
					silent_periods.push_back (std::make_pair (silence_start, silence_end));
				}
			}
		}

		pos += cur_samples;
		itt.progress = (end - pos) / (double) length_samples();

		if (cur_samples == 0) {
			assert (pos >= end);
			break;
		}
	}

	if (in_silence && !itt.cancel) {
		/* last block was silent, so finish off the last period */
		if (end - 1 - silence_start >= min_length + fade_length) {
			silent_periods.push_back (std::make_pair (silence_start, end - 1));
		}
	}

	itt.done = true;

	return silent_periods;
}

Temporal::Range
AudioRegion::body_range () const
{
	return Temporal::Range ((position() + _fade_in->back()->when).increment(), end().earlier (_fade_out->back()->when));
}

std::shared_ptr<Region>
AudioRegion::get_single_other_xfade_region (bool start) const
{
	std::shared_ptr<Playlist> pl (playlist());

	if (!pl) {
		/* not currently in a playlist - xfade length is unbounded
		   (and irrelevant)
		*/
		return std::shared_ptr<AudioRegion> ();
	}

	std::shared_ptr<RegionList> rl;

	if (start) {
		rl = pl->regions_at (position());
	} else {
		rl = pl->regions_at (nt_last());
	}

	RegionList::iterator i;
	std::shared_ptr<Region> other;
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
		return std::shared_ptr<AudioRegion> ();
	}

	return other;
}

samplecnt_t
AudioRegion::verify_xfade_bounds (samplecnt_t len, bool start)
{
	/* this is called from a UI to check on whether a new proposed
	   length for an xfade is legal or not. it returns the legal
	   length corresponding to @a len which may be shorter than or
	   equal to @a len itself.
	*/

	std::shared_ptr<Region> other = get_single_other_xfade_region (start);
	samplecnt_t maxlen;

	if (!other) {
		/* zero or > 2 regions here, don't care about len, but
		   it can't be longer than the region itself.
		 */
		return min (length_samples(), len);
	}

	/* we overlap a single region. clamp the length of an xfade to
	   the maximum possible duration of the overlap (if the other
	   region were trimmed appropriately).
	*/

	if (start) {
		maxlen = other->latest_possible_sample() - position_sample();
	} else {
		maxlen = last_sample() - other->earliest_possible_position().samples();
	}

	return min (length_samples(), min (maxlen, len));

}

bool
AudioRegion::do_export (std::string const& path) const
{
	const uint32_t    n_chn      = n_channels ();
	const samplecnt_t chunk_size = 8192;
	Sample            buf[chunk_size];

	const int format = SF_FORMAT_FLAC | SF_FORMAT_PCM_24; // TODO preference or option

	assert (!path.empty ());
	assert (!Glib::file_test (path, Glib::FILE_TEST_EXISTS));

	typedef std::shared_ptr<AudioGrapher::SndfileWriter<Sample>> FloatWriterPtr;
	FloatWriterPtr                                                 sfw;
	try {
		sfw.reset (new AudioGrapher::SndfileWriter<Sample> (path, format, n_chn, audio_source ()->sample_rate (), 0));
	} catch (...) {
		return false;
	}

	AudioGrapher::Interleaver<Sample> interleaver;
	interleaver.init (n_channels (), chunk_size);
	interleaver.add_output (sfw);

	samplecnt_t to_read  = length_samples ();
	samplepos_t pos      = position_sample ();
	samplecnt_t lsamples = _length.val().samples();

	while (to_read) {
		samplecnt_t this_time = min (to_read, chunk_size);

		for (uint32_t chn = 0; chn < n_chn; ++chn) {
			if (read_from_sources (_sources, lsamples, buf, pos, this_time, chn) != this_time) {
				goto errout;
			}

			AudioGrapher::ConstProcessContext<Sample> context (buf, this_time, 1);
			if (to_read == this_time) {
				context ().set_flag (AudioGrapher::ProcessContext<Sample>::EndOfInput);
			}
			interleaver.input (chn)->process (context);
		}

		to_read -= this_time;
		pos += this_time;
	}

errout:
	/* Drop references, close file */
	interleaver.clear_outputs ();
	sfw.reset ();

	if (to_read != 0) {
		::g_unlink (path.c_str());
	}

	return to_read == 0;
}

bool
AudioRegion::_add_plugin (std::shared_ptr<RegionFxPlugin> rfx, std::shared_ptr<RegionFxPlugin> before, bool from_set_state)
{
	ChanCount in (DataType::AUDIO, n_channels ());
	ChanCount out (in);

	if (!rfx->can_support_io_configuration (in, out)) {
		return false;
	}
	if (in.n_audio () > out.n_audio ()) {
		return false;
	}
	if (!rfx->configure_io (in, out)) {
		return false;
	}

	ChanCount fx_cc;
	{
		Glib::Threads::RWLock::ReaderLock lm (_fx_lock, Glib::Threads::NOT_LOCK);
		if (!from_set_state) {
			lm.acquire();
		}
		ChanCount cc (DataType::AUDIO, n_channels ());
		fx_cc = ChanCount::max (in, out);
		fx_cc = ChanCount::max (fx_cc, rfx->required_buffers ());
		for (auto const& i : _plugins) {
			fx_cc = ChanCount::max (fx_cc, i->required_buffers ());
		}
	}

	DEBUG_TRACE (DEBUG::RegionFx, string_compose ("Audio Region Fx required ChanCount: %1\n", fx_cc));

	_session.ensure_buffers_unlocked (fx_cc);

	/* subscribe to parameter changes */
	ControllableSet acs;
	rfx->automatables (acs);
	for (auto& ec : acs) {
		std::shared_ptr<AutomationControl> ac (std::dynamic_pointer_cast<AutomationControl>(ec));
		std::weak_ptr<AutomationControl> wc (ac);
		ec->Changed.connect_same_thread (*this, [this, wc] (bool, PBD::Controllable::GroupControlDisposition)
				{
					std::shared_ptr<AutomationControl> ac = wc.lock ();
					if (ac && ac->automation_playback ()) {
						return;
					}
					if (!_invalidated.exchange (true)) {
						send_change (PropertyChange (Properties::region_fx)); // trigger DiskReader overwrite
					}
				});
		if (!ac->alist ()) {
			continue;
		}
		ac->alist()->StateChanged.connect_same_thread (*this, [this] ()
				{
					if (!_invalidated.exchange (true)) {
						send_change (PropertyChange (Properties::region_fx)); // trigger DiskReader overwrite
					}
				});
	}

	rfx->LatencyChanged.connect_same_thread (*this, boost::bind (&AudioRegion::fx_latency_changed, this, false));
	rfx->set_block_size (_session.get_block_size ());

	if (from_set_state) {
		return true;
	}

	{
		Glib::Threads::RWLock::WriterLock lm (_fx_lock);
		RegionFxList::iterator loc = _plugins.end ();
		if (before) {
			loc = find (_plugins.begin (), _plugins.end (), before);
		}
		_plugins.insert (loc, rfx);
	}

	rfx->set_default_automation (len_as_tpos ());

	fx_latency_changed (true);
	if (!_invalidated.exchange (true)) {
		send_change (PropertyChange (Properties::region_fx)); // trigger DiskReader overwrite
	}
	RegionFxChanged (); /* EMIT SIGNAL */
	return true;
}

bool
AudioRegion::remove_plugin (std::shared_ptr<RegionFxPlugin> fx)
{
	Glib::Threads::RWLock::WriterLock lm (_fx_lock);
	auto i = find (_plugins.begin(), _plugins.end(), fx);
	if (i == _plugins.end ()) {
		return false;
	}
	_plugins.erase (i);

	lm.release ();

	fx->drop_references ();
	fx_latency_changed (true);

	if (!_invalidated.exchange (true)) {
		send_change (PropertyChange (Properties::region_fx)); // trigger DiskReader overwrite
	}
	RegionFxChanged (); /* EMIT SIGNAL */
	return true;
}

void
AudioRegion::reorder_plugins (RegionFxList const& new_order)
{
	Region::reorder_plugins (new_order);
	if (!_invalidated.exchange (true)) {
		send_change (PropertyChange (Properties::region_fx)); // trigger DiskReader overwrite
	}
	RegionFxChanged (); /* EMIT SIGNAL */
}

void
AudioRegion::fx_latency_changed (bool no_emit)
{
	uint32_t l = 0;
	for (auto const& rfx : _plugins) {
		l += rfx->effective_latency ();
	}
	if (l == _fx_latency) {
		return;
	}
	_fx_latency = l;

	if (no_emit) {
		return;
	}

	if (!_invalidated.exchange (true)) {
		send_change (PropertyChange (Properties::region_fx)); // trigger DiskReader overwrite
	}
}

void
AudioRegion::apply_region_fx (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, samplecnt_t n_samples)
{
	Glib::Threads::RWLock::ReaderLock lm (_fx_lock);

	if (_plugins.empty ()) {
		return;
	}

	pframes_t block_size = _session.get_block_size ();
	if (_fx_block_size != block_size) {
		_fx_block_size = block_size;
		for (auto const& rfx : _plugins) {
			rfx->set_block_size (_session.get_block_size ());
		}
	}

	samplecnt_t latency_offset = 0;

	for (auto const& rfx : _plugins) {
		if (_fx_pos != start_sample) {
			rfx->flush ();
		}
		samplecnt_t remain = n_samples;
		samplecnt_t offset = 0;
		samplecnt_t latency = rfx->effective_latency ();

		while (remain > 0) {
			pframes_t run = std::min <pframes_t> (remain, block_size);
			if (!rfx->run (bufs, start_sample + offset - latency_offset, end_sample + offset - latency_offset, position().samples(), run, offset)) {
				lm.release ();
				/* this triggers a re-read */
				const_cast<AudioRegion*>(this)->remove_plugin (rfx);
				return;
			}
			remain -= run;
			offset += run;
		}

		if (_fx_latent_read && latency > 0) {
			for (uint32_t c = 0; c < n_channels (); ++c) {
				Sample* to   = _readcache.get_audio (c).data();
				Sample* from = _readcache.get_audio (c).data(latency);
				// XXX can left to right copy_vector() work here?
				memmove (to, from, (n_samples - latency) * sizeof(Sample));
			}
			n_samples -= latency;
		}
		if (!_fx_latent_read) {
			latency_offset += latency;
		}
	}
	_fx_pos = end_sample;
	_fx_latent_read = false;
}
