#include <algorithm>
#include <iostream>
#include <cstdlib>

#include <glibmm.h>

#include <rubberband/RubberBandStretcher.h>

#include "pbd/basename.h"
#include "pbd/compose.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"
#include "pbd/types_convert.h"

#include "temporal/tempo.h"

#include "ardour/audioengine.h"
#include "ardour/audioregion.h"
#include "ardour/audio_buffer.h"
#include "ardour/debug.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_model.h"
#include "ardour/midi_region.h"
#include "ardour/minibpm.h"
#include "ardour/port.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/session_object.h"
#include "ardour/sidechain.h"
#include "ardour/source_factory.h"
#include "ardour/sndfilesource.h"
#include "ardour/triggerbox.h"
#include "ardour/types_convert.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;
using std::string;
using std::cerr;
using std::endl;

namespace ARDOUR {
	namespace Properties {
		PBD::PropertyDescriptor<bool> use_follow;
		PBD::PropertyDescriptor<bool> running;
		PBD::PropertyDescriptor<bool> legato;
		PBD::PropertyDescriptor<bool> quantization;
		PBD::PropertyDescriptor<Trigger::LaunchStyle> launch_style;
		PBD::PropertyDescriptor<Trigger::FollowAction> follow_action0;
		PBD::PropertyDescriptor<Trigger::FollowAction> follow_action1;
	}
}

Trigger::Trigger (uint64_t n, TriggerBox& b)
	: _box (b)
	, _state (Stopped)
	, _requested_state (None)
	, _bang (0)
	, _unbang (0)
	, _index (n)
	, _launch_style (Toggle)
	, _use_follow (Properties::use_follow, true)
	, _follow_action { NextTrigger, Stop }
	, _follow_action_probability (100)
	, _loop_cnt (0)
	, _follow_count (1)
	, _quantization (Temporal::BBT_Offset (0, 1, 0))
	, _legato (Properties::legato, false)
	, _barcnt (0.)
	, _apparent_tempo (0.)
	, _gain (1.0)
	, _pending_gain (1.0)
	, _midi_velocity_effect (0.)
	, _ui (0)
{
	add_property (_legato);
	add_property (_use_follow);
}

void
Trigger::set_use_follow (bool yn)
{
	_use_follow = yn;
	PropertyChanged (Properties::use_follow);
}

void
Trigger::set_name (std::string const & str)
{
	_name = str;
}

void
Trigger::set_ui (void* p)
{
	_ui = p;
}

void
Trigger::bang ()
{
	if (!_region) {
		return;
	}
	_bang.fetch_add (1);
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("bang on %1\n", _index));
}

void
Trigger::unbang ()
{
	if (!_region) {
		return;
	}
	_unbang.fetch_add (1);
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("un-bang on %1\n", _index));
}

void
Trigger::set_gain (gain_t g)
{
	_pending_gain = g;
}

void
Trigger::set_midi_velocity_effect (float mve)
{
	_midi_velocity_effect = std::min (1.f, std::max (0.f, mve));
}

void
Trigger::set_follow_count (uint32_t n)
{
	_follow_count = n;
}

void
Trigger::set_follow_action (FollowAction f, uint64_t n)
{
	assert (n < 2);
	_follow_action[n] = f;
	if (n == 0) {
		PropertyChanged (Properties::follow_action0);
	} else {
		PropertyChanged (Properties::follow_action1);
	}
}

void
Trigger::set_launch_style (LaunchStyle l)
{
	_launch_style = l;

	set_usable_length ();
	PropertyChanged (Properties::launch_style);
}

XMLNode&
Trigger::get_state (void)
{
	XMLNode* node = new XMLNode (X_("Trigger"));

	for (OwnedPropertyList::iterator i = _properties->begin(); i != _properties->end(); ++i) {
		i->second->get_value (*node);
	}

	node->set_property (X_("launch-style"), enum_2_string (_launch_style));
	node->set_property (X_("follow-action-0"), enum_2_string (_follow_action[0]));
	node->set_property (X_("follow-action-1"), enum_2_string (_follow_action[1]));
	node->set_property (X_("quantization"), _quantization);
	node->set_property (X_("name"), _name);
	node->set_property (X_("index"), _index);
	node->set_property (X_("apparent-tempo"), _apparent_tempo);
	node->set_property (X_("barcnt"), _barcnt);

	if (_region) {
		node->set_property (X_("region"), _region->id());
	}

	return *node;
}

int
Trigger::set_state (const XMLNode& node, int version)
{
	PropertyChange what_changed;

	what_changed = set_values (node);

	node.get_property (X_("launch-style"), _launch_style);
	node.get_property (X_("follow-action-0"), _follow_action[0]);
	node.get_property (X_("follow-action-1"), _follow_action[1]);
	node.get_property (X_("quantization"), _quantization);
	node.get_property (X_("name"), _name);
	node.get_property (X_("index"), _index);

	PBD::ID rid;

	node.get_property (X_("region"), rid);

	boost::shared_ptr<Region> r = RegionFactory::region_by_id (rid);

	if (r) {
		set_region (r);
	}

	return 0;
}

void
Trigger::set_legato (bool yn)
{
	_legato = yn;
	PropertyChanged (Properties::legato);
}

void
Trigger::set_follow_action_probability (int n)
{
	n = std::min (100, n);
	n = std::max (0, n);

	_follow_action_probability = n;
}

void
Trigger::set_quantization (Temporal::BBT_Offset const & q)
{
	_quantization = q;
	set_usable_length ();
	PropertyChanged (Properties::quantization);
}

void
Trigger::set_region (boost::shared_ptr<Region> r)
{
	TriggerBox::worker->set_region (this, r);
}

void
Trigger::set_region_internal (boost::shared_ptr<Region> r)
{
	_region = r;
}

Temporal::BBT_Offset
Trigger::quantization () const
{
	return _quantization;
}

void
Trigger::stop (int next)
{
	request_state (Stopped);
}

void
Trigger::request_state (State s)
{
	_requested_state.store (s);
}

void
Trigger::startup()
{
	_state = WaitingToStart;
	_gain = _pending_gain;
	_loop_cnt = 0;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 starts up\n", name()));
	PropertyChanged (ARDOUR::Properties::running);
}

void
Trigger::shutdown ()
{
	_state = Stopped;
	_gain = 1.0;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 shuts down\n", name()));
	PropertyChanged (ARDOUR::Properties::running);
}

void
Trigger::jump_start()
{
	/* this is used when we start a new trigger in legato mode. We do not
	   wait for quantization.
	*/
	_state = Running;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 requested state %2\n", index(), enum_2_string (_state)));
	PropertyChanged (ARDOUR::Properties::running);
}

void
Trigger::jump_stop()
{
	/* this is used when we start a new trigger in legato mode. We do not
	   wait for quantization.
	*/
	shutdown ();
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 requested state %2\n", index(), enum_2_string (_state)));
	PropertyChanged (ARDOUR::Properties::running);
}

void
Trigger::start_stop()
{
	/* this is used when we start a tell a currently playing trigger to
	   stop, but wait for quantization first.
	*/
	_state = WaitingToStop;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 requested state %2\n", index(), enum_2_string (_state)));
	PropertyChanged (ARDOUR::Properties::running);
}

void
Trigger::process_state_requests ()
{
	State new_state = _requested_state.exchange (None);

	if (new_state != None && new_state != _state) {

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 requested state %2\n", index(), enum_2_string (new_state)));

		switch (new_state) {
		case Stopped:
			if (_state != WaitingToStop) {
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 => %3\n", index(), enum_2_string (_state), enum_2_string (WaitingToStop)));
				_state = WaitingToStop;
				PropertyChanged (ARDOUR::Properties::running);
			}
			break;
		case Running:
			_box.queue_explict (this);
			break;
		default:
			break;
		}
	}

	/* now check bangs/unbangs */

	int x;

	while ((x = _bang.load ())) {

		_bang.fetch_sub (1);

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 handling bang with state = %2\n", index(), enum_2_string (_state)));

		switch (_state) {
		case None:
			abort ();
			break;

		case Running:
			switch (launch_style()) {
			case OneShot:
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 oneshot %2 => %3\n", index(), enum_2_string (Running), enum_2_string (WaitingForRetrigger)));
				_state = WaitingForRetrigger;
				PropertyChanged (ARDOUR::Properties::running);
				break;
			case Gate:
			case Toggle:
			case Repeat:
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 gate/toggle/repeat => %3\n", index(), enum_2_string (Running), enum_2_string (WaitingToStop)));
				_state = WaitingToStop;
				_box.clear_implicit ();
				PropertyChanged (ARDOUR::Properties::running);
			}
			break;

		case Stopped:
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 stopped => %3\n", index(), enum_2_string (Stopped), enum_2_string (WaitingToStart)));
			_box.queue_explict (this);
			break;

		case WaitingToStart:
		case WaitingToStop:
		case WaitingForRetrigger:
		case Stopping:
			break;
		}
	}

	while ((x = _unbang.load ())) {

		_unbang.fetch_sub (1);

		if (_launch_style == Gate || _launch_style == Repeat) {
			switch (_state) {
			case Running:
				_state = WaitingToStop;
				PropertyChanged (ARDOUR::Properties::running);
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 unbanged, now in WaitingToStop\n", index()));
				break;
			default:
				/* didn't even get started */
				shutdown ();
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 unbanged, never started, now stopped\n", index()));
			}
		}
	}
}

Trigger::RunType
Trigger::maybe_compute_next_transition (Temporal::Beats const & start, Temporal::Beats const & end)
{
	using namespace Temporal;

	/* In these states, we are not waiting for a transition */

	switch (_state) {
	case Stopped:
		return RunNone;
	case Running:
		return RunAll;
	case Stopping:
		return RunAll;
	default:
		break;
	}

	timepos_t transition_time (BeatTime);

	/* XXX need to use global grid here is quantization == zero */

	if (_quantization.bars == 0) {
		transition_time = timepos_t (start.snap_to (Temporal::Beats (_quantization.beats, _quantization.ticks)));
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 quantized with %5 start at %2, sb %3 eb %4\n", index(), transition_time.beats(), start, end, _quantization));
	} else {
		TempoMap::SharedPtr tmap (TempoMap::use());
		BBT_Time bbt = tmap->bbt_at (timepos_t (start));
		bbt = bbt.round_up_to_bar ();
		bbt.bars = (bbt.bars / _quantization.bars) * _quantization.bars;
		transition_time = timepos_t (tmap->quarters_at (bbt));
	}

	if (transition_time.beats() >= start && transition_time < end) {

		transition_samples = transition_time.samples();
		transition_beats = transition_time.beats ();

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 in range, should start/stop at %2 aka %3\n", index(), transition_samples, transition_beats));

		if (_state == WaitingToStop) {
			_state = Stopping;
			PropertyChanged (ARDOUR::Properties::running);
			return RunEnd;
		} else if (_state == WaitingToStart) {
			retrigger ();
			_state = Running;
			_box.prepare_next (_index);
			PropertyChanged (ARDOUR::Properties::running);
			return RunStart;
		} else if (_state == WaitingForRetrigger) {
			retrigger ();
			_state = Running;
			_box.prepare_next (_index);
			PropertyChanged (ARDOUR::Properties::running);
			return RunAll;
		}
	} else {
		if (_state == WaitingForRetrigger || _state == WaitingToStop) {
			/* retrigger time has not been reached, just continue
			   to play normally until then.
			*/
			return RunAll;
		}
	}

	return RunNone;
}

/*--------------------*/

AudioTrigger::AudioTrigger (uint64_t n, TriggerBox& b)
	: Trigger (n, b)
	, data (0)
	, read_index (0)
	, data_length (0)
	, _start_offset (0)
	, _legato_offset (0)
	, usable_length (0)
	, last_sample (0)
	, _stretcher (0)
{
}

AudioTrigger::~AudioTrigger ()
{
	for (std::vector<Sample*>::iterator d = data.begin(); d != data.end(); ++d) {
		delete *d;
	}

	delete _stretcher;
}

void
AudioTrigger::startup ()
{
	Trigger::startup ();
	retrigger ();
}

void
AudioTrigger::jump_start ()
{
	Trigger::jump_start ();
	retrigger ();
}

void
AudioTrigger::jump_stop ()
{
	Trigger::jump_stop ();
	retrigger ();
}

double
AudioTrigger::position_as_fraction () const
{
	if (!active()) {
		return 0.0;
	}

	return read_index / (double) usable_length;
}

XMLNode&
AudioTrigger::get_state (void)
{
	XMLNode& node (Trigger::get_state());

	node.set_property (X_("start"), timepos_t (_start_offset));
	node.set_property (X_("length"), timepos_t (usable_length));

	return node;
}

int
AudioTrigger::set_state (const XMLNode& node, int version)
{
	timepos_t t;

	if (!Trigger::set_state (node, version)) {
		return -1;
	}

	node.get_property (X_("start"), t);
	_start_offset = t.samples();

	node.get_property (X_("length"), t);
	usable_length = t.samples();
	last_sample = _start_offset + usable_length;

	return 0;
}

void
AudioTrigger::set_start (timepos_t const & s)
{
	_start_offset = s.samples ();
}

void
AudioTrigger::set_end (timepos_t const & e)
{
	assert (!data.empty());
	set_length (timecnt_t (e.samples() - _start_offset, timepos_t (_start_offset)));
}

void
AudioTrigger::set_legato_offset (timepos_t const & offset)
{
	_legato_offset = offset.samples();
}

timepos_t
AudioTrigger::current_pos() const
{
	return timepos_t (read_index);
}

void
AudioTrigger::set_usable_length ()
{
	if (!_region) {
		return;
	}

	switch (_launch_style) {
	case Repeat:
		break;
	default:
		usable_length = data_length;
		last_sample = _start_offset + usable_length;
		return;
	}

	if (_quantization == Temporal::BBT_Offset ()) {
		usable_length = data_length;
		last_sample = _start_offset + usable_length;
		return;
	}

	/* XXX MUST HANDLE BAR-LEVEL QUANTIZATION */

	timecnt_t len (Temporal::Beats (_quantization.beats, _quantization.ticks), timepos_t (Temporal::Beats()));
	usable_length = len.samples();
	last_sample = _start_offset + usable_length;
}

void
AudioTrigger::set_length (timecnt_t const & newlen)
{
	/* XXX what? */
}

timepos_t
AudioTrigger::current_length() const
{
	if (_region) {
		return timepos_t (data_length);
	}
	return timepos_t (Temporal::BeatTime);
}

timepos_t
AudioTrigger::natural_length() const
{
	if (_region) {
		return timepos_t::from_superclock (_region->length().magnitude());
	}
	return timepos_t (Temporal::BeatTime);
}

int
AudioTrigger::set_region_threaded (boost::shared_ptr<Region> r)
{
	using namespace RubberBand;

	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);

	if (!ar) {
		return -1;
	}

	set_region_internal (r);
	load_data (ar);
	determine_tempo ();
	setup_stretcher ();

	PropertyChanged (ARDOUR::Properties::name);

	return 0;
}

void
AudioTrigger::determine_tempo ()
{
	using namespace Temporal;

	/* now potentially stretch it to match our tempo.
	 *
	 * We do not handle tempo changes at present, and should probably issue
	 * a warming about this.
	 */

	TempoMap::SharedPtr tm (TempoMap::use());

	if (_barcnt == 0) {

		TempoMetric const & metric (tm->metric_at (timepos_t (AudioTime)));
		breakfastquay::MiniBPM mbpm (_box.session().sample_rate());

		mbpm.setBPMRange (metric.tempo().quarter_notes_per_minute () * 0.75, metric.tempo().quarter_notes_per_minute() * 1.5);

		_apparent_tempo = mbpm.estimateTempoOfSamples (data[0], data_length);

		if (_apparent_tempo == 0.0) {
			/* no apparent tempo, just return since we'll use it as-is */
			return;
		}

		cerr << name() << " Estimated bpm " << _apparent_tempo << " from " << (double) data_length / _box.session().sample_rate() << " seconds\n";

		const double seconds = (double) data_length  / _box.session().sample_rate();
		const double quarters = (seconds / 60.) * _apparent_tempo;
		_barcnt = quarters / metric.meter().divisions_per_bar();
	}

	/* use initial tempo in map (assumed for now to be the only one */

	const samplecnt_t one_bar = tm->bbt_duration_at (timepos_t (AudioTime), BBT_Offset (1, 0, 0)).samples();

	cerr << "one bar in samples: " << one_bar << endl;
	cerr << "barcnt = " << round (_barcnt) << endl;
}

void
AudioTrigger::setup_stretcher ()
{
	using namespace RubberBand;
	using namespace Temporal;

	if (!_region) {
		return;
	}

	boost::shared_ptr<AudioRegion> ar (boost::dynamic_pointer_cast<AudioRegion> (_region));
	const uint32_t nchans = ar->n_channels();

	/* XXX maybe get some of these options from region properties (when/if we have them) ? */

	RubberBandStretcher::Options options = RubberBandStretcher::Option (RubberBandStretcher::OptionProcessRealTime |
	                                                                    RubberBandStretcher::OptionTransientsCrisp |
	                                                                    RubberBandStretcher::OptionPhaseIndependent);

	if (!_stretcher) {
		_stretcher = new RubberBandStretcher (_box.session().sample_rate(), nchans, options, 1.0, 1.0);
	} else {
		_stretcher->reset ();
	}

	/* XXX this needs to change if the engine buffer size changes */

	_stretcher->setMaxProcessSize (AudioEngine::instance()->raw_buffer_size (DataType::AUDIO));
}

void
AudioTrigger::drop_data ()
{
	for (uint32_t n = 0; n < data.size(); ++n) {
		delete [] data[n];
	}
	data.clear ();
}

int

AudioTrigger::load_data (boost::shared_ptr<AudioRegion> ar)
{
	const uint32_t nchans = ar->n_channels();

	data_length = ar->length_samples();

	/* if usable length was already set, only adjust it if it is too large */
	if (!usable_length || usable_length > data_length) {
		usable_length = data_length;
		last_sample = _start_offset + usable_length;
	}

	drop_data ();

	try {
		for (uint32_t n = 0; n < nchans; ++n) {
			data.push_back (new Sample[data_length]);
			ar->read (data[n], 0, data_length, n);
		}

		set_name (ar->name());

	} catch (...) {
		drop_data ();
		return -1;
	}

	return 0;
}

void
AudioTrigger::retrigger ()
{
	read_index = _start_offset + _legato_offset;
	_legato_offset = 0; /* used one time only */
	_stretcher->reset ();
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 retriggered to %2\n", _index, read_index));
}

int
AudioTrigger::run (BufferSet& bufs, pframes_t nframes, pframes_t dest_offset, bool first, double bpm)
{
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(_region);
	const uint32_t nchans = ar->n_channels();
	const bool long_enough_to_fade = (nframes >= 64);

	assert (ar);
	assert (active());

	/* tell the stretcher what we are doing for this ::run() call */

	if (_apparent_tempo == 0.) {
		/* XXX just use data directly */
		return 0;
	}

	const double stretch = _apparent_tempo / bpm;
	_stretcher->setTimeRatio (stretch);

	cerr << "apparent " << _apparent_tempo << " bpm " << bpm << " TR " << std::setprecision (4) << stretch << endl;

	int avail = _stretcher->available();

	if (avail < 0) {
		error << _("Could not configure rubberband stretcher") << endmsg;
		return -1;
	}

	while (nframes) {

		pframes_t this_read;

		if (read_index < last_sample) {

			/* still have data to push into the stretcher */

			this_read = (pframes_t) std::min ((samplecnt_t) nframes, (last_sample - read_index));
			const bool at_end = (this_read < nframes);

			while ((pframes_t) avail < this_read && (read_index < last_sample)) {
				/* keep feeding the stretcher in chunks of "this_read",
				 * until there's nframes of data available, or we reach
				 * the end of the region
				 */


				Sample* in[nchans];

				for (uint32_t chn = 0; chn < nchans; ++chn) {
					in[chn] = data[chn] + read_index;
				}

				_stretcher->process (in, this_read, at_end);
				read_index += this_read;

				avail = _stretcher->available ();
			}
		} else {
			/* finished reading data, but have not yet delivered it all */
			avail = _stretcher->available ();
			this_read = (pframes_t) std::min ((pframes_t) nframes, (pframes_t) avail);
		}

		/* Set up buffers for RB to write into */

		Sample* out[nchans];
		BufferSet& scratch (_box.session().get_scratch_buffers (ChanCount (DataType::AUDIO, nchans)));

		for (uint32_t chn = 0; chn < bufs.count().n_audio(); ++chn) {
			out[chn] = scratch.get_audio (chn).data();
		}

		/* fetch the stretch */

		_stretcher->retrieve (out, this_read);

		/* deliver to buffers */

		for (uint32_t chn = 0; chn < bufs.count().n_audio(); ++chn) {

			uint64_t channel = chn %  data.size();
			AudioBuffer& buf (bufs.get_audio (chn));
			Sample* src = out[channel];

			if (first) {
				buf.read_from (src, this_read, dest_offset);
				if (_gain != 1.0) {
					buf.apply_gain (_gain, this_read);
				}
			} else {
				if (_gain != 1.0) {
					buf.accumulate_with_gain_from (src, this_read, _gain, dest_offset);
				} else {
					buf.accumulate_with_gain_from (src, this_read, _gain, dest_offset);
				}
			}
		}

		avail = _stretcher->available ();

		if (read_index >= last_sample && avail <= 0) {

			/* We reached the end */

			_loop_cnt++;

			if ((_loop_cnt == _follow_count) || (_launch_style == Repeat) || (_box.peek_next_trigger() == this)) { /* self repeat */
				nframes -= this_read;
				dest_offset += this_read;
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end, but set to loop, so retrigger\n", index()));
				retrigger ();
				/* and go around again */
				continue;

			} else {

				if (this_read < nframes) {

					for (uint64_t chn = 0; chn < ar->n_channels(); ++chn) {
						uint64_t channel = chn %  data.size();
						AudioBuffer& buf (bufs.get_audio (channel));
						DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 short fill, ri %2 vs ls %3, do silent fill\n", index(), read_index, last_sample));
						buf.silence (nframes - this_read, dest_offset + this_read);
					}
				}
				shutdown ();
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end, now stopped\n", index()));
				break;
			}
		}

		nframes -= this_read;
	}

	if (_state == Stopping && long_enough_to_fade) {
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 was stopping, now stopped\n", index()));
		shutdown ();
	}

	return 0;
}

void
AudioTrigger::reload (BufferSet&, void*)
{
}

/*--------------------*/

/* Design notes:

   for the ::run() call, where we are in process() context, we will use an
   RTMidiBuffer as the data structure holding our MIDI. The events here form a
   simple array of sample-timestamped MIDI events (though with the extra
   complication of having to handle 2/3 byte messages *slightly* differently
   from larger messages).

   This allows us to actually use a simple integer array index to record where
   we are during playback and know when we've reached the end.

   However, attributes of the trigger like start_offset are kept in BBT_Offsets
   or Beats as appropriate, because those are the correct temporal
   semantics. This means that we need to refer back to the Region for some
   things, since it will be the place where we can look at MIDI events with
   musical time stamps (unlike the sample timestamps in the RTMidiBuffer).

   To keep things simple, this means that we will only render the actual clip
   into the RTMidiBuffer - if start_offset/length reduce the clip from the
   Region bounds, we will not place the "excess" in the RTMidiBuffer.

   However, if we do any UI display of the clip, we will use the Region for
   that, partly because it has music time timestamps and partly because we
   already have GUI objects that can operate on MIDIRegions.

 */

MIDITrigger::MIDITrigger (uint64_t n, TriggerBox& b)
	: Trigger (n, b)
	, data (0)
	, read_index (0)
	, data_length (0)
	, usable_length (0)
	, _start_offset (0, 0, 0)
	, _legato_offset (0, 0, 0)
{
}

MIDITrigger::~MIDITrigger ()
{
	drop_data ();
}

void
MIDITrigger::startup ()
{
	Trigger::startup ();
	retrigger ();
}

void
MIDITrigger::jump_start ()
{
	Trigger::jump_start ();
	retrigger ();
}

void
MIDITrigger::jump_stop ()
{
	Trigger::jump_stop ();
	retrigger ();
}

double
MIDITrigger::position_as_fraction () const
{
	if (!active()) {
		return 0.0;
	}

	if (data->size() == 0) {
		return 0.0;
	}

	if (read_index >= data->size()) {
		return 1.0;
	}

	const samplepos_t l = (*data)[read_index].timestamp;

	return l / (double) usable_length;
}

XMLNode&
MIDITrigger::get_state (void)
{
	XMLNode& node (Trigger::get_state());

	node.set_property (X_("start"), start_offset());
	node.set_property (X_("length"), timepos_t (usable_length));

	return node;
}

int
MIDITrigger::set_state (const XMLNode& node, int version)
{
	timepos_t t;

	if (!Trigger::set_state (node, version)) {
		return -1;
	}

	node.get_property (X_("start"), t);
	Temporal::Beats b (t.beats());
	/* XXX need to deal with bar offsets */
	_start_offset = Temporal::BBT_Offset (0, b.get_beats(), b.get_ticks());

	node.get_property (X_("length"), t);
	usable_length = t.samples();

	return 0;
}

void
MIDITrigger::set_start (timepos_t const & s)
{
	/* XXX need to handle bar offsets */
	Temporal::Beats b (s.beats());
	_start_offset = Temporal::BBT_Offset (0, b.get_beats(), b.get_ticks());
}

void
MIDITrigger::set_end (timepos_t const & e)
{
	/* XXX need to handle bar offsets */
	set_length (timecnt_t (e.beats() - Temporal::Beats (_start_offset.beats, _start_offset.ticks), start_offset()));
}

void
MIDITrigger::set_legato_offset (timepos_t const & offset)
{
	/* XXX need to handle bar offsets */
	Temporal::Beats b (offset.beats());
	_legato_offset = Temporal::BBT_Offset (0, b.get_beats(), b.get_ticks());
}

timepos_t
MIDITrigger::start_offset () const
{
	/* XXX single meter assumption */

	Temporal::Meter const &m = Temporal::TempoMap::use()->meter_at (Temporal::Beats (0, 0));
	return timepos_t (m.to_quarters (_start_offset));
}

timepos_t
MIDITrigger::current_pos() const
{
	return timepos_t ((*data)[read_index].timestamp);
}

void
MIDITrigger::set_length (timecnt_t const & newlen)
{

}

void
MIDITrigger::set_usable_length ()
{
	if (!_region) {
		return;
	}

	switch (_launch_style) {
	case Repeat:
		break;
	default:
		usable_length = data_length;
		return;
	}

	if (_quantization == Temporal::BBT_Offset ()) {
		usable_length = data_length;
		return;
	}

	/* XXX MUST HANDLE BAR-LEVEL QUANTIZATION */

	timecnt_t len (Temporal::Beats (_quantization.beats, _quantization.ticks), timepos_t (Temporal::Beats()));
	usable_length = len.samples ();
}

timepos_t
MIDITrigger::current_length() const
{
	if (_region) {
		return timepos_t (data_length);
	}
	return timepos_t (Temporal::BeatTime);
}

timepos_t
MIDITrigger::natural_length() const
{
	if (_region) {
		return timepos_t::from_ticks (_region->length().magnitude());
	}
	return timepos_t (Temporal::BeatTime);
}

int
MIDITrigger::set_region_threaded (boost::shared_ptr<Region> r)
{
	boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion> (r);

	if (!mr) {
		return -1;
	}

	set_region_internal (r);
	load_data (mr);
	set_length (mr->length());

	mr->model()->ContentsChanged.connect_same_thread (content_connection, boost::bind (&MIDITrigger::re_render, this));
	mr->PropertyChanged.connect_same_thread (content_connection, boost::bind (&MIDITrigger::re_render, this));

	PropertyChanged (ARDOUR::Properties::name);

	return 0;
}

void
MIDITrigger::render (RTMidiBuffer& rtmb)
{
	/* this generates timestamps in session time. We want trigger-relative
	 * time (so the beginning of the region/trigger is zero).
	 */

	boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion> (_region);
	assert (mr);

	mr->render_range (rtmb, 0, Sustained, mr->start(), mr->length(), 0);
	const sampleoffset_t shift = mr->position().samples();
	rtmb.shift (-shift);
}

void
MIDITrigger::reload (BufferSet& bufs, void* ptr)
{
	MidiBuffer& mb (bufs.get_midi (0));

	tracker.resolve_notes (mb, 0);

	RTMidiBuffer* rtmb = reinterpret_cast<RTMidiBuffer*> (ptr);

	std::swap (data, rtmb);

	delete rtmb;
}

void
MIDITrigger::re_render ()
{
	RTMidiBuffer* new_data = new RTMidiBuffer;
	std::cerr << "will re-render " << _region->name() << " into " << new_data << std::endl;
	render (*new_data);
	_box.request_reload (_index, new_data);
}

void
MIDITrigger::drop_data ()
{
	delete data;
	data = 0;
}

int
MIDITrigger::load_data (boost::shared_ptr<MidiRegion> mr)
{
	drop_data ();

	data = new RTMidiBuffer;

	render (*data);

	set_name (mr->name());

	/* There may not be a MIDI event at the end of the region, but we use
	 * the region size to define how long this trigger is. This allows for
	 * space at the end of the region to be a part of the timing.
	 */

	data_length = mr->length().samples();

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 loaded midi region, span is %2\n", name(), data_length));

	return 0;
}

void
MIDITrigger::retrigger ()
{
	/* XXX need to deal with bar offsets */
	// const Temporal::BBT_Offset o = _start_offset + _legato_offset;
	read_index = 0;
	_legato_offset = Temporal::BBT_Offset ();
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 retriggered to %2, ts = %3\n", _index, read_index, transition_samples));
}

int
MIDITrigger::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, pframes_t nframes, pframes_t dest_offset, bool first, double bpm)
{
	MidiBuffer& mb (bufs.get_midi (0));

	while (true) {

		if (read_index < data->size()) {

			RTMidiBuffer::Item const & item ((*data)[read_index]);

			/* timestamps inside the RTMidiBuffer are relative to
			   the start of the region.

			   Offset them to give us process/timeline timestamps.
			*/

			const samplepos_t effective_time = transition_samples + item.timestamp;

			// cerr << start_sample << " .. " << end_sample << " Item " << read_index << " @ " << item.timestamp << " + " << transition_samples << " = " << effective_time << endl;

			if (effective_time >= start_sample && effective_time < end_sample) {

				uint32_t sz;
				uint8_t const * bytes = data->bytes (item, sz);

				samplepos_t process_relative_timestamp = effective_time - start_sample;

				const Evoral::Event<MidiBuffer::TimeType> ev (Evoral::MIDI_EVENT, process_relative_timestamp, sz, const_cast<uint8_t*>(bytes), false);
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("inserting %1\n", ev));
				mb.insert_event (ev);

				tracker.track (bytes);
				read_index++;

			} else {
				break;
			}
		}

		const samplepos_t region_end = transition_samples + data_length;

		if (read_index >= data->size() || (_state == Running && region_end >= start_sample && region_end <= end_sample)) {

			/* We reached the end */

			cerr << "reached end, ri " << read_index << " rend " << transition_samples + data_length << " vs end @ " << end_sample << endl;

			_loop_cnt++;

			if ((_loop_cnt == _follow_count) || (_launch_style == Repeat) || (_box.peek_next_trigger() == this)) { /* self repeat */
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end, but set to loop, so retrigger\n", index()));

				/* we will "restart" at the beginning of the
				   next iteration of the trigger.
				*/
				transition_samples = transition_samples + data_length;
				retrigger ();
				/* and go around again */
				continue;

			} else {

				shutdown ();
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end, now stopped\n", index()));
				break;
			}

		}
	}

	if (_state == Stopping) {
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 was stopping, now stopped\n", index()));
		tracker.resolve_notes (mb, nframes);
		shutdown ();
	}

	return 0;
}

/**************/

void
Trigger::make_property_quarks ()
{
	Properties::muted.property_id = g_quark_from_static_string (X_("running"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for running = %1\n", Properties::running.property_id));
	Properties::legato.property_id = g_quark_from_static_string (X_("legato"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for legato = %1\n", Properties::legato.property_id));
	Properties::use_follow.property_id = g_quark_from_static_string (X_("use-follow"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for use-follow = %1\n", Properties::use_follow.property_id));
	Properties::quantization.property_id = g_quark_from_static_string (X_("quantization"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for quantization = %1\n", Properties::quantization.property_id));
	Properties::launch_style.property_id = g_quark_from_static_string (X_("launch-style"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for quantization = %1\n", Properties::launch_style.property_id));
	Properties::follow_action0.property_id = g_quark_from_static_string (X_("follow-action-0"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for follow-action-0 = %1\n", Properties::follow_action0.property_id));
	Properties::follow_action1.property_id = g_quark_from_static_string (X_("follow-action-1"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for follow-action-1 = %1\n", Properties::follow_action1.property_id));
}

const int32_t TriggerBox::default_triggers_per_box = 8;
Temporal::BBT_Offset TriggerBox::_assumed_trigger_duration (4, 0, 0);
//TriggerBox::TriggerMidiMapMode TriggerBox::_midi_map_mode (TriggerBox::AbletonPush);
TriggerBox::TriggerMidiMapMode TriggerBox::_midi_map_mode (TriggerBox::SequentialNote);
int TriggerBox::_first_midi_note = 60;
std::atomic<int32_t> TriggerBox::_pending_scene (-1);
std::atomic<int32_t> TriggerBox::_active_scene (-1);
std::atomic<int> TriggerBox::active_trigger_boxes (0);
TriggerBoxThread* TriggerBox::worker = 0;
PBD::Signal0<void> TriggerBox::StopAllTriggers;

void
TriggerBox::init ()
{
	worker = new TriggerBoxThread;
	TriggerBoxThread::init_request_pool ();
	init_pool ();
}

void
TriggerBox::start_transport_stop (Session& s)
{
	if (active_trigger_boxes.load ()) {
		StopAllTriggers(); /* EMIT SIGNAL */
	} else {
		s.stop_transport_from_trigger ();
	}
}

TriggerBox::TriggerBox (Session& s, DataType dt)
	: Processor (s, _("TriggerBox"), Temporal::BeatTime)
	, _bang_queue (1024)
	, _unbang_queue (1024)
	, _data_type (dt)
	, _order (-1)
	, explicit_queue (64)
	, up_next (0)
	, currently_playing (0)
	, _stop_all (false)
	, requests (1024)
{

	/* default number of possible triggers. call ::add_trigger() to increase */

	if (_data_type == DataType::AUDIO) {
		for (uint64_t n = 0; n < default_triggers_per_box; ++n) {
			all_triggers.push_back (new AudioTrigger (n, *this));
		}
	} else {
		for (uint64_t n = 0; n < default_triggers_per_box; ++n) {
			all_triggers.push_back (new MIDITrigger (n, *this));
		}
	}

	Config->ParameterChanged.connect_same_thread (*this, boost::bind (&TriggerBox::parameter_changed, this, _1));

	StopAllTriggers.connect_same_thread (stop_all_connection, boost::bind (&TriggerBox::request_stop_all, this));
}

void
TriggerBox::scene_bang (uint32_t n)
{
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("scene bang on %1 for %2\n", n));
	_pending_scene = n;
}

void
TriggerBox::scene_unbang (uint32_t n)
{
}

void
TriggerBox::maybe_find_scene_bang ()
{
	int32_t pending = _pending_scene.exchange (-1);

	if (pending >= 0) {
		_active_scene = pending;
	}
}

void
TriggerBox::clear_scene_bang ()
{
	(void) _active_scene.exchange (-1);
}

void
TriggerBox::clear_implicit ()
{
	up_next = 0;
}

void
TriggerBox::set_order (int32_t n)
{
	_order = n;
}

void
TriggerBox::queue_explict (Trigger* t)
{
	assert (t);
	explicit_queue.write (&t, 1);
	clear_implicit ();
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("explicit queue %1, EQ = %2\n", t->index(), explicit_queue.read_space()));

	if (currently_playing) {
		currently_playing->unbang ();
	}
}

Trigger*
TriggerBox::peek_next_trigger ()
{
	/* allows us to check if there's a next trigger queued, without
	 * actually reading it from either of the queues.
	 */

	RingBuffer<Trigger*>::rw_vector rwv;

	explicit_queue.get_read_vector (&rwv);

	if (rwv.len[0] > 0) {
		return *(rwv.buf[0]);
	}

	return up_next;
}

Trigger*
TriggerBox::get_next_trigger ()
{
	Trigger* r;

	if (explicit_queue.read (&r, 1) == 1) {
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("next trigger from explicit queue = %1\n", r->index()));
		return r;
	}

	if (up_next) {
		r = up_next;
		up_next = 0;
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("next trigger from implicit queue = %1\n", r->index()));
		return r;
	}

	return 0;
}

void
TriggerBox::set_from_selection (uint64_t slot, boost::shared_ptr<Region> region)
{
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("load %1 into %2\n", region->name(), slot));

	if (slot >= all_triggers.size()) {
		return;
	}

	all_triggers[slot]->set_region (region);
}

void
TriggerBox::set_from_path (uint64_t slot, std::string const & path)
{
	if (slot >= all_triggers.size()) {
		return;
	}

	try {
		SoundFileInfo info;
		string errmsg;

		if (!SndFileSource::get_soundfile_info (path, info, errmsg)) {
			error << string_compose (_("Cannot get info from audio file %1 (%2)"), path, errmsg) << endmsg;
			return;
		}

		SourceList src_list;

		for (uint16_t n = 0; n < info.channels; ++n) {
			boost::shared_ptr<Source> source (SourceFactory::createExternal (DataType::AUDIO, _session, path, n, Source::Flag (0), true));
			if (!source) {
				error << string_compose (_("Cannot create source from %1"), path) << endmsg;
				src_list.clear ();
				return;
			}
			src_list.push_back (source);
		}

		PropertyList plist;

		plist.add (Properties::start, 0);
		plist.add (Properties::length, src_list.front()->length ());
		plist.add (Properties::name, basename_nosuffix (path));
		plist.add (Properties::layer, 0);
		plist.add (Properties::layering_index, 0);

		boost::shared_ptr<Region> the_region (RegionFactory::create (src_list, plist, true));

		all_triggers[slot]->set_region (the_region);

		/* XXX catch region going away */

	} catch (std::exception& e) {
		cerr << "loading sample from " << path << " failed: " << e.what() << endl;
		return;
	}
}

TriggerBox::~TriggerBox ()
{
	drop_triggers ();
}

void
TriggerBox::request_stop_all ()
{
	_stop_all = true;
}

void
TriggerBox::stop_all ()
{
	/* XXX needs to be done with mutex or via thread-safe queue */

	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->stop (-1);
	}

	clear_implicit ();
	explicit_queue.reset ();

	_stop_all = false;
}

void
TriggerBox::drop_triggers ()
{
	Glib::Threads::RWLock::WriterLock lm (trigger_lock);

	for (Triggers::iterator t = all_triggers.begin(); t != all_triggers.end(); ++t) {
		if (*t) {
			delete *t;
			(*t) = 0;
		}
	}

	all_triggers.clear ();
}

Trigger*
TriggerBox::trigger (Triggers::size_type n)
{
	Glib::Threads::RWLock::ReaderLock lm (trigger_lock);

	if (n >= all_triggers.size()) {
		return 0;
	}

	return all_triggers[n];
}

void
TriggerBox::add_midi_sidechain (std::string const & name)
{
	if (!_sidechain) {
		_sidechain.reset (new SideChain (_session, name + "-trig"));
		_sidechain->activate ();
		_sidechain->input()->add_port ("", owner(), DataType::MIDI); // add a port, don't connect.
		boost::shared_ptr<Port> p = _sidechain->input()->nth (0);

		if (p) {
			p->connect (Config->get_default_trigger_input_port());
		} else {
			error << _("Could not create port for trigger side-chain") << endmsg;
		}
	}
}

bool
TriggerBox::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out.set_audio (std::max (out.n_audio(), 2U)); /* for now, enforce stereo output */
	if (_sidechain) {
		out.set_midi (std::max (out.n_midi(), 1U));
	}
	return true;
}

bool
TriggerBox::configure_io (ChanCount in, ChanCount out)
{
	if (_sidechain) {
		_sidechain->configure_io (in, out);
	}
	return Processor::configure_io (in, out);
}

void
TriggerBox::add_trigger (Trigger* trigger)
{
	Glib::Threads::RWLock::WriterLock lm (trigger_lock);
	all_triggers.push_back (trigger);
}

void
TriggerBox::set_midi_map_mode (TriggerMidiMapMode m)
{
	_midi_map_mode = m;
}

void
TriggerBox::set_first_midi_note (int n)
{
	_first_midi_note = n;
}

int
TriggerBox::note_to_trigger (int midi_note, int channel)
{
	const int column = _order;
	int first_note;
	int top;

	switch (_midi_map_mode) {

	case AbletonPush:
		/* the top row of pads generate MIDI note 92, 93, 94 and so on.
		   Each lower row generates notes 8 below the one above it.
		*/
		top = 92 + column;
		for (int row = 0; row < 8; ++row) {
			if (midi_note == top - (row * 8)) {
				return row;
			}
		}
		return -1;
		break;

	case SequentialNote:
		first_note = _first_midi_note + (column * all_triggers.size());
		return midi_note - first_note; /* direct access to row */

	case ByMidiChannel:
		first_note = 3;
		break;

	default:
		break;

	}

	return midi_note;
}

void
TriggerBox::process_midi_trigger_requests (BufferSet& bufs)
{
	/* check MIDI port input buffers for triggers */

	for (BufferSet::midi_iterator mi = bufs.midi_begin(); mi != bufs.midi_end(); ++mi) {
		MidiBuffer& mb (*mi);

		for (MidiBuffer::iterator ev = mb.begin(); ev != mb.end(); ++ev) {

			if (!(*ev).is_note()) {
				continue;
			}

			int trigger_number = note_to_trigger ((*ev).note(), (*ev).channel());

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("note %1 received on %2, translated to trigger num %3\n", (int) (*ev).note(), (int) (*ev).channel(), trigger_number));

			if (trigger_number < 0) {
				/* not for us */
				continue;
			}

			if (trigger_number > (int) all_triggers.size()) {
				continue;
			}

			Trigger* t = all_triggers[trigger_number];

			if (!t) {
				continue;
			}

			if ((*ev).is_note_on()) {

				if (t->midi_velocity_effect() != 0.0) {
					/* if MVE is zero, MIDI velocity has no
					   impact on gain. If it is small, it
					   has a small effect on gain. As it
					   approaches 1.0, it has full control
					   over the trigger gain.
				*/
					t->set_gain (1.0 - (t->midi_velocity_effect() * (*ev).velocity() / 127.f));
				}
				t->bang ();

			} else if ((*ev).is_note_off()) {

				t->unbang ();
			}
		}
	}
}

void
TriggerBox::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required)
{
	/* XXX a test to check if we have no usable slots would be good
	   here. if so, we can just return.
	*/

	if (_active) {
		if (!_pending_active) {
			_active = false;
			return;
		}
	} else {
		if (_pending_active) {
			_active = true;
		} else {
			return;
		}
	}

	if (start_sample < 0) {
		/* we can't do anything under these conditions (related to
		   latency compensation
		*/
		return;
	}

	if (_sidechain) {
		_sidechain->run (bufs, start_sample, end_sample, speed, nframes, true);
	}

	process_requests (bufs);

	process_midi_trigger_requests (bufs);

	if (_active_scene >= 0) {
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("tb noticed active scene %1\n", _active_scene));
		if (_active_scene < (int32_t) all_triggers.size()) {
			all_triggers[_active_scene]->bang ();
		}
	}

	/* now let each trigger handle any state changes */

	std::vector<uint64_t> to_run;

	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->process_state_requests ();
	}

	Trigger* nxt = 0;

	if (!currently_playing) {
		if ((currently_playing = get_next_trigger ()) != 0) {
			currently_playing->startup ();
			active_trigger_boxes.fetch_add (1);
		}
	}

	if (_stop_all) {
		stop_all ();
	}

	if (!currently_playing) {
		return;
	}

	/* transport must be active for triggers */

	if (!_session.transport_state_rolling()) {
		_session.start_transport_from_trigger ();
	}

	timepos_t start (start_sample);
	timepos_t end (end_sample);
	Temporal::Beats start_beats (start.beats());
	Temporal::Beats end_beats (end.beats());
	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());
	const double bpm = tmap->quarters_per_minute_at (start);
	uint64_t max_chans = 0;
	bool first = false;

	/* see if there's another trigger explicitly queued that has legato set. */

	RingBuffer<Trigger*>::rw_vector rwv;
	explicit_queue.get_read_vector (&rwv);

	if (rwv.len[0] > 0) {

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("explicit queue rvec %1 + 2%\n", rwv.len[0], rwv.len[1]));

		/* actually fetch it (guaranteed to pull from the explicit queue */

		nxt = get_next_trigger ();

		/* if user triggered same clip, with legato set, then there is
		 * nothing to do
		 */

		if (nxt != currently_playing) {

			if (nxt->legato()) {
				/* We want to start this trigger immediately, without
				 * waiting for quantization points, and it should start
				 * playing at the same internal offset as the current
				 * trigger.
				 */

				nxt->set_legato_offset (currently_playing->current_pos());
				nxt->jump_start ();
				currently_playing->jump_stop ();
				prepare_next (nxt->index());
				/* and switch */
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 => %2 switched to in legato mode\n", currently_playing->index(), nxt->index()));
				currently_playing = nxt;

			} else {

				currently_playing->start_stop ();
				up_next = all_triggers[nxt->index()];
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("start stop for %1 before switching to %2\n", currently_playing->index(), nxt->index()));
			}
		}
	}

	while (currently_playing) {

		assert (currently_playing->state() >= Trigger::WaitingToStart);

		Trigger::RunType rt;

		switch (currently_playing->state()) {
		case Trigger::WaitingToStop:
		case Trigger::WaitingToStart:
		case Trigger::WaitingForRetrigger:
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 in state %2, recompute next transition\n", currently_playing->name(), currently_playing->state()));
			rt = currently_playing->maybe_compute_next_transition (start_beats, end_beats);
			break;
		default:
			rt = Trigger::RunAll;
		}

		if (rt == Trigger::RunNone) {
			/* nothing to do at this time, still waiting to start */
			return;
		}

		boost::shared_ptr<Region> r = currently_playing->region();

		sampleoffset_t dest_offset;
		pframes_t trigger_samples;

		const bool was_waiting_to_start = (currently_playing->state() == Trigger::WaitingToStart);

		if (rt == Trigger::RunEnd) {

			/* trigger will reach it's end somewhere within this
			 * process cycle, so compute the number of samples it
			 * should generate.
			 */

			trigger_samples = nframes - (currently_playing->transition_samples - start_sample);
			dest_offset = 0;

		} else if (rt == Trigger::RunStart) {

			/* trigger will start somewhere within this process
			 * cycle. Compute the sample offset where any audio
			 * should end up, and the number of samples it should generate.
			 */

			dest_offset = std::max (samplepos_t (0), currently_playing->transition_samples - start_sample);
			trigger_samples = nframes - dest_offset;

		} else if (rt == Trigger::RunAll) {

			/* trigger is just running normally, and will fill
			 * buffers entirely.
			 */

			dest_offset = 0;
			trigger_samples = nframes;

		} else {
			/* NOTREACHED */
		}

		if (was_waiting_to_start) {
			determine_next_trigger (currently_playing->index());
		}

		AudioTrigger* at = dynamic_cast<AudioTrigger*> (currently_playing);

		if (at) {

			boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);
			const uint64_t nchans = ar->n_channels ();

			max_chans = std::max (max_chans, nchans);

			at->run (bufs, trigger_samples, dest_offset, first, bpm);

			first = false;

		} else {
			MIDITrigger* mt = dynamic_cast<MIDITrigger*> (currently_playing);

			if (mt) {
				mt->run (bufs, start_sample, end_sample, trigger_samples, dest_offset, first, bpm);
				first = false;
			}

		}

		if (currently_playing->state() == Trigger::Stopped) {

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 did stop\n", currently_playing->index()));

			Trigger* nxt = get_next_trigger ();

			if (nxt) {

				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 switching to %2\n", currently_playing->index(), nxt->index()));
				if (nxt->legato()) {
					nxt->set_legato_offset (currently_playing->current_pos());
				}
				/* start it up */
				nxt->startup();
				currently_playing = nxt;

			} else {
				currently_playing = 0;
				if (active_trigger_boxes.fetch_sub (1) == 1) {
					/* last active trigger box */
					_session.stop_transport_from_trigger ();
				}
			}

		} else {
			/* done */
			break;
		}
	}

	ChanCount cc (DataType::AUDIO, max_chans);
	cc.set_midi (bufs.count().n_midi());
	bufs.set_count (cc);
}

void
TriggerBox::prepare_next (uint64_t current)
{
	int nxt = determine_next_trigger (current);

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("nxt for %1 = %2\n", current, nxt));

	if (nxt >= 0) {
		up_next = all_triggers[nxt];
	}
}

int
TriggerBox::determine_next_trigger (uint64_t current)
{
	uint64_t n;
	uint64_t runnable = 0;

	/* count number of triggers that can actually be run (i.e. they have a region) */

	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		if (all_triggers[n]->region()) {
			runnable++;
		}
	}

	/* decide which of the two follow actions we're going to use (based on
	 * random number and the probability setting)
	 */

	int which_follow_action;
	int r = _pcg.rand (100); // 0 .. 99

	if (r <= all_triggers[current]->follow_action_probability()) {
		which_follow_action = 0;
	} else {
		which_follow_action = 1;
	}

	/* first switch: deal with the "special" cases where we either do
	 * nothing or just repeat the current trigger
	 */

	switch (all_triggers[current]->follow_action (which_follow_action)) {

	case Trigger::Stop:
		return -1;

	case Trigger::QueuedTrigger:
		/* XXX implement me */
		return -1;
	default:
		if (runnable == 1) {
			/* there's only 1 runnable trigger, so the "next" one
			   is the same as the current one.
			*/
			return current;
		}
	}

	/* second switch: handle the "real" follow actions */

	switch (all_triggers[current]->follow_action (which_follow_action)) {

	case Trigger::Again:
		return current;

	case Trigger::NextTrigger:
		n = current;
		while (true) {
			++n;

			if (n >= all_triggers.size()) {
				n = 0;
			}

			if (n == current) {
				cerr << "outa here\n";
				break;
			}

			if (all_triggers[n]->region() && !all_triggers[n]->active()) {
				return n;
			}
		}
		break;
	case Trigger::PrevTrigger:
		n = current;
		while (true) {
			if (n == 0) {
				n = all_triggers.size() - 1;
			} else {
				n -= 1;
			}

			if (n == current) {
				break;
			}

			if (all_triggers[n]->region() && !all_triggers[n]->active ()) {
				return n;
			}
		}
		break;

	case Trigger::FirstTrigger:
		for (n = 0; n < all_triggers.size(); ++n) {
			if (all_triggers[n]->region() && !all_triggers[n]->active ()) {
				return n;
			}
		}
		break;
	case Trigger::LastTrigger:
		for (int i = all_triggers.size() - 1; i >= 0; --i) {
			if (all_triggers[i]->region() && !all_triggers[i]->active ()) {
				return i;
			}
		}
		break;

	case Trigger::AnyTrigger:
		while (true) {
			n = _pcg.rand (all_triggers.size());
			if (!all_triggers[n]->region()) {
				continue;
			}
			if (all_triggers[n]->active()) {
				continue;
			}
			break;
		}
		return n;


	case Trigger::OtherTrigger:
		while (true) {
			n = _pcg.rand (all_triggers.size());
			if ((uint64_t) n == current) {
				continue;
			}
			if (!all_triggers[n]->region()) {
				continue;
			}
			if (all_triggers[n]->active()) {
				continue;
			}
			break;
		}
		return n;


	/* NOTREACHED */
	case Trigger::Stop:
	case Trigger::QueuedTrigger:
		break;

	}

	return current;
}

XMLNode&
TriggerBox::get_state (void)
{
	XMLNode& node (Processor::get_state ());

	node.set_property (X_("type"), X_("triggerbox"));
	node.set_property (X_("data-type"), _data_type.to_string());
	node.set_property (X_("order"), _order);

	XMLNode* trigger_child (new XMLNode (X_("Triggers")));

	{
		Glib::Threads::RWLock::ReaderLock lm (trigger_lock);
		for (Triggers::iterator t = all_triggers.begin(); t != all_triggers.end(); ++t) {
			trigger_child->add_child_nocopy ((*t)->get_state());
		}
	}

	node.add_child_nocopy (*trigger_child);

	if (_sidechain) {
		XMLNode* scnode = new XMLNode (X_("Sidechain"));
		std::string port_name = _sidechain->input()->nth (0)->name();
		port_name = port_name.substr (0, port_name.find ('-'));
		scnode->set_property (X_("name"), port_name);
		scnode->add_child_nocopy (_sidechain->get_state());
		node.add_child_nocopy (*scnode);
	}

	return node;
}

int
TriggerBox::set_state (const XMLNode& node, int version)
{
	Processor::set_state (node, version);

	node.get_property (X_("data-type"), _data_type);
	node.get_property (X_("order"), _order);

	XMLNode* tnode (node.child (X_("Triggers")));
	assert (tnode);

	XMLNodeList const & tchildren (tnode->children());

	drop_triggers ();

	{
		Glib::Threads::RWLock::WriterLock lm (trigger_lock);

		for (XMLNodeList::const_iterator t = tchildren.begin(); t != tchildren.end(); ++t) {
			Trigger* trig;

			if (_data_type == DataType::AUDIO) {
				trig = new AudioTrigger (all_triggers.size(), *this);
				all_triggers.push_back (trig);
				trig->set_state (**t, version);
			} else if (_data_type == DataType::MIDI) {
				trig = new MIDITrigger (all_triggers.size(), *this);
				all_triggers.push_back (trig);
				trig->set_state (**t, version);
			}
		}
	}

	XMLNode* scnode = node.child (X_("Sidechain"));

	if (scnode) {
		std::string name;
		scnode->get_property (X_("name"), name);
		add_midi_sidechain (name);
		assert (_sidechain);
		_sidechain->set_state (*scnode->children().front(), version);
	}

	return 0;
}

void
TriggerBox::parameter_changed (std::string const & param)
{
	if (param == X_("default-trigger-input-port")) {
		reconnect_to_default ();
	}
}

void
TriggerBox::reconnect_to_default ()
{
	if (!_sidechain) {
		return;
	}

	_sidechain->input()->nth (0)->disconnect_all ();
	_sidechain->input()->nth (0)->connect (Config->get_default_trigger_input_port());
}

MultiAllocSingleReleasePool* TriggerBox::Request::pool;

void
TriggerBox::init_pool ()
{
	/* "indirection" is because the Request struct is private, and so
	   nobody else can call its ::init_pool() static method.
	*/

	Request::init_pool ();
}

void
TriggerBox::Request::init_pool ()
{
	pool = new MultiAllocSingleReleasePool (X_("TriggerBoxRequests"), sizeof (TriggerBox::Request), 1024);
}

void*
TriggerBox::Request::operator new (size_t)
{
	return pool->alloc();
 }

void
TriggerBox::Request::operator delete (void *ptr, size_t /*size*/)
{
	return pool->release (ptr);
}

void
TriggerBox::request_reload (int32_t slot, void* ptr)
{
	Request* r = new Request (Request::Reload);
	r->slot = slot;
	r->ptr = ptr;
	requests.write (&r, 1);
}

void
TriggerBox::request_use (int32_t slot, Trigger& t)
{
	Request* r = new Request (Request::Use);
	r->slot = slot;
	r->trigger = &t;
	requests.write (&r, 1);
}

void
TriggerBox::process_requests (BufferSet& bufs)
{
	Request* r;

	while (requests.read (&r, 1) == 1) {
		process_request (bufs, r);
	}
}

void
TriggerBox::process_request (BufferSet& bufs, Request* req)
{
	switch (req->type) {
	case Request::Use:
		std::cerr << "Use for " << req->slot << std::endl;
		break;
	case Request::Reload:
		std::cerr << "Reload for " << req->slot << std::endl;
		reload (bufs, req->slot, req->ptr);
		break;
	}

	delete req; /* back to the pool, RT-safe */
}

void
TriggerBox::reload (BufferSet& bufs, int32_t slot, void* ptr)
{
	if (slot >= (int32_t) all_triggers.size()) {
		return;
	}
	std::cerr << "reload slot " << slot << std::endl;
	all_triggers[slot]->reload (bufs, ptr);
}

/* Thread */

MultiAllocSingleReleasePool* TriggerBoxThread::Request::pool = 0;

TriggerBoxThread::TriggerBoxThread ()
	: requests (1024)
	, _xthread (true)
{
	if (pthread_create_and_store ("triggerbox thread", &thread, _thread_work, this)) {
		error << _("Session: could not create triggerbox thread") << endmsg;
		throw failed_constructor ();
	}
}

TriggerBoxThread::~TriggerBoxThread()
{
	void* status;
	char msg = (char) Quit;
	_xthread.deliver (msg);
	pthread_join (thread, &status);
}

void *
TriggerBoxThread::_thread_work (void* arg)
{
	SessionEvent::create_per_thread_pool ("tbthread events", 4096);
	pthread_set_name (X_("tbthread"));
	return ((TriggerBoxThread *) arg)->thread_work ();
}

void *
TriggerBoxThread::thread_work ()
{
	while (true) {

		char msg;

		if (_xthread.receive (msg, true) >= 0) {

			if (msg == (char) Quit) {
				return (void *) 0;
				abort(); /*NOTREACHED*/
			}

			Temporal::TempoMap::fetch ();

			Request* req;

			while (requests.read (&req, 1) == 1) {
				switch (req->type) {
				case SetRegion:
					req->trig->set_region_threaded (req->region);
					break;
				default:
					break;
				}
				delete req; /* back to pool */
			}
		}
	}

	return (void *) 0;
}

void
TriggerBoxThread::queue_request (Request* req)
{
	char c = req->type;

	if (req->type != Quit) {
		if (requests.write (&req, 1) != 1) {
			return;
		}
	}
	if (_xthread.deliver (c) != 1) {
		/* the x-thread channel is non-blocking
		 * write may fail, but we really don't want to wait
		 * under normal circumstances.
		 *
		 * a lost "run" requests under normal RT operation
		 * is mostly harmless.
		 *
		 * TODO if ardour is freehweeling, wait & retry.
		 * ditto for Request::Type Quit
		 */
		assert(1); // we're screwd
	}
}

void*
TriggerBoxThread::Request::operator new (size_t)
{
	return pool->alloc ();
}

void
TriggerBoxThread::Request::operator delete (void* ptr, size_t)
{
	pool->release (ptr);
}

void
TriggerBoxThread::Request::init_pool ()
{
	pool = new MultiAllocSingleReleasePool (X_("TriggerBoxThreadRequests"), sizeof (TriggerBoxThread::Request), 1024);
}

void
TriggerBoxThread::set_region (Trigger* t, boost::shared_ptr<Region> r)
{
	TriggerBoxThread::Request* req = new TriggerBoxThread::Request (TriggerBoxThread::SetRegion);

	req->trig = t;
	req->region = r;

	queue_request (req);
}
