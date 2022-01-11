#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <sstream>

#include <boost/make_shared.hpp>

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
#include "ardour/import_status.h"
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
#include "ardour/smf_source.h"
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
		PBD::PropertyDescriptor<bool> passthru;
		PBD::PropertyDescriptor<bool> legato;
		PBD::PropertyDescriptor<Temporal::BBT_Offset> quantization;
		PBD::PropertyDescriptor<Temporal::BBT_Offset> follow_length;
		PBD::PropertyDescriptor<Trigger::LaunchStyle> launch_style;
		PBD::PropertyDescriptor<Trigger::FollowAction> follow_action0;
		PBD::PropertyDescriptor<Trigger::FollowAction> follow_action1;
		PBD::PropertyDescriptor<uint32_t> currently_playing;
		PBD::PropertyDescriptor<uint32_t> follow_count;
		PBD::PropertyDescriptor<int> follow_action_probability;
		PBD::PropertyDescriptor<float> velocity_effect;
		PBD::PropertyDescriptor<gain_t> gain;
		PBD::PropertyDescriptor<bool> stretchable;
		PBD::PropertyDescriptor<bool> isolated;
	}
}

Trigger * const Trigger::MagicClearPointerValue = (Trigger*) 0xfeedface;

Trigger::Trigger (uint32_t n, TriggerBox& b)
	: _box (b)
	, _state (Stopped)
	, _bang (0)
	, _unbang (0)
	, _index (n)
	, _loop_cnt (0)
	, _ui (0)
	, _explicitly_stopped (false)
	, _pending_velocity_gain (1.0)
	, _velocity_gain (1.0)
	, _launch_style (Properties::launch_style, OneShot)
	, _use_follow (Properties::use_follow, true)
	, _follow_action0 (Properties::follow_action0, Again)
	, _follow_action1 (Properties::follow_action1, Stop)
	, _follow_action_probability (Properties::follow_action_probability, 0)
	, _follow_count (Properties::follow_count, 1)
	, _quantization (Properties::quantization, Temporal::BBT_Offset (1, 0, 0))
	, _follow_length (Properties::quantization, Temporal::BBT_Offset (0, 0, 0))
	, _legato (Properties::legato, false)
	, _name (Properties::name, "")
	, _gain (Properties::gain, 1.0)
	, _midi_velocity_effect (Properties::velocity_effect, 0.)
	, _stretchable (Properties::stretchable, true)
	, _isolated (Properties::isolated, false)
	, _color (Properties::color, 0xBEBEBEFF)
	, cue_launched (false)
	, _barcnt (0.)
	, _apparent_tempo (0.)
	, expected_end_sample (0)
	, _pending ((Trigger*) 0)
{
	add_property (_launch_style);
	add_property (_use_follow);
	add_property (_follow_action0);
	add_property (_follow_action1);
	add_property (_follow_action_probability);
	add_property (_follow_count);
	add_property (_quantization);
	add_property (_legato);
	add_property (_name);
	add_property (_gain);
	add_property (_midi_velocity_effect);
	add_property (_stretchable);
	add_property (_isolated);
	add_property (_color);
}

void
Trigger::request_trigger_delete (Trigger* t)
{
	TriggerBox::worker->request_delete_trigger (t);
}

void
Trigger::set_pending (Trigger* t)
{
	Trigger* old = _pending.exchange (t);
	if (old && old != MagicClearPointerValue) {
		/* new pending trigger set before existing pending trigger was used */
		delete old;
	}
}

Trigger*
Trigger::swap_pending (Trigger* t)
{
	return _pending.exchange (t);
}

void
Trigger::set_use_follow (bool yn)
{
	_use_follow = yn;
	PropertyChanged (Properties::use_follow);
	_box.session().set_dirty();
}

void
Trigger::set_name (std::string const & str)
{
	_name = str;
	PropertyChanged (Properties::name);
	_box.session().set_dirty();
}

void
Trigger::set_scene_isolated (bool i)
{
	_isolated = i;
	PropertyChanged (ARDOUR::Properties::isolated);
	_box.session().set_dirty();
}

void
Trigger::set_color (color_t c)
{
	_color = c;
	PropertyChanged (ARDOUR::Properties::color);
	_box.session().set_dirty();
}

void
Trigger::set_stretchable (bool s)
{
	_stretchable = s;
	PropertyChanged (ARDOUR::Properties::stretchable);
	_box.session().set_dirty();
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
	if (_gain == g) {
		return;
	}

	_gain = g;
	PropertyChanged (Properties::gain);
	_box.session().set_dirty();
}

void
Trigger::set_midi_velocity_effect (float mve)
{
	if (_midi_velocity_effect == mve) {
		return;
	}

	_midi_velocity_effect = std::min (1.f, std::max (0.f, mve));
	PropertyChanged (Properties::velocity_effect);
	_box.session().set_dirty();
}

void
Trigger::set_follow_count (uint32_t n)
{
	if (_follow_count == n) {
		return;
	}

	_follow_count = n;
	PropertyChanged (Properties::follow_count);
	_box.session().set_dirty();
}

void
Trigger::set_follow_action (FollowAction f, uint32_t n)
{
	assert (n < 2);

	if (n == 0) {
		if (_follow_action0 == f) {
			return;
		}
		_follow_action0 = f;
		PropertyChanged (Properties::follow_action0);
	} else {
		if (_follow_action1 == f) {
			return;
		}
		_follow_action1 = f;
		PropertyChanged (Properties::follow_action1);
	}
	_box.session().set_dirty();
}

Trigger::LaunchStyle
Trigger::launch_style () const
{
	if (cue_launched) {
		return OneShot;
	}
	return _launch_style;
}

void
Trigger::set_launch_style (LaunchStyle l)
{
	if (_launch_style == l) {
		return;
	}

	_launch_style = l;

	PropertyChanged (Properties::launch_style);
	_box.session().set_dirty();
}

XMLNode&
Trigger::get_state (void)
{
	XMLNode* node = new XMLNode (X_("Trigger"));

	for (OwnedPropertyList::iterator i = _properties->begin(); i != _properties->end(); ++i) {
		i->second->get_value (*node);
	}

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
	/* Set region first since set_region_in_worker_thread() will set some
	   values that may/will need to be overridden by XML
	*/

	PBD::ID rid;

	node.get_property (X_("region"), rid);

	boost::shared_ptr<Region> r = RegionFactory::region_by_id (rid);

	if (r) {
		set_region (r, false);
	}

	set_values (node);

	node.get_property (X_("index"), _index);

	return 0;
}

void
Trigger::set_follow_length (Temporal::BBT_Offset const & bbo)
{
	_follow_length = bbo;
}

void
Trigger::set_legato (bool yn)
{
	_legato = yn;
	PropertyChanged (Properties::legato);
	_box.session().set_dirty();
}

void
Trigger::set_follow_action_probability (int n)
{
	if (_follow_action_probability == n) {
		return;
	}

	n = std::min (100, n);
	n = std::max (0, n);

	_follow_action_probability = n;
	PropertyChanged (Properties::follow_action_probability);
	_box.session().set_dirty();
}

void
Trigger::set_quantization (Temporal::BBT_Offset const & q)
{
	if (_quantization == q) {
		return;
	}

	_quantization = q;
	PropertyChanged (Properties::quantization);
	_box.session().set_dirty();
}

void
Trigger::set_region (boost::shared_ptr<Region> r, bool use_thread)
{
	/* Called from (G)UI thread */

	if (!r) {
		/* clear operation, no need to talk to the worker thread */
		set_pending (Trigger::MagicClearPointerValue);
		request_stop ();
	} else if (use_thread) {
		/* load data, do analysis in another thread */
		TriggerBox::worker->set_region (_box, index(), r);
	} else {
		set_region_in_worker_thread (r);
	}
}

void
Trigger::clear_region ()
{
	/* Called from RT process thread */

	_region.reset ();

	set_name("");
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
Trigger::request_stop ()
{
	_requests.stop = true;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 asked to stop\n", name()));
}

void
Trigger::startup()
{
	_state = WaitingToStart;
	_loop_cnt = 0;
	_velocity_gain = _pending_velocity_gain;
	_explicitly_stopped = false;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 starts up\n", name()));
	PropertyChanged (ARDOUR::Properties::running);
}

void
Trigger::shutdown (BufferSet& bufs, pframes_t dest_offset)
{
	_state = Stopped;
	cue_launched = false;
	_pending_velocity_gain = _velocity_gain = 1.0;
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
	/* XXX set expected_end_sample */
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 requested state %2\n", index(), enum_2_string (_state)));
	PropertyChanged (ARDOUR::Properties::running);
}

void
Trigger::jump_stop (BufferSet& bufs, pframes_t dest_offset)
{
	/* this is used when we start a new trigger in legato mode. We do not
	   wait for quantization.
	*/
	shutdown (bufs, dest_offset);
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 requested state %2\n", index(), enum_2_string (_state)));
	PropertyChanged (ARDOUR::Properties::running);
}

void
Trigger::begin_stop (bool explicit_stop)
{
	/* this is used when we start a tell a currently playing trigger to
	   stop, but wait for quantization first.
	*/
	_state = WaitingToStop;
	_explicitly_stopped = explicit_stop;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 requested state %2\n", index(), enum_2_string (_state)));
	PropertyChanged (ARDOUR::Properties::running);
}

void
Trigger::process_state_requests (BufferSet& bufs, pframes_t dest_offset)
{
	bool stop = _requests.stop.exchange (false);

	if (stop) {

		/* This is for an immediate stop, not a quantized one */

		if (_state != Stopped) {
			shutdown (bufs, dest_offset);
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 immediate stop implemented\n", name()));
		}

		/* Don't process bang/unbang requests since we're stopping */

		_bang = 0;
		_unbang = 0;

		return;
	}

	/* now check bangs/unbangs */

	int x;

	while ((x = _bang.load ())) {

		_bang.fetch_sub (1);

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 handling bang with state = %2\n", index(), enum_2_string (_state)));

		switch (_state) {
		case Running:
		case Playout:
			switch (launch_style()) {
			case OneShot:
				/* do nothing, just let it keep playing */
				break;
			case ReTrigger:
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 oneshot %2 => %3\n", index(), enum_2_string (Running), enum_2_string (WaitingForRetrigger)));
				_state = WaitingForRetrigger;
				PropertyChanged (ARDOUR::Properties::running);
				break;
			case Gate:
			case Toggle:
			case Repeat:
				if (_box.active_scene() >= 0) {
					std::cerr << "should not happen, cue launching but launch_style() said " << enum_2_string (launch_style()) << std::endl;
				} else {
					DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 gate/toggle/repeat => %3\n", index(), enum_2_string (Running), enum_2_string (WaitingToStop)));
					begin_stop (true);
				}
			}
			break;

		case Stopped:
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 stopped => %3\n", index(), enum_2_string (Stopped), enum_2_string (WaitingToStart)));
			_box.queue_explict (index());
			cue_launched = (_box.active_scene() >= 0);
			std::cerr << index() << " aka " << name() << " launched via cue ? " << cue_launched << std::endl;
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

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 unbanged\n", index()));

		switch (_state) {
		case Running:
		case Playout:
			begin_stop (true);
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 unbanged, now in WaitingToStop\n", index()));
			break;

		case Stopped:
		case Stopping: /* theoretically not possible */
		case WaitingToStop:
		case WaitingForRetrigger:
			/* do nothing */
			break;

		case WaitingToStart:
			/* didn't even get started */
			shutdown (bufs, dest_offset);
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 unbanged, never started, now stopped\n", index()));
		}
	}
}

void
Trigger::maybe_compute_next_transition (samplepos_t start_sample, Temporal::Beats const & start, Temporal::Beats const & end, pframes_t& nframes, pframes_t&  dest_offset, bool passthru)
{
	using namespace Temporal;

	/* This should never be called by a stopped trigger */

	assert (_state != Stopped);

	/* In these states, we are not waiting for a transition */

	if (_state == Running || _state == Stopping || _state == Playout) {
		/* will cover everything */
		return;
	}

	timepos_t transition_time (BeatTime);
	TempoMap::SharedPtr tmap (TempoMap::use());
	Temporal::BBT_Time transition_bbt;
	pframes_t extra_offset = 0;
	BBT_Offset q (_quantization);

	/* Clips don't stop on their own quantize; in Live they stop on the Global Quantize setting; we will choose 1 bar (Live's default) for now */
# warning when Global Quantize is implemented, use that instead of '1 bar' here
	if (_state == WaitingToStop) {
		q = BBT_Offset(1,0,0);
	}

	/* XXX need to use global grid here is quantization == zero */

	/* Given the value of @param start, determine, based on the
	 * quantization, the next time for a transition.
	 */

	if (q < Temporal::BBT_Offset (0, 0, 0)) {
		/* negative quantization == do not quantize */

		transition_samples = start_sample;
		transition_beats = start;
		transition_time = timepos_t (start);
	} else if (q.bars == 0) {
		Temporal::Beats transition_beats = start.round_up_to_multiple (Temporal::Beats (q.beats, q.ticks));
		transition_bbt = tmap->bbt_at (transition_beats);
		transition_time = timepos_t (transition_beats);
	} else {
		transition_bbt = tmap->bbt_at (timepos_t (start));
		transition_bbt = transition_bbt.round_up_to_bar ();
		/* bars are 1-based; 'every 4 bars' means 'on bar 1, 5, 9, ...' */
		transition_bbt.bars = 1 + ((transition_bbt.bars-1) / q.bars * q.bars);
		transition_time = timepos_t (tmap->quarters_at (transition_bbt));
	}

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 quantized with %5 transition at %2, sb %3 eb %4\n", index(), transition_time.beats(), start, end, q));

	/* See if this time falls within the range of time given to us */

	if (transition_time.beats() < start || transition_time > end) {

		/* retrigger time has not been reached, just continue
		   to play normally until then.
		*/

		return;
	}

	/* transition time has arrived! let's figure out what're doing:
	 * stopping, starting, retriggering
	 */

	transition_samples = transition_time.samples();
	transition_beats = transition_time.beats ();

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 in range, should start/stop at %2 aka %3\n", index(), transition_samples, transition_beats));

	switch (_state) {

	case WaitingToStop:
		_state = Stopping;
		PropertyChanged (ARDOUR::Properties::running);

		/* trigger will reach it's end somewhere within this
		 * process cycle, so compute the number of samples it
		 * should generate.
		 */

		nframes = transition_samples - start_sample;

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 will stop somewhere in the middle of run(), specifically at %2 (%3) vs expected end at %4\n", name(), transition_time, transition_time.beats(), expected_end_sample));

		/* offset within the buffer(s) for output remains
		   unchanged, since we will write from the first
		   location corresponding to start
		*/
		break;

	case WaitingToStart:
		retrigger ();
		_state = Running;
		set_expected_end_sample (tmap, transition_bbt, transition_samples);
		PropertyChanged (ARDOUR::Properties::running);

		/* trigger will start somewhere within this process
		 * cycle. Compute the sample offset where any audio
		 * should end up, and the number of samples it should generate.
		 */

		extra_offset = std::max (samplepos_t (0), transition_samples - start_sample);

		nframes -= extra_offset;
		dest_offset += extra_offset;

		if (!passthru) {
			/* XXX need to silence start of buffers up to dest_offset */
		}
		break;

	case WaitingForRetrigger:
		retrigger ();
		_state = Running;
		set_expected_end_sample (tmap, transition_bbt, transition_samples);
		PropertyChanged (ARDOUR::Properties::running);

		/* trigger is just running normally, and will fill
		 * buffers entirely.
		 */

		break;

	default:
		fatal << string_compose (_("programming error: %1"), "impossible trigger state in ::maybe_compute_next_transition()") << endmsg;
		abort();
	}

	return;
}

void
Trigger::when_stopped_during_run (BufferSet& bufs, pframes_t dest_offset)
{
	if (_state == Stopped || _state == Stopping) {

		if ((_state == Stopped) && !_explicitly_stopped && (launch_style() == Trigger::Gate || launch_style() == Trigger::Repeat)) {

			jump_start ();
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 was stopped, repeat/gate ret\n", index()));

		} else {

			if ((launch_style() != Repeat) && (launch_style() != Gate) && (_loop_cnt == _follow_count)) {

				/* have played the specified number of times, we're done */

				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 loop cnt %2 satisfied, now stopped\n", index(), _follow_count));
				shutdown (bufs, dest_offset);


			} else if (_state == Stopping) {

				/* did not reach the end of the data. Presumably
				 * another trigger was explicitly queued, and we
				 * stopped
				 */

				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 not at end, but ow stopped\n", index()));
				shutdown (bufs, dest_offset);

			} else {

				/* reached the end, but we haven't done that enough
				 * times yet for a follow action/stop to take
				 * effect. Time to get played again.
				 */

				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 was stopping, now waiting to retrigger, loop cnt %2 fc %3\n", index(), _loop_cnt, _follow_count));
				/* we will "restart" at the beginning of the
				   next iteration of the trigger.
				*/
				_state = WaitingToStart;
				retrigger ();
				PropertyChanged (ARDOUR::Properties::running);
			}
		}
	}
}


/*--------------------*/

AudioTrigger::AudioTrigger (uint32_t n, TriggerBox& b)
	: Trigger (n, b)
	, _stretcher (0)
	, _start_offset (0)
	, last_sample (0)
	, read_index (0)
	, process_index (0)
	, _legato_offset (0)
	, retrieved (0)
	, got_stretcher_padding (false)
	, to_pad (0)
	, to_drop (0)
{
}

AudioTrigger::~AudioTrigger ()
{
	drop_data ();
	delete _stretcher;
}

bool
AudioTrigger::stretching() const
{
	return (_apparent_tempo != .0) && _stretchable;
}

SegmentDescriptor
AudioTrigger::get_segment_descriptor () const
{
	SegmentDescriptor sd;

	sd.set_tempo (Temporal::Tempo (_apparent_tempo, 4));

	return sd;
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
AudioTrigger::jump_stop (BufferSet& bufs, pframes_t dest_offset)
{
	Trigger::jump_stop (bufs, dest_offset);
	retrigger ();
}

double
AudioTrigger::position_as_fraction () const
{
	if (!active()) {
		return 0.0;
	}

	return process_index / (double) (expected_end_sample - transition_samples);
}

XMLNode&
AudioTrigger::get_state (void)
{
	XMLNode& node (Trigger::get_state());

	node.set_property (X_("start"), timepos_t (_start_offset));

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
AudioTrigger::start_offset () const
{
	return timepos_t (_start_offset);
}

timepos_t
AudioTrigger::current_pos() const
{
	return timepos_t (process_index);
}

void
AudioTrigger::set_expected_end_sample (Temporal::TempoMap::SharedPtr const & tmap, Temporal::BBT_Time const & transition_bbt, samplepos_t transition_sample)
{
	/* Our task here is to set:

	   expected_end_sample: the sample position where the data for the clip should run out (taking stretch into account)
           last_sample: the sample in the data where we stop reading
           final_sample: the sample where the trigger stops and the follow action if any takes effect

           Things that affect these values:

           data.length : how many samples there are in the data  (AudioTime / samples)
           _follow_length : the time after the start of the trigger when the follow action should take effect
           _barcnt : the expected duration of the trigger, based on analysis of its tempo, or user-set

	*/

	samplepos_t end_by_follow_length = _follow_length != Temporal::BBT_Offset() ? tmap->sample_at (tmap->bbt_walk(transition_bbt, _follow_length)) : 0;
	samplepos_t end_by_barcnt = tmap->sample_at (tmap->bbt_walk(transition_bbt, Temporal::BBT_Offset (round (_barcnt), 0, 0)));
	samplepos_t end_by_data_length = transition_sample + data.length;

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 ends: FL %2 BC %3 DL %4\n", index(), end_by_follow_length, end_by_barcnt, end_by_data_length));

	if (stretching()) {
		if (end_by_follow_length) {
			expected_end_sample = std::min (end_by_follow_length, end_by_barcnt);
		} else {
			expected_end_sample = end_by_barcnt;
		}
	} else {
		if (end_by_follow_length) {
			expected_end_sample = std::min (end_by_follow_length, end_by_data_length);
		} else {
			expected_end_sample = end_by_data_length;
		}
	}

	if (end_by_follow_length) {
		final_sample = end_by_follow_length;
	} else {
		final_sample = expected_end_sample;
	}

	samplecnt_t usable_length;

	if (end_by_follow_length && (end_by_follow_length < end_by_data_length)) {
		usable_length = tmap->sample_at (tmap->bbt_walk (Temporal::BBT_Time (), _follow_length));
	} else {
		usable_length = data.length;
	}

	/* called from set_expected_end_sample() when we know the time (audio &
	 * musical time domains when we start starting. Our job here is to
	 * define the last_sample we can use as data.
	 */

	if (launch_style() != Repeat) {

		last_sample = _start_offset + usable_length;

	} else {

		/* This is for Repeat mode only deliberately ignore the _follow_length
		 * here, because we'll be playing just the quantization distance no
		 * matter what.
		 */

		Temporal::BBT_Offset q (_quantization);

		if (q == Temporal::BBT_Offset ()) {
			usable_length = data.length;
			last_sample = _start_offset + usable_length;
			return;
		}

		/* XXX MUST HANDLE BAR-LEVEL QUANTIZATION */

		timecnt_t len (Temporal::Beats (q.beats, q.ticks), timepos_t (Temporal::Beats()));
		usable_length = len.samples();
		last_sample = _start_offset + usable_length;
	}

	std::cerr << "final sample = " << final_sample << " vs expected end " << expected_end_sample << " last sample " << last_sample << std::endl;
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
		return timepos_t (data.length);
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
AudioTrigger::set_region_in_worker_thread (boost::shared_ptr<Region> r)
{
	assert (!active());

	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);

	if (r && !ar) {
		return -1;
	}

	set_region_internal (r);

	if (!r) {
		/* unset */
		return 0;
	}

	load_data (ar);
	determine_tempo ();
	setup_stretcher ();

	/* Given what we know about the tempo and duration, set the defaults
	 * for the trigger properties.
	 */

	if (_apparent_tempo == 0.) {
		_stretchable = false;
		_quantization = Temporal::BBT_Offset (-1, 0, 0);
		_follow_action0 = None;
	} else {

		if (probably_oneshot()) {
			/* short trigger, treat as a one shot */
			_stretchable = false;
			_follow_action0 = None;
			_quantization = Temporal::BBT_Offset (-1, 0, 0);
		} else {
			_stretchable = true;
			_quantization = Temporal::BBT_Offset (1, 0, 0);
			_follow_action0 = Again;
		}
	}

	_follow_action_probability = 0; /* 100% left */

	PropertyChanged (ARDOUR::Properties::name);

	return 0;
}

void
AudioTrigger::determine_tempo ()
{
	using namespace Temporal;

	/* check the name to see if there's a (heuristically obvious) hint
	 * about the tempo.
	 */

	string str = _region->name();
	string::size_type bi;
	string::size_type ni;
	double text_tempo = -1.;

	std::cerr << "Determine tempo for " << name() << std::endl;

	if (((bi = str.find ("bpm")) != string::npos) ||
	    ((bi = str.find ("BPM")) != string::npos)) {

		string sub (str.substr (0, bi));

		if ((ni = sub.find_last_of ("0123456789.,_-")) != string::npos) {

			int nni = ni; /* ni is unsigned, nni is signed */

			while (nni >= 0) {
				if (!isdigit (sub[nni]) &&
				    (sub[nni] != '.') &&
				    (sub[nni] != ',')) {
					break;
				}
				--nni;
			}

			if (nni > 0) {
				std::stringstream p (sub.substr (nni + 1));
				p >> text_tempo;
				if (!p) {
					text_tempo = -1.;
				} else {
					_apparent_tempo = text_tempo;
					std::cerr << "from filename, tempo = " << _apparent_tempo << std::endl;
				}
			}
		}
	}

	TempoMap::SharedPtr tm (TempoMap::use());

	/* We don't have too many good choices here. Triggers can fire at any
	 * time, so there's no special place on the tempo map that we can use
	 * to get the meter from and thus compute an apparent bar count for
	 * this region. Our solution for now: just use the first meter.
	 */

	TempoMetric const & metric (tm->metric_at (timepos_t (AudioTime)));

	if (text_tempo < 0) {

		breakfastquay::MiniBPM mbpm (_box.session().sample_rate());

		mbpm.setBPMRange (metric.tempo().quarter_notes_per_minute () * 0.75, metric.tempo().quarter_notes_per_minute() * 1.5);

		_apparent_tempo = mbpm.estimateTempoOfSamples (data[0], data.length);

		if (_apparent_tempo == 0.0) {
			/* no apparent tempo, just return since we'll use it as-is */
			std::cerr << "Could not determine tempo for " << name() << std::endl;
			return;
		}

		cerr << name() << " Estimated bpm " << _apparent_tempo << " from " << (double) data.length / _box.session().sample_rate() << " seconds\n";
	}

	const double seconds = (double) data.length  / _box.session().sample_rate();
	const double quarters = (seconds / 60.) * _apparent_tempo;
	_barcnt = quarters / metric.meter().divisions_per_bar();

	/* now check the determined tempo and force it to a value that gives us
	   an integer bar/quarter count. This is a heuristic that tries to
	   avoid clips that slightly over- or underrun a quantization point,
	   resulting in small or larger gaps in output if they are repeating.
	*/

	if ((_apparent_tempo != 0.) && (rint (_barcnt) != _barcnt)) {
		/* fractional barcnt */
		int intquarters = floor (quarters);
		double at = _apparent_tempo;
		_apparent_tempo = intquarters / (seconds/60.);
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("adjusted barcnt of %1 and q = %2 to %3, old %4 new at = %5\n", _barcnt, quarters, intquarters, at, _apparent_tempo));
	}

	/* use initial tempo in map (assumed for now to be the only one */

	const samplecnt_t one_bar = tm->bbt_duration_at (timepos_t (AudioTime), BBT_Offset (1, 0, 0)).samples();

	cerr << "tempo: " << _apparent_tempo << endl;
	cerr << "one bar in samples: " << one_bar << endl;
	cerr << "barcnt = " << round (_barcnt) << endl;
}

bool
AudioTrigger::probably_oneshot () const
{
	assert (_apparent_tempo != 0.);

	if ((data.length < (_box.session().sample_rate()/2)) ||
	    /* XXX use Meter here, not 4.0 */
	    ((_barcnt < 1) && (data.length < (4.0 * ((_box.session().sample_rate() * 60) / _apparent_tempo))))) {
		std::cerr << "looks like a one-shot\n";
		return true;
	}

	return false;
}

void
AudioTrigger::io_change ()
{
	if (_stretcher) {
		setup_stretcher ();
	}
}

/* This exists so that we can play with the value easily. Currently, 1024 seems as good as any */
static const samplecnt_t rb_blocksize = 1024;

void
AudioTrigger::setup_stretcher ()
{
	using namespace RubberBand;
	using namespace Temporal;

	if (!_region) {
		return;
	}

	boost::shared_ptr<AudioRegion> ar (boost::dynamic_pointer_cast<AudioRegion> (_region));
	const uint32_t nchans = std::min (_box.input_streams().n_audio(), ar->n_channels());

	/* XXX maybe get some of these options from region properties (when/if we have them) ? */

	RubberBandStretcher::Options options = RubberBandStretcher::Option (RubberBandStretcher::OptionProcessRealTime |
	                                                                    RubberBandStretcher::OptionTransientsCrisp);

	delete _stretcher;
	_stretcher = new RubberBandStretcher (_box.session().sample_rate(), nchans, options, 1.0, 1.0);

	_stretcher->setMaxProcessSize (rb_blocksize);
}

void
AudioTrigger::drop_data ()
{
	for (auto& d : data) {
		delete [] d;
	}
	data.clear ();
}

int
AudioTrigger::load_data (boost::shared_ptr<AudioRegion> ar)
{
	const uint32_t nchans = ar->n_channels();

	data.length = ar->length_samples();
	drop_data ();

	try {
		for (uint32_t n = 0; n < nchans; ++n) {
			data.push_back (new Sample[data.length]);
			ar->read (data[n], 0, data.length, n);
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
	process_index = 0;
	retrieved = 0;
	_legato_offset = 0; /* used one time only */
	_stretcher->reset ();
	got_stretcher_padding = false;
	to_pad = 0;
	to_drop = 0;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 retriggered to %2\n", _index, read_index));
}

pframes_t
AudioTrigger::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, Temporal::Beats const & start, Temporal::Beats const & end, pframes_t nframes, pframes_t dest_offset, bool passthru, double bpm)
{
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(_region);
	/* We do not modify the I/O of our parent route, so we process only min (bufs.n_audio(),region.channels()) */
	const uint32_t nchans = std::min (bufs.count().n_audio(), ar->n_channels());
	int avail = 0;
	BufferSet& scratch (_box.session().get_scratch_buffers (ChanCount (DataType::AUDIO, nchans)));
	std::vector<Sample*> bufp(nchans);
	const bool do_stretch = stretching();

	/* see if we're going to start or stop or retrigger in this run() call */
	maybe_compute_next_transition (start_sample, start, end, nframes, dest_offset, passthru);
	const pframes_t orig_nframes = nframes;

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 after checking for transition, state = %2, will stretch %3, nf will be %4\n", name(), enum_2_string (_state), do_stretch, nframes));

	switch (_state) {
	case Stopped:
	case WaitingForRetrigger:
	case WaitingToStart:
		/* did everything we could do */
		return nframes;
	case Running:
	case Playout:
	case WaitingToStop:
	case Stopping:
		/* stuff to do */
		break;
	}

	/* We use session scratch buffers for both padding the start of the
	 * input to RubberBand, and to hold the output. Because of this dual
	 * purpose, we use a generic variable name ('bufp') to refer to them.
	 */

	for (uint32_t chn = 0; chn < bufs.count().n_audio(); ++chn) {
		bufp[chn] = scratch.get_audio (chn).data();
	}

	/* tell the stretcher what we are doing for this ::run() call */

	if (do_stretch && _state != Playout) {

		const double stretch = _apparent_tempo / bpm;
		_stretcher->setTimeRatio (stretch);

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("clip tempo %1 bpm %2 ratio %3%4\n", _apparent_tempo, bpm, std::setprecision (6), stretch));

		if ((avail = _stretcher->available()) < 0) {
			error << _("Could not configure rubberband stretcher") << endmsg;
			return 0;
		}

		/* We are using Rubberband in realtime mode, but this mdoe of
		 * operation has some issues. The first is that it will
		 * generate a certain number of samples of output at the start
		 * that are not based on the input, due to processing latency.
		 *
		 * In this context, we don't care about this output, because we
		 * have all the data available from the outset, and we can just
		 * wait until this "latency" period is over. So we will feed
		 * an initial chunk of data to the stretcher, and then throw
		 * away the corresponding data on the output.
		 *
		 * This code is modelled on the code for rubberband(1), part of
		 * the rubberband software.
		 */

		if (!got_stretcher_padding) {
			to_pad = _stretcher->getLatency();
			to_drop = to_pad;
			got_stretcher_padding = true;
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 requires %2 padding %3\n", name(), to_pad));
		}

		while (to_pad > 0) {
			const samplecnt_t limit = std::min ((samplecnt_t) scratch.get_audio (0).capacity(), to_pad);
			for (uint32_t chn = 0; chn < nchans; ++chn) {
				for (samplecnt_t n = 0; n < limit; ++n) {
					bufp[chn][n] = 0.f;
				}
			}

			_stretcher->process (&bufp[0], limit, false);
			to_pad -= limit;
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 padded %2 left %3\n", name(), limit, to_pad));
		}
	}

	while (nframes && (_state != Playout)) {

		pframes_t to_stretcher;
		pframes_t from_stretcher;

		if (do_stretch) {

			if (read_index < last_sample) {

				/* still have data to push into the stretcher */

				to_stretcher = (pframes_t) std::min (samplecnt_t (rb_blocksize), (last_sample - read_index));
				const bool at_end = (to_stretcher < rb_blocksize);

				while ((pframes_t) avail < nframes && (read_index < last_sample)) {
					/* keep feeding the stretcher in chunks of "to_stretcher",
					 * until there's nframes of data available, or we reach
					 * the end of the region
					 */

					std::vector<Sample*> in(nchans);

					for (uint32_t chn = 0; chn < nchans; ++chn) {
						in[chn] = data[chn] + read_index;
					}

					/* Note: RubberBandStretcher's process() and retrieve() API's accepts Sample**
					 * as their first argument. This code may appear to only be processing the first
					 * channel, but actually processes them all in one pass.
					 */

					_stretcher->process (&in[0], to_stretcher, at_end);
					read_index += to_stretcher;
					avail = _stretcher->available ();

					if (to_drop && avail) {
						samplecnt_t this_drop = std::min (std::min ((samplecnt_t) avail, to_drop), (samplecnt_t) scratch.get_audio (0).capacity());
						_stretcher->retrieve (&bufp[0], this_drop);
						to_drop -= this_drop;
						avail = _stretcher->available ();
					}

					DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 process %2 at-end %3 avail %4 of %5\n", name(), to_stretcher, at_end, avail, nframes));
				}

				/* we've fed the stretcher enough data to have
				 * (at least) nframes of output available.
				 */

				from_stretcher = nframes;

			} else {

				/* finished delivering data to stretcher, but may have not yet retrieved it all */
				avail = _stretcher->available ();
				from_stretcher = (pframes_t) std::min ((pframes_t) nframes, (pframes_t) avail);
			}

			/* fetch the stretch */

			retrieved += _stretcher->retrieve (&bufp[0], from_stretcher);

			if (read_index >= last_sample) {

				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 no more data to deliver to stretcher, but retrieved %2 to put current end at %3 vs %4 / %5 pi %6\n",
				                                              index(), retrieved, transition_samples + retrieved, expected_end_sample, final_sample, process_index));

				if (transition_samples + retrieved > expected_end_sample) {
					/* final pull from stretched data into output buffers */
					from_stretcher = std::min ((samplecnt_t) from_stretcher, expected_end_sample - process_index);

					DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 total retrieved data %2 exceeds theoretical size %3, truncate from_stretcher to %4\n",
					                                              index(), retrieved, expected_end_sample - transition_samples, from_stretcher));

					if (from_stretcher == 0) {

						if (process_index < final_sample) {
							DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached (EX) end, entering playout mode to cover %2 .. %3\n", index(), process_index, final_sample));
							_state = Playout;
						} else {
							DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached (EX) end, now stopped, retrieved %2, avail %3 pi %4 vs fs %5\n", index(), retrieved, avail, process_index, final_sample));
							_state = Stopped;
							_loop_cnt++;
						}

						break;
					}

				}
			}

		} else {
			/* no stretch */
			from_stretcher = (pframes_t) std::min ((samplecnt_t) nframes, (last_sample - read_index));
		}

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 ready with %2 ri %3 ls %4, will write %5\n", name(), avail, read_index, last_sample, from_stretcher));

		/* deliver to buffers */

		for (uint32_t chn = 0; chn < bufs.count().n_audio(); ++chn) {

			uint32_t channel = chn %  data.size();
			AudioBuffer& buf (bufs.get_audio (chn));
			Sample* src = do_stretch ? bufp[channel] : (data[channel] + read_index);

			gain_t gain = _velocity_gain * _gain;  //incorporate the gain from velocity_effect

			if (!passthru) {
				buf.read_from (src, from_stretcher, dest_offset);
				if (gain != 1.0f) {
					buf.apply_gain (gain, from_stretcher);
				}
			} else {
				if (gain != 1.0f) {
					buf.accumulate_with_gain_from (src, from_stretcher, gain, dest_offset);
				} else {
					buf.accumulate_from (src, from_stretcher, dest_offset);
				}
			}
		}

		process_index += from_stretcher;
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 pi grew by %2 to %3\n", index(), from_stretcher, process_index));

		/* Move read_index, in the case that we are not using a
		 * stretcher
		 */

		if (!do_stretch) {
			read_index += from_stretcher;
		}

		nframes -= from_stretcher;
		avail = _stretcher->available ();
		dest_offset += from_stretcher;

		if (read_index >= last_sample && (!do_stretch || avail <= 0)) {

			if (process_index < final_sample) {
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end, entering playout mode to cover %2 .. %3\n", index(), process_index, final_sample));
				_state = Playout;
			} else {
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end, now stopped, retrieved %2, avail %3\n", index(), retrieved, avail));
				_state = Stopped;
				_loop_cnt++;
			}
			break;
		}
	}


	pframes_t covered_frames =  orig_nframes - nframes;

	if (_state == Playout) {

		if (nframes != orig_nframes) {
			/* we've already taken dest_offset into account, it plays no
			   role in a "playout" during the same ::run() call
			*/
			dest_offset = 0;
		}

		const pframes_t remaining_frames_for_run= orig_nframes - covered_frames;
		const pframes_t remaining_frames_till_final = (final_sample - transition_samples) - process_index;
		const pframes_t to_fill = std::min (remaining_frames_till_final, remaining_frames_for_run);

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 playout mode, remaining in run %2 till final %3 @ %5 ts %7 vs pi @ %6 to fill %4\n",
		                                              index(), remaining_frames_for_run, remaining_frames_till_final, to_fill, final_sample, process_index, transition_samples));

		if (remaining_frames_till_final != 0) {

			for (uint32_t chn = 0; chn < bufs.count().n_audio(); ++chn) {

				AudioBuffer& buf (bufs.get_audio (chn));
				buf.silence (to_fill, covered_frames + dest_offset);
			}

			process_index += to_fill;
			covered_frames += to_fill;

			if (process_index < final_sample) {
				/* more playout to be done */
				return covered_frames;
			}
		}

		_state = Stopped;
		_loop_cnt++;
	}

	if (_state == Stopped || _state == Stopping) {
		/* note: neither argument is used in the audio case */
		when_stopped_during_run (bufs, dest_offset);
	}

	return covered_frames;
}

void
AudioTrigger::reload (BufferSet&, void*)
{
}

/*--------------------*/

MIDITrigger::MIDITrigger (uint32_t n, TriggerBox& b)
	: Trigger (n, b)
	, data_length (Temporal::Beats ())
	, last_event_beats (Temporal::Beats ())
	, _start_offset (0, 0, 0)
	, _legato_offset (0, 0, 0)
{
}

MIDITrigger::~MIDITrigger ()
{
}

bool
MIDITrigger::probably_oneshot () const
{
	/* XXX fix for short chord stabs */
	return false;
}

void
MIDITrigger::set_expected_end_sample (Temporal::TempoMap::SharedPtr const & tmap, Temporal::BBT_Time const & transition_bbt, samplepos_t)
{
	expected_end_sample = tmap->sample_at (tmap->bbt_walk(transition_bbt, Temporal::BBT_Offset (round (_barcnt), 0, 0)));

	Temporal::Beats usable_length; /* using timestamps from data */

	if (launch_style() != Repeat) {

		usable_length = data_length;

	} else {

		Temporal::BBT_Offset q (_quantization);

		if (q == Temporal::BBT_Offset ()) {
			usable_length = data_length;
			return;
		}

		/* XXX MUST HANDLE BAR-LEVEL QUANTIZATION */

		timecnt_t len (Temporal::Beats (q.beats, q.ticks), timepos_t (Temporal::Beats()));
		usable_length = len.beats ();
	}
}

SegmentDescriptor
MIDITrigger::get_segment_descriptor () const
{
	SegmentDescriptor sd;
	boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion> (_region);
	assert (mr);

	sd.set_extent (Temporal::Beats(), mr->length().beats());

	/* we don't really have tempo information for MIDI yet */
	sd.set_tempo (Temporal::Tempo (120, 4));

	return sd;
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
MIDITrigger::shutdown (BufferSet& bufs, pframes_t dest_offset)
{
	Trigger::shutdown (bufs, dest_offset);
	MidiBuffer& mb (bufs.get_midi (0));
	tracker.resolve_notes (mb, dest_offset);
}

void
MIDITrigger::jump_stop (BufferSet& bufs, pframes_t dest_offset)
{
	Trigger::jump_stop (bufs, dest_offset);

	MidiBuffer& mb (bufs.get_midi (0));
	tracker.resolve_notes (mb, dest_offset);

	retrigger ();
}

double
MIDITrigger::position_as_fraction () const
{
	if (!active()) {
		return 0.0;
	}

	Temporal::DoubleableBeats db (last_event_beats);

	double dl = db.to_double ();
	double dr = data_length.to_double ();

	return dl / dr;
}

XMLNode&
MIDITrigger::get_state (void)
{
	XMLNode& node (Trigger::get_state());

	node.set_property (X_("start"), start_offset());

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
	return timepos_t (last_event_beats);
}

void
MIDITrigger::set_length (timecnt_t const & newlen)
{

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
MIDITrigger::set_region_in_worker_thread (boost::shared_ptr<Region> r)
{
	boost::shared_ptr<MidiRegion> mr = boost::dynamic_pointer_cast<MidiRegion> (r);

	if (!mr) {
		return -1;
	}

	set_region_internal (r);
	set_name (mr->name());
	data_length = mr->length().beats();
	set_length (mr->length());
	model = mr->model ();

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 loaded midi region, span is %2\n", name(), data_length));

	PropertyChanged (ARDOUR::Properties::name);

	return 0;
}

void
MIDITrigger::retrigger ()
{
	/* XXX need to deal with bar offsets */
	// const Temporal::BBT_Offset o = _start_offset + _legato_offset;
	iter = model->begin();
	_legato_offset = Temporal::BBT_Offset ();
	last_event_beats = Temporal::Beats();
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 retriggered to %2, ts = %3\n", _index, iter->time(), transition_beats));
}

void
MIDITrigger::reload (BufferSet&, void*)
{
}

pframes_t
MIDITrigger::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample,
                  Temporal::Beats const & start_beats, Temporal::Beats const & end_beats,
                  pframes_t nframes, pframes_t dest_offset, bool passthru, double bpm)
{
	MidiBuffer& mb (bufs.get_midi (0));
	typedef Evoral::Event<MidiModel::TimeType> MidiEvent;
	const timepos_t region_start_time = _region->start();
	const Temporal::Beats region_start = region_start_time.beats();
	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());
	samplepos_t last_event_samples = max_samplepos;

	/* see if we're going to start or stop or retrigger in this run() call */
	maybe_compute_next_transition (start_sample, start_beats, end_beats, nframes, dest_offset, passthru);
	const pframes_t orig_nframes = nframes;

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 after checking for transition, state = %2\n", name(), enum_2_string (_state)));

	switch (_state) {
	case Stopped:
	case WaitingForRetrigger:
	case WaitingToStart:
		return nframes;
	case Running:
	case Playout:
	case WaitingToStop:
	case Stopping:
		break;
	}

	if (!passthru) {
		mb.clear ();
	}

	while (iter != model->end()) {

		MidiEvent const & next_event (*iter);

		/* Event times are in beats, relative to start of source
		 * file. We need to convert to region-relative time, and then
		 * a session timeline time, which is defined by the time at
		 * which we last transitioned (in this case, to being active)
		 */

		const Temporal::Beats effective_time = transition_beats + (next_event.time() - region_start);

		/* Now get samples */

		const samplepos_t timeline_samples = tmap->sample_at (effective_time);

		if (timeline_samples >= end_sample) {
			break;
		}

		/* Now we have to convert to a position within the buffer we
		 * are writing to.
		 *
		 * start_sample has already been been adjusted to reflect a
		 * previous Trigger's processing during this run cycle, so we
		 * can ignore dest_offset (which is necessary for audio
		 * triggers where the data is a continuous data stream, but not
		 * required here).
		 */

		samplepos_t buffer_samples = timeline_samples - start_sample;
		last_event_samples = timeline_samples;

		const Evoral::Event<MidiBuffer::TimeType> ev (Evoral::MIDI_EVENT, buffer_samples, next_event.size(), const_cast<uint8_t*>(next_event.buffer()), false);
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("given et %1 TS %7 rs %8 ts %2 bs %3 ss %4 do %5, inserting %6\n", effective_time, timeline_samples, buffer_samples, start_sample, dest_offset, ev, transition_beats, region_start));
		mb.insert_event (ev);
		tracker.track (next_event.buffer());
		last_event_beats = next_event.time();

		++iter;
	}


	if (_state == Stopping) {
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 was stopping, now stopped\n", index()));
		tracker.resolve_notes (mb, nframes-1);
	}

	if (iter == model->end()) {

		/* We reached the end */

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end\n", index()));

		_loop_cnt++;
		_state = Stopped;

		/* the time we processed spans from start to the last event */

		if (last_event_samples != max_samplepos) {
			nframes = (last_event_samples - start_sample);
		} else {
			/* all frames covered */
			nframes = 0;
		}
	} else {
		/* we didn't reach the end of the MIDI data, ergo we covered
		   the entire timespan passed into us.
		*/
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 did not reach end, nframes left at %2\n", index(), nframes));
		nframes = 0;
	}

	if (_state == Stopped || _state == Stopping) {
		when_stopped_during_run (bufs, dest_offset);
	}

	return orig_nframes - nframes;
}

/**************/

void
Trigger::make_property_quarks ()
{
	Properties::running.property_id = g_quark_from_static_string (X_("running"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for running = %1\n", Properties::running.property_id));
	Properties::passthru.property_id = g_quark_from_static_string (X_("passthru"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for passthru = %1\n", Properties::passthru.property_id));
	Properties::follow_count.property_id = g_quark_from_static_string (X_("follow-count"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for follow_count = %1\n", Properties::follow_count.property_id));
	Properties::legato.property_id = g_quark_from_static_string (X_("legato"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for legato = %1\n", Properties::legato.property_id));
	Properties::velocity_effect.property_id = g_quark_from_static_string (X_("velocity-effect"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for velocity_effect = %1\n", Properties::velocity_effect.property_id));
	Properties::follow_action_probability.property_id = g_quark_from_static_string (X_("follow-action-probability"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for follow_action_probability = %1\n", Properties::follow_action_probability.property_id));
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
	Properties::gain.property_id = g_quark_from_static_string (X_("gain"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for gain = %1\n", Properties::gain.property_id));
	Properties::stretchable.property_id = g_quark_from_static_string (X_("stretchable"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for stretchable = %1\n", Properties::stretchable.property_id));
	Properties::isolated.property_id = g_quark_from_static_string (X_("isolated"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for isolated = %1\n", Properties::isolated.property_id));
}

const int32_t TriggerBox::default_triggers_per_box = 8;
Temporal::BBT_Offset TriggerBox::_assumed_trigger_duration (4, 0, 0);
//TriggerBox::TriggerMidiMapMode TriggerBox::_midi_map_mode (TriggerBox::AbletonPush);
TriggerBox::TriggerMidiMapMode TriggerBox::_midi_map_mode (TriggerBox::SequentialNote);
int TriggerBox::_first_midi_note = 60;
std::atomic<int> TriggerBox::active_trigger_boxes (0);
TriggerBoxThread* TriggerBox::worker = 0;

void
TriggerBox::init ()
{
	worker = new TriggerBoxThread;
	TriggerBoxThread::init_request_pool ();
	init_pool ();
}

TriggerBox::TriggerBox (Session& s, DataType dt)
	: Processor (s, _("TriggerBox"), Temporal::BeatTime)
	, _data_type (dt)
	, _order (-1)
	, explicit_queue (64)
	, _currently_playing (0)
	, _stop_all (false)
	, _pass_thru (false)
	, _active_scene (-1)
	, requests (1024)
{
	set_display_to_user (false);

	/* default number of possible triggers. call ::add_trigger() to increase */

	if (_data_type == DataType::AUDIO) {
		for (uint32_t n = 0; n < default_triggers_per_box; ++n) {
			all_triggers.push_back (boost::make_shared<AudioTrigger> (n, *this));
		}
	} else {
		for (uint32_t n = 0; n < default_triggers_per_box; ++n) {
			all_triggers.push_back (boost::make_shared<MIDITrigger> (n, *this));
		}
	}

	while (pending.size() < all_triggers.size()) {
		pending.push_back (std::atomic<Trigger*>(0));
	}

	Config->ParameterChanged.connect_same_thread (*this, boost::bind (&TriggerBox::parameter_changed, this, _1));
}

void
TriggerBox::set_region (uint32_t slot, boost::shared_ptr<Region> region)
{
	/* This is called from our worker thread */

	Trigger* t;

	switch (_data_type) {
	case DataType::AUDIO:
		t = new AudioTrigger (slot, *this);
		break;
	case DataType::MIDI:
		t = new MIDITrigger (slot, *this);
		break;
	default:
		return;
	}

	t->set_region_in_worker_thread (region);

	/* XXX what happens if pending is already set? */

	set_pending (slot, t);
}

void
TriggerBox::set_pending (uint32_t slot, Trigger* t)
{
	all_triggers[slot]->set_pending (t);
}

void
TriggerBox::maybe_swap_pending (uint32_t slot)
{
	/* This is called synchronously with process() (i.e. in an RT process
	   thread) and so it is impossible for any Triggers in this TriggerBox
	   to be invoked while this executes.
	*/

	Trigger* p = 0;

	p = all_triggers[slot]->swap_pending (p);

	if (p) {

		if (p == Trigger::MagicClearPointerValue) {
			all_triggers[slot]->clear_region ();
		} else {
			/* Note use of a custom delete function. We cannot
			   delete the old trigger from the RT context where the
			   trigger swap will happen, so we will ask the trigger
			   helper thread to take care of it.
			*/
			all_triggers[slot].reset (p, Trigger::request_trigger_delete);
			TriggerSwapped (slot); /* EMIT SIGNAL */
		}
	}
}

void
TriggerBox::set_order (int32_t n)
{
	_order = n;
}

void
TriggerBox::queue_explict (uint32_t n)
{
	assert (n < all_triggers.size());
	explicit_queue.write (&n, 1);
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("explicit queue %1, EQ = %2\n", n, explicit_queue.read_space()));

	if (_currently_playing) {
		_currently_playing->unbang ();
	}
}

TriggerPtr
TriggerBox::get_next_trigger ()
{
	uint32_t n;

	if (explicit_queue.read (&n, 1) == 1) {
		TriggerPtr r = trigger (n);
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("next trigger from explicit queue = %1\n", r->index()));
		return r;
	}
	return 0;
}

void
TriggerBox::set_from_selection (uint32_t slot, boost::shared_ptr<Region> region)
{
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("load %1 into %2\n", region->name(), slot));

	if (slot >= all_triggers.size()) {
		return;
	}

	all_triggers[slot]->set_region (region);
}

void
TriggerBox::set_from_path (uint32_t slot, std::string const & path)
{
	if (slot >= all_triggers.size()) {
		return;
	}

	const DataType source_type = SMFSource::safe_midi_file_extension (path) ? DataType::MIDI : DataType::AUDIO;

	if (source_type != _data_type) {
		error << string_compose (_("Cannot use %1 files in %2 slots"),
		                         ((source_type == DataType::MIDI) ? "MIDI" : "audio"),
		                         ((source_type == DataType::MIDI) ? "audio" : "MIDI")) << endmsg;
		return;
	}

	try {
		ImportStatus status;

		status.total = 1;
		status.quality = SrcBest;
		status.freeze = false;
		status.paths.push_back (path);
		status.replace_existing_source = false;
		status.split_midi_channels = false;
		status.midi_track_name_source = ARDOUR::SMFTrackNumber;

		_session.import_files (status);

		if (status.cancel) {
			error << string_compose (_("Cannot create source from %1"), path) << endmsg;
			return;
		}

		if (status.sources.empty()) {
			error << string_compose (_("Could not create source from %1"), path) << endmsg;
			return;
		}

		SourceList src_list;

		for (auto& src : status.sources) {
			src_list.push_back (src);
		}

		PropertyList plist;

		plist.add (Properties::start, 0);
		plist.add (Properties::length, src_list.front()->length ());
		plist.add (Properties::name, basename_nosuffix (path));
		plist.add (Properties::layer, 0);
		plist.add (Properties::layering_index, 0);

		boost::shared_ptr<Region> the_region (RegionFactory::create (src_list, plist, true));

		all_triggers[slot]->set_region (the_region);

	} catch (std::exception& e) {
		cerr << "loading sample from " << path << " failed: " << e.what() << endl;
		return;
	}
}

TriggerBox::~TriggerBox ()
{
}

void
TriggerBox::stop_all_immediately ()
{
	_requests.stop_all = true;
}

void
TriggerBox::clear_all_triggers ()
{
	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->set_region (boost::shared_ptr<Region>());
	}
}

void
TriggerBox::set_all_launch_style (ARDOUR::Trigger::LaunchStyle ls)
{
	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->set_launch_style (ls);
	}
}

void
TriggerBox::set_all_follow_action (ARDOUR::Trigger::FollowAction fa, uint32_t fa_n)
{
	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->set_follow_action (fa, fa_n);
	}
}

void
TriggerBox::set_all_probability (int zero_to_hundred)
{
	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->set_follow_action_probability (zero_to_hundred);
	}
}

void
TriggerBox::set_all_quantization (Temporal::BBT_Offset const& q)
{
	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->set_quantization (q);
	}
}

void
TriggerBox::stop_all ()
{
	/* Stops all triggers as soon as possible */

	/* XXX needs to be done with mutex or via thread-safe queue */

	DEBUG_TRACE (DEBUG::Triggers, "stop-all request received\n");

	for (uint32_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->request_stop ();
	}

	_stop_all = true;

	explicit_queue.reset ();
}

void
TriggerBox::stop_all_quantized ()
{
	for (uint32_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->unbang ();
	}
}

void
TriggerBox::drop_triggers ()
{
	Glib::Threads::RWLock::WriterLock lm (trigger_lock);
	all_triggers.clear ();
}

TriggerPtr
TriggerBox::trigger (Triggers::size_type n)
{
	Glib::Threads::RWLock::ReaderLock lm (trigger_lock);

	if (n >= all_triggers.size()) {
		return 0;
	}

	return all_triggers[n];
}

void
TriggerBox::add_midi_sidechain ()
{
	assert (owner());
	if (!_sidechain) {
		_sidechain.reset (new SideChain (_session, string_compose ("%1/%2", owner()->name(), name ())));
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

void
TriggerBox::update_sidechain_name ()
{
	if (!_sidechain) {
		return;
	}
	assert (owner());
	_sidechain->set_name (string_compose ("%1/%2", owner()->name(), name ()));
}

bool
TriggerBox::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	/* if this is an audio trigger, let it be known that we have at least 1 audio output.
	*/
	if (_data_type == DataType::AUDIO) {
		out.set_audio (std::max (in.n_audio(), 1U));
	}
	/* if this is a MIDI trigger, let it be known that we have at least 1 MIDI output.
	*/
	if (_data_type == DataType::MIDI) {
		out.set_midi (std::max (in.n_midi(), 1U));
	}
	return true;
}

bool
TriggerBox::configure_io (ChanCount in, ChanCount out)
{
	if (_sidechain) {
		_sidechain->configure_io (in, out);
	}

	bool ret = Processor::configure_io (in, out);

	if (ret) {
		for (uint32_t n = 0; n < all_triggers.size(); ++n) {
			all_triggers[n]->io_change ();
		}
	}
	return ret;
}

void
TriggerBox::add_trigger (TriggerPtr trigger)
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

			if (trigger_number >= (int) all_triggers.size()) {
				continue;
			}

			TriggerPtr t = all_triggers[trigger_number];

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
					t->set_velocity_gain (1.0 - (t->midi_velocity_effect() * (*ev).velocity() / 127.f));
				}
				t->bang ();

			} else if ((*ev).is_note_off()) {

				t->unbang ();
			}
		}
	}
}

void
TriggerBox::set_pass_thru (bool yn)
{
	_requests.pass_thru = yn;
	PropertyChanged (Properties::passthru);
}

void
TriggerBox::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required)
{
	/* XXX a test to check if we have no usable slots would be good
	   here. if so, we can just return.
	*/

	/* STEP ONE: are we actually active? */

	if (!check_active()) {
		return;
	}

#ifndef NDEBUG
	{
		Temporal::TempoMap::SharedPtr __tmap (Temporal::TempoMap::use());
		const Temporal::Beats __start_beats (timepos_t (start_sample).beats());
		const Temporal::Beats __end_beats (timepos_t (end_sample).beats());
		const double __bpm = __tmap->quarters_per_minute_at (timepos_t (__start_beats));

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("**** Triggerbox::run() for %6, ss %1 es %2 sb %3 eb %4 bpm %5\n", start_sample, end_sample, __start_beats, __end_beats, __bpm, order()));
	}
#endif

	_pass_thru = _requests.pass_thru.load ();
	bool allstop = _requests.stop_all.exchange (false);

	/* STEP TWO: if latency compensation tells us that we haven't really
	 * started yet, do nothing, because we can't make sense of a negative
	 * start sample time w.r.t the tempo map.
	 */

	if (start_sample < 0) {
		return;
	}

	const pframes_t orig_nframes = nframes;

	/* STEP THREE: triggers in audio tracks need a MIDI sidechain to be
	 * able to receive inbound MIDI for triggering etc. This needs to run
	 * before anything else, since we may need data just received to launch
	 * a trigger (or stop it)
	 */

	if (_sidechain) {
		_sidechain->run (bufs, start_sample, end_sample, speed, nframes, true);
	}

	int32_t cue_bang = _session.first_cue_within (start_sample, end_sample);
	if (cue_bang >= 0) {
		std::cerr << " CUE BANG " << cue_bang << std::endl;
		_active_scene = cue_bang;
	}

	/* STEP SIX: if at this point there is an active cue, make it trigger
	 * our corresponding slot
	 */

	if (_active_scene >= 0) {
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("tb noticed active scene %1\n", _active_scene));
		if (_active_scene < (int32_t) all_triggers.size()) {
			if (!all_triggers[_active_scene]->scene_isolated()) {
				all_triggers[_active_scene]->bang ();
			}
		}
	}

	/* STEP FOUR: handle any incoming requests from the GUI or other
	 * non-MIDI UIs
	 */

	process_requests (bufs);

	/* STEP FIVE: handle any incoming MIDI requests
	 */

	process_midi_trigger_requests (bufs);

	/* STEP SEVEN: let each slot process any individual state requests
	 */

	std::vector<uint32_t> to_run;

	for (uint32_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->process_state_requests (bufs, nframes - 1);
	}

	/* cue handling is over at this point, reset _active_scene to reflect this */

	_active_scene = -1;

	if (_currently_playing && _currently_playing->state() == Trigger::Stopped) {
		_currently_playing = 0;
	}

	for (uint32_t n = 0; n < all_triggers.size(); ++n) {
		if (all_triggers[n] != _currently_playing) {
			maybe_swap_pending (n);
		}
	}

	/* STEP EIGHT: if there is no active slot, see if there any queued up
	 */

	if (!_currently_playing && !allstop) {
		if ((_currently_playing = get_next_trigger()) != 0) {
			maybe_swap_pending (_currently_playing->index());
			_currently_playing->startup ();
			PropertyChanged (Properties::currently_playing);
			active_trigger_boxes.fetch_add (1);
		}
	}

	/* STEP NINE: if we've been told to stop all slots, do so
	 */

	if (allstop) {
		stop_all ();
	}

	/* STEP TEN: nothing to do?
	 */

	if (!_currently_playing) {
		return;
	}

	/* transport must be active for triggers */

	if (!_session.transport_state_rolling() && !allstop) {
		_session.start_transport_from_trigger ();
	}

	/* now get the information we need related to the tempo map and the
	 * timeline
	 */

	const Temporal::Beats end_beats (timepos_t (end_sample).beats());
	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());
	uint32_t max_chans = 0;
	TriggerPtr nxt;
	pframes_t dest_offset = 0;

	while (nframes) {

		/* start can move if we have to switch triggers in mid-process cycle */

		const Temporal::Beats start_beats (timepos_t (start_sample).beats());
		const double bpm = tmap->quarters_per_minute_at (timepos_t (start_beats));

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("nf loop, ss %1 es %2 sb %3 eb %4 bpm %5\n", start_sample, end_sample, start_beats, end_beats, bpm));

		 /* see if there's another trigger explicitly queued */

		RingBuffer<uint32_t>::rw_vector rwv;
		explicit_queue.get_read_vector (&rwv);

		if (rwv.len[0] > 0) {

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("explicit queue rvec %1 + %2\n", rwv.len[0], rwv.len[1]));

			/* peek at it without dequeing it */

			uint32_t n = *(rwv.buf[0]);
			nxt = trigger (n);

			/* if user triggered same clip, that will have been handled as
			 * it processed bang requests. Nothing to do here otherwise.
			 */

			if (nxt != _currently_playing) {

				/* user has triggered a different slot than the currently waiting-to-play or playing slot */

				if (nxt->legato()) {
					/* We want to start this trigger immediately, without
					 * waiting for quantization points, and it should start
					 * playing at the same internal offset as the current
					 * trigger.
					 */

					explicit_queue.increment_read_idx (1); /* consume the entry we peeked at */

					nxt->set_legato_offset (_currently_playing->current_pos());

					/* starting up next trigger, check for pending */

					maybe_swap_pending (n);
					nxt = trigger (n);

					nxt->jump_start ();
					_currently_playing->jump_stop (bufs, dest_offset);
					/* and switch */
					DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 => %2 switched to in legato mode\n", _currently_playing->index(), nxt->index()));
					_currently_playing = nxt;
					PropertyChanged (Properties::currently_playing);

				} else {

					/* no legato-switch */

					if (_currently_playing->state() == Trigger::Stopped) {

						explicit_queue.increment_read_idx (1); /* consume the entry we peeked at */

						/* starting up next trigger, check for pending */
						maybe_swap_pending (n);
						nxt = trigger (n);

						nxt->startup ();
						DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 was finished, started %2\n", _currently_playing->index(), nxt->index()));
						_currently_playing = nxt;
						PropertyChanged (Properties::currently_playing);

					} else if (_currently_playing->state() != Trigger::WaitingToStop) {

						/* but just begin stoppingthe currently playing slot */
						_currently_playing->begin_stop ();
						DEBUG_TRACE (DEBUG::Triggers, string_compose ("start stop for %1 before switching to %2\n", _currently_playing->index(), nxt->index()));

					}
				}
			}
		}

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("currently playing: %1, state now %2\n", _currently_playing->name(), enum_2_string (_currently_playing->state())));

		/* if we're not in the process of stopping all active triggers,
		 * but the current one has stopped, decide which (if any)
		 * trigger to play next.
		 */

		if (_currently_playing->state() == Trigger::Stopped) {

			if (!_stop_all && !_currently_playing->explicitly_stopped()) {

				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 has stopped, need next...\n", _currently_playing->name()));

				if (_currently_playing->use_follow()) {
					int n = determine_next_trigger (_currently_playing->index());
					std::cerr << "dnt = " << n << endl;
					if (n < 0) {
						DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 finished, no next trigger\n", _currently_playing->name()));
						_currently_playing = 0;
						PropertyChanged (Properties::currently_playing);
						break; /* no triggers to come next, break out of nframes loop */
					}
					DEBUG_TRACE (DEBUG::Triggers, string_compose ("switching to next trigger %1\n", _currently_playing->name()));
					_currently_playing = all_triggers[n];
					_currently_playing->startup ();
					PropertyChanged (Properties::currently_playing);
				} else {
					_currently_playing = 0;
					PropertyChanged (Properties::currently_playing);
					DEBUG_TRACE (DEBUG::Triggers, "currently playing was stopped, but stop_all was set, leaving nf loop\n");
					/* leave nframes loop */
					break;
				}

			} else {

				_currently_playing = 0;
				PropertyChanged (Properties::currently_playing);
				DEBUG_TRACE (DEBUG::Triggers, "currently playing was stopped, but stop_all was set, leaving nf loop\n");
				/* leave nframes loop */
				break;
			}
		}

		pframes_t frames_covered;


		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (_currently_playing->region());
		if (ar) {
			max_chans = std::max (ar->n_channels(), max_chans);
		}

		frames_covered = _currently_playing->run (bufs, start_sample, end_sample, start_beats, end_beats, nframes, dest_offset, _pass_thru, bpm);

		nframes -= frames_covered;
		start_sample += frames_covered;
		dest_offset += frames_covered;

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("trig %1 ran, covered %2 state now %3 nframes now %4\n",
		                                              _currently_playing->name(), frames_covered, enum_2_string (_currently_playing->state()), nframes));

	}

	if (nframes && !_pass_thru) {

		/* didn't cover the entire nframes worth of the buffer, and not
		 * doing pass thru, so silence whatever is left.
		 */

		for (uint32_t chn = 0; chn < bufs.count().n_audio(); ++chn) {
			AudioBuffer& buf (bufs.get_audio (chn));
			buf.silence (nframes, (orig_nframes - nframes));
		}
	}

	if (!_currently_playing) {
		DEBUG_TRACE (DEBUG::Triggers, "nothing currently playing, consider stopping transport\n");
		_stop_all = false;
	}

	/* audio buffer (channel) count determined by max of input and
	 * _currently_playing's channel count (if it was audio).
	 */

	ChanCount cc (DataType::AUDIO, max_chans);

	/* MIDI buffer count not changed */
	cc.set_midi (bufs.count().n_midi());

	bufs.set_count (cc);
}

int
TriggerBox::determine_next_trigger (uint32_t current)
{
	uint32_t n;
	uint32_t runnable = 0;

	/* count number of triggers that can actually be run (i.e. they have a region) */

	for (uint32_t n = 0; n < all_triggers.size(); ++n) {
		if (all_triggers[n]->region()) {
			runnable++;
		}
	}

	if (runnable == 0 || !all_triggers[current]->region()) {
		return -1;
	}

	/* decide which of the two follow actions we're going to use (based on
	 * random number and the probability setting)
	 */

	int r = _pcg.rand (100); // 0 .. 99
	Trigger::FollowAction fa;

	if (r >= all_triggers[current]->follow_action_probability()) {
		fa = all_triggers[current]->follow_action (0);
	} else {
		fa = all_triggers[current]->follow_action (1);
	}

	/* first switch: deal with the "special" cases where we either do
	 * nothing or just repeat the current trigger
	 */

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("choose next trigger using follow action %1 given prob %2 and rnd %3\n", enum_2_string (fa), all_triggers[current]->follow_action_probability(), r));

	switch (fa) {

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

	switch (fa) {
	case Trigger::None:
		return -1;

	case Trigger::Again:
		return current;


	case Trigger::NextTrigger:
		n = current + 1;
		if (n < all_triggers.size()) {
			if (all_triggers[n]->region()) {
				return n;
			}
		}
		break;

	case Trigger::PrevTrigger:
		if (current > 0) {
			n = current - 1;
			if (all_triggers[n]->region()) {
				return n;
			}
		}
		break;

	case Trigger::ForwardTrigger:
		n = current;
		while (true) {
			++n;

			if (n >= all_triggers.size()) {
				cerr << "loop with n = " << n << " of " << all_triggers.size() << endl;
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

	case Trigger::ReverseTrigger:
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
			if ((uint32_t) n == current) {
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
		node.add_child_nocopy (_sidechain->get_state ());
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
			TriggerPtr trig;

			/* Note use of a custom delete function. We cannot
			   delete the old trigger from the RT context where the
			   trigger swap will happen, so we will ask the trigger
			   helper thread to take care of it.
			*/

			if (_data_type == DataType::AUDIO) {
				trig.reset (new AudioTrigger (all_triggers.size(), *this), Trigger::request_trigger_delete);
				all_triggers.push_back (trig);
				trig->set_state (**t, version);
			} else if (_data_type == DataType::MIDI) {
				trig.reset (new MIDITrigger (all_triggers.size(), *this), Trigger::request_trigger_delete);
				all_triggers.push_back (trig);
				trig->set_state (**t, version);
			}
		}
	}

	/* sidechain is a Processor (IO) */
	XMLNode* scnode = node.child (Processor::state_node_name.c_str ());
	if (scnode) {
		add_midi_sidechain ();
		assert (_sidechain);
		_sidechain->set_state (*scnode, version);
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

double
TriggerBox::position_as_fraction () const
{
	TriggerPtr cp = _currently_playing;
	if (!cp) {
		return -1;
	}
	return cp->position_as_fraction ();
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
	pthread_set_name (X_("Trigger Worker"));

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
					req->box->set_region (req->slot, req->region);
					break;
				case DeleteTrigger:
					delete_trigger (req->trigger);
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

	/* Quit is handled by simply delivering the request type (1 byte), with
	 * no payload in the FIFO. See ::thread_work() above.
	 */

	if (req->type != Quit) {
		if (requests.write (&req, 1) != 1) {
			return;
		}
	}

	_xthread.deliver (c);
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
TriggerBoxThread::set_region (TriggerBox& box, uint32_t slot, boost::shared_ptr<Region> r)
{
	TriggerBoxThread::Request* req = new TriggerBoxThread::Request (TriggerBoxThread::SetRegion);

	req->box = &box;
	req->slot = slot;
	req->region = r;

	queue_request (req);
}

void
TriggerBoxThread::request_delete_trigger (Trigger* t)
{
	TriggerBoxThread::Request* req = new TriggerBoxThread::Request (DeleteTrigger);
	req->trigger  = t;
	queue_request (req);
}

void
TriggerBoxThread::delete_trigger (Trigger* t)
{
	delete t;
}
