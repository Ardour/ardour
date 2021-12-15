#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <sstream>

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
		PBD::PropertyDescriptor<bool> passthru;
		PBD::PropertyDescriptor<bool> legato;
		PBD::PropertyDescriptor<Temporal::BBT_Offset> quantization;
		PBD::PropertyDescriptor<Trigger::LaunchStyle> launch_style;
		PBD::PropertyDescriptor<Trigger::FollowAction> follow_action0;
		PBD::PropertyDescriptor<Trigger::FollowAction> follow_action1;
		PBD::PropertyDescriptor<Trigger*> currently_playing;
		PBD::PropertyDescriptor<int> follow_count;
		PBD::PropertyDescriptor<int> follow_action_probability;
		PBD::PropertyDescriptor<float> velocity_effect;
		PBD::PropertyDescriptor<gain_t> gain;
		PBD::PropertyDescriptor<bool> stretching;
	}
}

Trigger::Trigger (uint64_t n, TriggerBox& b)
	: _box (b)
	, _state (Stopped)
	, _bang (0)
	, _unbang (0)
	, _index (n)
	, _launch_style (Toggle)
	, _use_follow (Properties::use_follow, true)
	, _follow_action { NextTrigger, Stop }
	, _follow_action_probability (Properties::follow_action_probability, 100)
	, _loop_cnt (0)
	, _follow_count (Properties::follow_count, 1)
	, _quantization (Temporal::BBT_Offset (0, 1, 0))
	, _legato (Properties::legato, false)
	, _barcnt (0.)
	, _apparent_tempo (0.)
	, _gain (1.0)
	, _pending_gain (1.0)
	, _midi_velocity_effect (Properties::velocity_effect, 0.)
	, _ui (0)
	, expected_end_sample (0)
	, _stretching (Properties::stretching, true)
	, _explicitly_stopped (false)
{
	add_property (_legato);
	add_property (_use_follow);
	add_property (_follow_count);
	add_property (_midi_velocity_effect);
	add_property (_follow_action_probability);
	add_property (_stretching);
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
	PropertyChanged (Properties::gain);
}

void
Trigger::set_midi_velocity_effect (float mve)
{
	_midi_velocity_effect = std::min (1.f, std::max (0.f, mve));
	PropertyChanged (Properties::velocity_effect);
}

void
Trigger::set_follow_count (uint32_t n)
{
	_follow_count = n;
	PropertyChanged (Properties::follow_count);
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
	PropertyChanged (Properties::follow_action_probability);
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
Trigger::request_stop ()
{
	_requests.stop = true;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 asked to stop\n", name()));
}

void
Trigger::startup()
{
	_state = WaitingToStart;
	_gain = _pending_gain;
	_loop_cnt = 0;
	_explicitly_stopped = false;
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
	/* XXX set expected_end_sample */
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
Trigger::process_state_requests ()
{
	bool stop = _requests.stop.exchange (false);

	if (stop) {

		/* This is for an immediate stop, not a quantized one */

		if (_state != Stopped) {
			shutdown ();
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
				begin_stop (true);
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
				begin_stop (true);
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

pframes_t
Trigger::maybe_compute_next_transition (samplepos_t start_sample, Temporal::Beats const & start, Temporal::Beats const & end, pframes_t dest_offset, bool passthru)
{
	using namespace Temporal;

	/* This should never be called by a stopped trigger */

	assert (_state != Stopped);

	/* In these states, we are not waiting for a transition */

	if (_state == Running || _state == Stopping) {
		/* will cover everything */
		return 0;
	}

	timepos_t transition_time (BeatTime);
	TempoMap::SharedPtr tmap (TempoMap::use());
	Temporal::BBT_Time transition_bbt;
	pframes_t extra_offset = 0;

	/* XXX need to use global grid here is quantization == zero */

	/* Given the value of @param start, determine, based on the
	 * quantization, the next time for a transition.
	 */

	if (_quantization.bars == 0) {
		Temporal::Beats transition_beats = start.round_up_to_multiple (Temporal::Beats (_quantization.beats, _quantization.ticks));
		transition_bbt = tmap->bbt_at (transition_beats);
		transition_time = timepos_t (transition_beats);
	} else {
		transition_bbt = tmap->bbt_at (timepos_t (start));
		transition_bbt = transition_bbt.round_up_to_bar ();
		transition_bbt.bars = (transition_bbt.bars / _quantization.bars) * _quantization.bars;
		transition_time = timepos_t (tmap->quarters_at (transition_bbt));
	}

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 quantized with %5 transition at %2, sb %3 eb %4\n", index(), transition_time.beats(), start, end, _quantization));

	/* See if this time falls within the range of time given to us */

	if (transition_time.beats() >= start && transition_time < end) {

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

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 will stop somewhere in the middle of run()\n", name()));

			/* offset within the buffer(s) for output remains
			   unchanged, since we will write from the first
			   location corresponding to start
			*/
			break;

		case WaitingToStart:
			retrigger ();
			_state = Running;
			set_expected_end_sample (tmap, transition_bbt);
			cerr << "starting at " << transition_bbt << " bars " << round(_barcnt) << " end at " << tmap->bbt_walk (transition_bbt, BBT_Offset (round (_barcnt), 0, 0)) << " sample = " << expected_end_sample << endl;
			PropertyChanged (ARDOUR::Properties::running);

			/* trigger will start somewhere within this process
			 * cycle. Compute the sample offset where any audio
			 * should end up, and the number of samples it should generate.
			 */

			extra_offset = std::max (samplepos_t (0), transition_samples - start_sample);

			if (!passthru) {
				/* XXX need to silence start of buffers up to dest_offset */
			}
			break;

		case WaitingForRetrigger:
			retrigger ();
			_state = Running;
			set_expected_end_sample (tmap, transition_bbt);
			cerr << "starting at " << transition_bbt << " bars " << round(_barcnt) << " end at " << tmap->bbt_walk (transition_bbt, BBT_Offset (round (_barcnt), 0, 0)) << " sample = " << expected_end_sample << endl;
			PropertyChanged (ARDOUR::Properties::running);

			/* trigger is just running normally, and will fill
			 * buffers entirely.
			 */

			break;

		default:
			fatal << string_compose (_("programming error: %1"), "impossible trigger state in ::maybe_compute_next_transition()") << endmsg;
			abort();
		}

	} else {

		/* retrigger time has not been reached, just continue
		   to play normally until then.
		*/

	}

	return extra_offset;
}

/*--------------------*/

AudioTrigger::AudioTrigger (uint64_t n, TriggerBox& b)
	: Trigger (n, b)
	, data (0)
	, read_index (0)
	, process_index (0)
	, data_length (0)
	, _start_offset (0)
	, _legato_offset (0)
	, usable_length (0)
	, last_sample (0)
	, _stretcher (0)
	, got_stretcher_padding (false)
	, to_pad (0)
	, to_drop (0)
{
}

AudioTrigger::~AudioTrigger ()
{
	for (std::vector<Sample*>::iterator d = data.begin(); d != data.end(); ++d) {
		delete *d;
	}

	delete _stretcher;
}

bool
AudioTrigger::stretching() const
{
	return (_apparent_tempo != .0) && _stretching;
}

void
AudioTrigger::set_expected_end_sample (Temporal::TempoMap::SharedPtr const & tmap, Temporal::BBT_Time const & transition_bbt)
{
	if (stretching()) {
		expected_end_sample = tmap->sample_at (tmap->bbt_walk(transition_bbt, Temporal::BBT_Offset (round (_barcnt), 0, 0)));
	} else {
		expected_end_sample = transition_samples = usable_length;
	}
}

SegmentDescriptor
AudioTrigger::get_segment_descriptor () const
{
	SegmentDescriptor sd;

	sd.set_extent (_start_offset, usable_length);
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

	return process_index / (double) (expected_end_sample - transition_samples);
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
	return timepos_t (process_index);
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

		_apparent_tempo = mbpm.estimateTempoOfSamples (data[0], data_length);

		if (_apparent_tempo == 0.0) {
			/* no apparent tempo, just return since we'll use it as-is */
			std::cerr << "Could not determine tempo for " << name() << std::endl;
			return;
		}

		cerr << name() << " Estimated bpm " << _apparent_tempo << " from " << (double) data_length / _box.session().sample_rate() << " seconds\n";
	}

	const double seconds = (double) data_length  / _box.session().sample_rate();
	const double quarters = (seconds / 60.) * _apparent_tempo;
	_barcnt = quarters / metric.meter().divisions_per_bar();

	/* use initial tempo in map (assumed for now to be the only one */

	const samplecnt_t one_bar = tm->bbt_duration_at (timepos_t (AudioTime), BBT_Offset (1, 0, 0)).samples();

	cerr << "tempo: " << _apparent_tempo << endl;
	cerr << "one bar in samples: " << one_bar << endl;
	cerr << "barcnt = " << round (_barcnt) << endl;
}

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
	const uint32_t nchans = ar->n_channels();

	/* XXX maybe get some of these options from region properties (when/if we have them) ? */

	RubberBandStretcher::Options options = RubberBandStretcher::Option (RubberBandStretcher::OptionProcessRealTime |
	                                                                    RubberBandStretcher::OptionTransientsCrisp);

	if (!_stretcher) {
		_stretcher = new RubberBandStretcher (_box.session().sample_rate(), nchans, options, 1.0, 1.0);
	} else {
		_stretcher->reset ();
	}

	_stretcher->setMaxProcessSize (rb_blocksize);
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
	const uint32_t nchans = ar->n_channels();
	const pframes_t orig_nframes = nframes;
	int avail = 0;
	BufferSet& scratch (_box.session().get_scratch_buffers (ChanCount (DataType::AUDIO, nchans)));
	std::vector<Sample*> bufp(nchans);
	const bool do_stretch = stretching();

	/* see if we're going to start or stop or retrigger in this run() call */
	pframes_t extra_offset = maybe_compute_next_transition (start_sample, start, end, dest_offset, passthru);

	nframes -= extra_offset;
	dest_offset += extra_offset;

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 after checking for transition, state = %2\n", name(), enum_2_string (_state)));

	switch (_state) {
	case Stopped:
	case WaitingForRetrigger:
	case WaitingToStart:
		/* did everything we could do */
		return nframes;
	case Running:
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

	if (do_stretch) {

		const double stretch = _apparent_tempo / bpm;
		_stretcher->setTimeRatio (stretch);

		// cerr << "apparent " << _apparent_tempo << " bpm " << bpm << " TR " << std::setprecision (6) << stretch << " latency " << _stretcher->getLatency() << endl;

		if ((avail = _stretcher->available()) < 0) {
			error << _("Could not configure rubberband stretcher") << endmsg;
			return -1;
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

	while (nframes) {

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

					_stretcher->process (&in[0], to_stretcher, at_end);
					read_index += to_stretcher;
					avail = _stretcher->available ();

					if (to_drop && avail) {
						samplecnt_t this_drop = std::min (std::min ((samplecnt_t) avail, to_drop), (samplecnt_t) scratch.get_audio (0).capacity());
						_stretcher->retrieve (&bufp[0], this_drop);
						to_drop -= this_drop;
						avail = _stretcher->available ();
					}

					// DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 process %2 at-end %3 avail %4 of %5\n", name(), to_stretcher, at_end, avail, nframes));
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
				if (transition_samples + retrieved > expected_end_sample) {
					from_stretcher = std::min ((samplecnt_t) from_stretcher, (retrieved - (expected_end_sample - transition_samples)));
					DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 total retrieved data %2 exceeds theoretical size %3, truncate from_stretcher to %4\n", expected_end_sample - transition_samples, from_stretcher));

					if (from_stretcher == 0) {
						break;
					}
				}
			}

		} else {
			/* no stretch */
			from_stretcher = (pframes_t) std::min ((samplecnt_t) nframes, (last_sample - read_index));
		}

		// DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 ready with %2 ri %3 ls %4, will write %5\n", name(), avail, read_index, last_sample, from_stretcher));

		/* deliver to buffers */

		for (uint32_t chn = 0; chn < bufs.count().n_audio(); ++chn) {

			uint64_t channel = chn %  data.size();
			AudioBuffer& buf (bufs.get_audio (channel));
			Sample* src = do_stretch ? bufp[channel] : (data[channel] + read_index);

			if (!passthru) {
				buf.read_from (src, from_stretcher, dest_offset);
				if (_gain != 1.0) {
					buf.apply_gain (_gain, from_stretcher);
				}
			} else {
				if (_gain != 1.0) {
					buf.accumulate_with_gain_from (src, from_stretcher, _gain, dest_offset);
				} else {
					buf.accumulate_with_gain_from (src, from_stretcher, _gain, dest_offset);
				}
			}
		}

		process_index += from_stretcher;

		/* Move read_index, in the case that we are not using a
		 * stretcher
		 */

		if (!do_stretch) {
			read_index += from_stretcher;
		}

		nframes -= from_stretcher;
		avail = _stretcher->available ();
		dest_offset += from_stretcher;

		if (read_index >= last_sample && (_apparent_tempo == 0. || avail <= 0)) {
			_state = Stopped;
			_loop_cnt++;
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end, now stopped, retrieved %2, avail %3\n", index(), retrieved, avail));
			break;
		}
	}

	if (_state == Stopped || _state == Stopping) {

		if (_loop_cnt == _follow_count) {

			/* have played the specified number of times, we're done */

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 loop cnt %2 satisfied, now stopped\n", index(), _follow_count));
			shutdown ();

		} else if (_state == Stopping) {

			/* did not reach the end of the data. Presumably
			 * another trigger was explicitly queued, and we
			 * stopped
			 */

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 not at end, but ow stopped\n", index()));
			shutdown ();

		} else {

			/* reached the end, but we haven't done that enough
			 * times yet for a follow action/stop to take
			 * effect. Time to get played again.
			 */

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 was stopping, now waiting to retrigger, loop cnt %2 fc %3\n", index(), _loop_cnt, _follow_count));
			_state = WaitingToStart;
			retrigger ();
			PropertyChanged (ARDOUR::Properties::running);
		}
	}

	return orig_nframes - nframes;
}

void
AudioTrigger::reload (BufferSet&, void*)
{
}

/*--------------------*/

MIDITrigger::MIDITrigger (uint64_t n, TriggerBox& b)
	: Trigger (n, b)
	, data_length (Temporal::Beats ())
	, usable_length (Temporal::Beats ())
	, last_event_beats (Temporal::Beats ())
	, _start_offset (0, 0, 0)
	, _legato_offset (0, 0, 0)
{
}

MIDITrigger::~MIDITrigger ()
{
}

void
MIDITrigger::set_expected_end_sample (Temporal::TempoMap::SharedPtr const & tmap, Temporal::BBT_Time const & transition_bbt)
{
	expected_end_sample = tmap->sample_at (tmap->bbt_walk(transition_bbt, Temporal::BBT_Offset (round (_barcnt), 0, 0)));
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
	usable_length = t.beats();

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
	usable_length = len.beats ();
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
	dest_offset = maybe_compute_next_transition (start_sample, start_beats, end_beats, dest_offset, passthru);

	switch (_state) {
	case Stopped:
	case WaitingForRetrigger:
	case WaitingToStart:
		return nframes;
	case Running:
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
		 * There's a slight complication here, because both
		 * start_sample and dest_offset reflect an offset from the
		 * start of the buffer that our parent (TriggerBox) processor
		 * is handling in its own run() method. start_sample may have
		 * been adjusted to reflect a previous Trigger's processing
		 * during this run cycle, and so has dest_offset.
		 */

		samplepos_t buffer_samples = timeline_samples - start_sample + dest_offset;
		last_event_samples = timeline_samples;

		const Evoral::Event<MidiBuffer::TimeType> ev (Evoral::MIDI_EVENT, buffer_samples, next_event.size(), const_cast<uint8_t*>(next_event.buffer()), false);
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("inserting %1\n", ev));
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

		if (_loop_cnt == _follow_count) {
			/* have played the specified number of times, we're done */

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 loop cnt %2 satisfied, now stopped\n", index(), _follow_count));
			shutdown ();

		} else {

			/* reached the end, but we haven't done that enough
			 * times yet for a follow action/stop to take
			 * effect. Time to get played again.
			 */

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 was stopping, now waiting to retrigger, loop cnt %2 fc %3\n", index(), _loop_cnt, _follow_count));
			/* we will "restart" at the beginning of the
			   next iteration of the trigger.
			*/
			transition_beats = transition_beats + data_length;
			retrigger ();
			_state = WaitingToStart;
		}

		/* the time we processed spans from start to the last event */

		if (last_event_samples != max_samplepos) {
			nframes = (last_event_samples - start_sample);
		} else {
			/* all frames covered */
		}

	} else {
		/* we didn't reach the end of the MIDI data, ergo we covered
		   the entire timespan passed into us.
		*/
	}

	return nframes;
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
	Properties::stretching.property_id = g_quark_from_static_string (X_("stretching"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for stretching = %1\n", Properties::stretching.property_id));
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
	, _currently_playing (0)
	, _stop_all (false)
	, _pass_thru (false)
	, requests (1024)
{
	set_display_to_user (false);

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
TriggerBox::set_order (int32_t n)
{
	_order = n;
}

void
TriggerBox::queue_explict (Trigger* t)
{
	assert (t);
	explicit_queue.write (&t, 1);
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("explicit queue %1, EQ = %2\n", t->index(), explicit_queue.read_space()));

	if (_currently_playing) {
		_currently_playing->unbang ();
	}
}

Trigger*
TriggerBox::get_next_trigger ()
{
	Trigger* r;

	if (explicit_queue.read (&r, 1) == 1) {

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("next trigger from explicit queue = %1\n", r->index()));
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
	_requests.stop_all = true;
}

void
TriggerBox::stop_all ()
{
	/* XXX needs to be done with mutex or via thread-safe queue */

	DEBUG_TRACE (DEBUG::Triggers, "stop-all request received\n");

	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->request_stop ();
	}

	_stop_all = true;

	explicit_queue.reset ();
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
	if (_data_type == DataType::MIDI) {
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

	_pass_thru = _requests.pass_thru.exchange (false);
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

	/* STEP FOUR: handle any incoming requests from the GUI or other
	 * non-MIDI UIs
	 */

	process_requests (bufs);

	/* STEP FIVE: handle any incoming MIDI requests
	 */

	process_midi_trigger_requests (bufs);

	/* STEP SIX: if at this point there is an active cue, make it trigger
	 * our corresponding slot
	 */

	if (_active_scene >= 0) {
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("tb noticed active scene %1\n", _active_scene));
		if (_active_scene < (int32_t) all_triggers.size()) {
			all_triggers[_active_scene]->bang ();
		}
	}

	/* STEP SEVEN: let each slot process any individual state requests
	 */

	std::vector<uint64_t> to_run;

	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		all_triggers[n]->process_state_requests ();
	}

	/* STEP EIGHT: if there is no active slot, see if there any queued up
	 */

	if (!_currently_playing) {
		if ((_currently_playing = get_next_trigger()) != 0) {
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

	if (!_session.transport_state_rolling()) {
		_session.start_transport_from_trigger ();
	}

	/* now get the information we need related to the tempo map and the
	 * timeline
	 */

	const Temporal::Beats end_beats (timepos_t (end_sample).beats());
	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());
	uint32_t max_chans = 0;
	Trigger* nxt = 0;
	pframes_t dest_offset = 0;

	while (nframes) {

		/* start can move if we have to switch triggers in mid-process cycle */

		const Temporal::Beats start_beats (timepos_t (start_sample).beats());
		const double bpm = tmap->quarters_per_minute_at (timepos_t (start_beats));

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("nf loop, ss %1 es %2 sb %3 eb %4 bpm %5\n", start_sample, end_sample, start_beats, end_beats, bpm));

		 /* see if there's another trigger explicitly queued */

		RingBuffer<Trigger*>::rw_vector rwv;
		explicit_queue.get_read_vector (&rwv);

		if (rwv.len[0] > 0) {

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("explicit queue rvec %1 + %2\n", rwv.len[0], rwv.len[1]));

			/* peek at it without dequeing it */

			nxt = *(rwv.buf[0]);

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
					nxt->jump_start ();
					_currently_playing->jump_stop ();
					/* and switch */
					DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 => %2 switched to in legato mode\n", _currently_playing->index(), nxt->index()));
					_currently_playing = nxt;
					PropertyChanged (Properties::currently_playing);

				} else {

					/* no legato-switch */

					if (_currently_playing->state() == Trigger::Stopped) {

						explicit_queue.increment_read_idx (1); /* consume the entry we peeked at */
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

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("trigger %1 complete, state now %2\n", _currently_playing->name(), enum_2_string (_currently_playing->state())));

		/* if we're not in the process of stopping all active triggers,
		 * but the current one has stopped, decide which (if any)
		 * trigger to play next.
		 */

		if (_currently_playing->state() == Trigger::Stopped) {
			if (!_stop_all && !_currently_playing->explicitly_stopped()) {
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 has stopped, need next...\n", _currently_playing->name()));
				int n = determine_next_trigger (_currently_playing->index());
				if (n < 0) {
					break; /* no triggers to come next, break out * of nframes loop */
					DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 finished, no next trigger\n", _currently_playing->name()));
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

		for (uint64_t chn = 0; chn < bufs.count().n_audio(); ++chn) {
			AudioBuffer& buf (bufs.get_audio (chn));
			buf.silence (nframes, (orig_nframes - nframes));
		}
	}

	if (!_currently_playing) {
		DEBUG_TRACE (DEBUG::Triggers, "nothing currently playing, consider stopping transport\n");
		_stop_all = false;
		if (active_trigger_boxes.fetch_sub (1) == 1) {
			/* last active trigger box */
			_session.stop_transport_from_trigger ();
		}
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
	case Trigger::None:
		return -1;

	case Trigger::Again:
		return current;

	case Trigger::NextTrigger:
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

double
TriggerBox::position_as_fraction () const
{
	Trigger* cp = _currently_playing;
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
