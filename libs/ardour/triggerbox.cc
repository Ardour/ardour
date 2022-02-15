#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <memory>
#include <sstream>

#include <boost/make_shared.hpp>

#include <glibmm.h>

#include <rubberband/RubberBandStretcher.h>

#include "pbd/basename.h"
#include "pbd/compose.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"
#include "pbd/types_convert.h"
#include "pbd/unwind.h"

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
		PBD::PropertyDescriptor<bool> running;
		PBD::PropertyDescriptor<bool> legato;
		PBD::PropertyDescriptor<bool> use_follow_length;
		PBD::PropertyDescriptor<Temporal::BBT_Offset> quantization;
		PBD::PropertyDescriptor<Temporal::BBT_Offset> follow_length;
		PBD::PropertyDescriptor<Trigger::LaunchStyle> launch_style;
		PBD::PropertyDescriptor<ARDOUR::FollowAction> follow_action0;
		PBD::PropertyDescriptor<ARDOUR::FollowAction> follow_action1;
		PBD::PropertyDescriptor<uint32_t> currently_playing;
		PBD::PropertyDescriptor<uint32_t> follow_count;
		PBD::PropertyDescriptor<int> follow_action_probability;
		PBD::PropertyDescriptor<float> velocity_effect;
		PBD::PropertyDescriptor<gain_t> gain;
		PBD::PropertyDescriptor<bool> stretchable;
		PBD::PropertyDescriptor<bool> cue_isolated;
		PBD::PropertyDescriptor<Trigger::StretchMode> stretch_mode;
		PBD::PropertyDescriptor<bool> tempo_meter;  /* only to transmit updates, not storage */
		PBD::PropertyDescriptor<bool> patch_change;  /* only to transmit updates, not storage */
		PBD::PropertyDescriptor<bool> channel_map;  /* only to transmit updates, not storage */
	}
}

std::string
ARDOUR::cue_marker_name (int32_t index)
{
	/* this somewhat weird code structure is intended to allow for easy and
	 * correct translation.
	 */

	using std::string;

	if (index == INT32_MAX) {
		/* this is a reasonable "stop" icon */
		return string (X_("\u25a1"));
	}

	switch (index) {
	case 0: return string (_("A"));
	case 1: return string (_("B"));
	case 2: return string (_("C"));
	case 3: return string (_("D"));
	case 4: return string (_("E"));
	case 5: return string (_("F"));
	case 6: return string (_("G"));
	case 7: return string (_("H"));
	case 8: return string (_("I"));
	case 9: return string (_("J"));
	case 10: return string (_("K"));
	case 11: return string (_("L"));
	case 12: return string (_("M"));
	case 13: return string (_("N"));
	case 14: return string (_("O"));
	case 15: return string (_("P"));
	case 16: return string (_("Q"));
	case 17: return string (_("R"));
	case 18: return string (_("S"));
	case 19: return string (_("T"));
	case 20: return string (_("U"));
	case 21: return string (_("V"));
	case 22: return string (_("W"));
	case 23: return string (_("X"));
	case 24: return string (_("Y"));
	case 25: return string (_("Z"));
	}

	return string();
}

FollowAction::FollowAction (std::string const & str)
{
	std::string::size_type colon = str.find_first_of (':');

	if (colon == std::string::npos) {
		throw failed_constructor ();
	}

	type = FollowAction::Type (string_2_enum (str.substr (0, colon), type));

	/* We use the ulong representation of the bitset because the string
	   version is absurd.
	*/
	unsigned long ul;
	std::stringstream ss (str.substr (colon+1));
	ss >> ul;
	if (!ss) {
		throw failed_constructor();
	}
	targets = Targets (ul);
}

std::string
FollowAction::to_string () const
{
	/* We use the ulong representation of the bitset because the string
	   version is absurd.
	*/
	return string_compose ("%1:%2", enum_2_string (type), targets.to_ulong());
}


Trigger * const Trigger::MagicClearPointerValue = (Trigger*) 0xfeedface;

Trigger::Trigger (uint32_t n, TriggerBox& b)
	: _launch_style (Properties::launch_style, OneShot)
	, _follow_action0 (Properties::follow_action0, FollowAction (FollowAction::Again))
	, _follow_action1 (Properties::follow_action1, FollowAction (FollowAction::Stop))
	, _follow_action_probability (Properties::follow_action_probability, 0)
	, _follow_count (Properties::follow_count, 1)
	, _quantization (Properties::quantization, Temporal::BBT_Offset (1, 0, 0))
	, _follow_length (Properties::follow_length, Temporal::BBT_Offset (1, 0, 0))
	, _use_follow_length (Properties::use_follow_length, false)
	, _legato (Properties::legato, false)
	, _gain (Properties::gain, 1.0)
	, _velocity_effect (Properties::velocity_effect, 0.)
	, _stretchable (Properties::stretchable, true)
	, _cue_isolated (Properties::cue_isolated, false)
	, _stretch_mode (Properties::stretch_mode, Trigger::Crisp)
	, _name (Properties::name, "")
	, _color (Properties::color, 0xBEBEBEFF)
	, process_index (0)
	, final_processed_sample (0)
	, _box (b)
	, _state (Stopped)
	, _bang (0)
	, _unbang (0)
	, _index (n)
	, _loop_cnt (0)
	, _ui (0)
	, _explicitly_stopped (false)
	, _pending_velocity_gain (1.0)
	, _velocity_gain (1.0)
	, _cue_launched (false)
	, _estimated_tempo (0.)
	, _segment_tempo (0.)
	, _beatcnt (0.)
	, _meter (4, 4)
	, expected_end_sample (0)
	, _pending ((Trigger*) 0)
	, last_property_generation (0)
{
	add_property (_launch_style);
	add_property (_follow_action0);
	add_property (_follow_action1);
	add_property (_follow_action_probability);
	add_property (_follow_count);
	add_property (_quantization);
	add_property (_follow_length);
	add_property (_use_follow_length);
	add_property (_legato);
	add_property (_name);
	add_property (_gain);
	add_property (_velocity_effect);
	add_property (_stretchable);
	add_property (_cue_isolated);
	add_property (_color);
	add_property (_stretch_mode);

	copy_to_ui_state ();
}

void
Trigger::request_trigger_delete (Trigger* t)
{
	TriggerBox::worker->request_delete_trigger (t);
}

void
Trigger::get_ui_state (Trigger::UIState &state) const 
{
	/* this is used for operations like d&d when we want to query the current state */
	/* you can't return ui_state here because that struct is used to queue properties that are being input *to* the trigger */
	/* TODO: rename our member variable ui_state to _queued_ui_state or similar @paul ? */
	state.launch_style = _launch_style;
	state.follow_action0 = _follow_action0;
	state.follow_action1 = _follow_action1;
	state.follow_action_probability = _follow_action_probability;
	state.follow_count = _follow_count;
	state.quantization = _quantization;
	state.follow_length = _follow_length;
	state.use_follow_length = _use_follow_length;
	state.legato = _legato;
	state.gain = _gain;
	state.velocity_effect = _velocity_effect;
	state.stretchable = _stretchable;
	state.cue_isolated = _cue_isolated;
	state.stretch_mode = _stretch_mode;

	state.name = _name;
	state.color = _color;

	/* tempo is currently not a property */
	state.tempo = segment_tempo();
}

void
Trigger::set_ui_state (Trigger::UIState &state)
{
	ui_state = state;

	/* increment ui_state generation so vals will get loaded when the trigger stops */
	unsigned int g = ui_state.generation.load();
	while (!ui_state.generation.compare_exchange_strong (g, g+1));

	/* tempo is currently outside the scope of ui_state */
	if (state.tempo > 0) {
		set_segment_tempo(state.tempo);
	}
}
	
void
Trigger::update_properties ()
{
	/* Don't update unless there is evidence of a change */

	unsigned int g;

	while ((g = ui_state.generation.load()) != last_property_generation) {

		StretchMode old_stretch = _stretch_mode;

		std::cerr << "prop copy for " << index() << endl;

		_launch_style = ui_state.launch_style;
		_follow_action0 = ui_state.follow_action0;
		_follow_action1 = ui_state.follow_action1;
		_follow_action_probability = ui_state.follow_action_probability;
		_follow_count = ui_state.follow_count;
		_quantization = ui_state.quantization;
		_follow_length = ui_state.follow_length;
		_use_follow_length = ui_state.use_follow_length;
		_legato = ui_state.legato;
		_gain = ui_state.gain;
		_velocity_effect = ui_state.velocity_effect;
		_stretchable = ui_state.stretchable;
		_cue_isolated = ui_state.cue_isolated;
		_stretch_mode = ui_state.stretch_mode;
		_color = ui_state.color;

		/* @paul: is this safe to do here ?*/
		/* the UI only allows changing stretch_mode when the clip is stopped,
		 * and you can't d+d or create a new clip while it's playing, so I think it's OK */
		if (_stretch_mode != old_stretch) {
			setup_stretcher ();
		}

		/* during construction of a new trigger, the ui_state.name is initialized and queued
		 *   ...but in the interim, we have likely been assigned a name from a region in a separate thread
		 *   ...so don't overwrite our name if ui_state.name is empty
		 */
		if (ui_state.name != "" ) {
			_name = ui_state.name;
		}

		last_property_generation = g;
	}

	/* we get here when we were able to copy the entire set of properties
	 * without the ui_state.generation value changing during the copy, or
	 * when no update appeared to be required.
	 */
}

void
Trigger::copy_to_ui_state ()
{
	/* usable only at object creation */

	ui_state.launch_style = _launch_style;
	ui_state.follow_action0 = _follow_action0;
	ui_state.follow_action1 = _follow_action1;
	ui_state.follow_action_probability = _follow_action_probability;
	ui_state.follow_count = _follow_count;
	ui_state.quantization = _quantization;
	ui_state.follow_length = _follow_length;
	ui_state.use_follow_length = _use_follow_length;
	ui_state.legato = _legato;
	ui_state.gain = _gain;
	ui_state.velocity_effect = _velocity_effect;
	ui_state.stretchable = _stretchable;
	ui_state.cue_isolated = _cue_isolated;
	ui_state.stretch_mode = _stretch_mode;
	ui_state.name = _name;
	ui_state.color = _color;
}

void
Trigger::send_property_change (PropertyChange pc)
{
	if (_box.fast_forwarding()) {
		return;
	}

	PropertyChanged (pc);
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

bool
Trigger::will_not_follow () const
{
	return (_follow_action0.val().type == FollowAction::None && _follow_action_probability == 0) ||
		(_follow_action0.val().type == FollowAction::None && _follow_action1.val().type == FollowAction::None);
}

#define TRIGGER_UI_SET(name,type) \
void \
Trigger::set_ ## name (type val) \
{ \
	unsigned int g = ui_state.generation.load(); \
	do { ui_state.name = val; } while (!ui_state.generation.compare_exchange_strong (g, g+1)); \
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("trigger %1 property& cas-set: %2 gen %3\n", index(), _ ## name.property_name(), ui_state.generation.load())); \
	send_property_change (Properties::name); /* EMIT SIGNAL */ \
	_box.session().set_dirty (); \
} \
type \
Trigger::name () const \
{ \
	unsigned int g = ui_state.generation.load (); \
	type val; \
\
	do { val = ui_state.name; } while (ui_state.generation.load () != g); \
\
	return val; \
}

#define TRIGGER_UI_SET_CONST_REF(name,type) \
void \
Trigger::set_ ## name (type const & val) \
{ \
	unsigned int g = ui_state.generation.load(); \
	do { ui_state.name = val; } while (!ui_state.generation.compare_exchange_strong (g, g+1)); \
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("trigger %1 property& cas-set: %2 gen %3\n", index(), _ ## name.property_name(), ui_state.generation.load())); \
	send_property_change (Properties::name); /* EMIT SIGNAL */ \
	_box.session().set_dirty (); \
} \
type \
Trigger::name () const \
{ \
	unsigned int g = ui_state.generation.load (); \
	type val; \
\
	do { val = ui_state.name; } while (ui_state.generation.load () != g); \
\
	return val; \
}

TRIGGER_UI_SET (cue_isolated,bool)
TRIGGER_UI_SET (stretchable, bool)
TRIGGER_UI_SET (gain, gain_t)
TRIGGER_UI_SET (velocity_effect, float)
TRIGGER_UI_SET (follow_count, uint32_t)
TRIGGER_UI_SET_CONST_REF (follow_action0, FollowAction)
TRIGGER_UI_SET_CONST_REF (follow_action1, FollowAction)
TRIGGER_UI_SET (launch_style, Trigger::LaunchStyle)
TRIGGER_UI_SET_CONST_REF (follow_length, Temporal::BBT_Offset)
TRIGGER_UI_SET (use_follow_length, bool)
TRIGGER_UI_SET (legato, bool)
TRIGGER_UI_SET (follow_action_probability, int)
TRIGGER_UI_SET_CONST_REF (quantization, Temporal::BBT_Offset)

#define TRIGGER_DIRECT_SET(name,type) \
void \
Trigger::set_ ## name (type val) \
{ \
	if (_ ## name == val) { return; } \
	_ ## name = val; \
	send_property_change (Properties::name); /* EMIT SIGNAL */ \
	_box.session().set_dirty (); \
} \
type \
Trigger::name () const \
{ \
	return _ ## name; \
}

#define TRIGGER_DIRECT_SET_CONST_REF(name,type) \
void \
Trigger::set_ ## name (type const & val) \
{ \
	if (_ ## name == val) { return; } \
	_ ## name = val; \
	send_property_change (Properties::name); /* EMIT SIGNAL */ \
	_box.session().set_dirty (); \
} \
type \
Trigger::name () const \
{ \
	return _ ## name; \
}

TRIGGER_DIRECT_SET_CONST_REF (name, std::string)
TRIGGER_DIRECT_SET (color, color_t)

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

XMLNode&
Trigger::get_state (void)
{
	XMLNode* node = new XMLNode (X_("Trigger"));

	/* XXX possible locking problems here if trigger is active, because
	 * properties could be overwritten
	 */

	for (OwnedPropertyList::iterator i = _properties->begin(); i != _properties->end(); ++i) {
		i->second->get_value (*node);
	}

	node->set_property (X_("index"), _index);
	node->set_property (X_("estimated-tempo"), _estimated_tempo);
	node->set_property (X_("segment-tempo"), _segment_tempo);

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
		set_region (r, false);  //TODO: this results in a call to estimate_tempo() which should be avoided if bpm is already known
	}

	node.get_property (X_("estimated-tempo"), _estimated_tempo);  //TODO: for now: if we know the bpm, overwrite the value that estimate_tempo() found

	double tempo;
	node.get_property (X_("segment-tempo"), tempo);
	set_segment_tempo(tempo);

	node.get_property (X_("index"), _index);
	set_values (node);

	copy_to_ui_state ();

	return 0;
}

bool
Trigger::internal_use_follow_length () const
{
	return (_follow_action0.val().type != FollowAction::None) && _use_follow_length;
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
	cerr << index() << " aka " << this << " region set to " << r << endl;
}

timepos_t
Trigger::current_pos() const
{
	return timepos_t (process_index);
}

double
Trigger::position_as_fraction () const
{
	if (!active()) {
		return 0.0;
	}

	return process_index / (double) final_processed_sample;
}

void
Trigger::retrigger ()
{
	process_index = 0;
}

void
Trigger::request_stop ()
{
	_requests.stop = true;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 asked to stop\n", name()));
}

void
Trigger::startup (BufferSet& bufs, pframes_t dest_offset, Temporal::BBT_Offset const & start_quantization)
{
	/* This is just a non-virtual wrapper with a default parameter that calls _startup() */
	_startup (bufs, dest_offset, start_quantization);
}

void
Trigger::_startup (BufferSet& bufs, pframes_t dest_offset, Temporal::BBT_Offset const & start_quantization)
{
	_state = WaitingToStart;
	_loop_cnt = 0;
	_velocity_gain = _pending_velocity_gain;
	_explicitly_stopped = false;

	if (start_quantization == Temporal::BBT_Offset()) {
		/* negative quantization means "do not quantize */
		_start_quantization = Temporal::BBT_Offset (-1, 0, 0);
	} else {
		_start_quantization = _quantization;
	}

	retrigger ();

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 starts up\n", name()));
	send_property_change (ARDOUR::Properties::running);
}

void
Trigger::shutdown (BufferSet& bufs, pframes_t dest_offset)
{
	_state = Stopped;
	_cue_launched = false;
	_pending_velocity_gain = _velocity_gain = 1.0;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 shuts down\n", name()));
	send_property_change (ARDOUR::Properties::running);
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
	send_property_change (ARDOUR::Properties::running);
}

void
Trigger::jump_stop (BufferSet& bufs, pframes_t dest_offset)
{
	/* this is used when we start a new trigger in legato mode. We do not
	   wait for quantization.
	*/
	shutdown (bufs, dest_offset);
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 requested state %2\n", index(), enum_2_string (_state)));
	send_property_change (ARDOUR::Properties::running);
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
	send_property_change (ARDOUR::Properties::running);
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
				send_property_change (ARDOUR::Properties::running);
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
			_cue_launched = (_box.active_scene() >= 0);
			std::cerr << index() << " aka " << name() << " launched via cue ? " << _cue_launched << std::endl;
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

Temporal::BBT_Time
Trigger::compute_start (Temporal::TempoMap::SharedPtr const & tmap, samplepos_t start, samplepos_t end, Temporal::BBT_Offset const & q, samplepos_t& start_samples, bool& will_start)
{
	Temporal::Beats start_beats (tmap->quarters_at (timepos_t (start)));
	Temporal::Beats end_beats (tmap->quarters_at (timepos_t (end)));

	Temporal::BBT_Time t_bbt;
	Temporal::Beats t_beats;

	if (!compute_quantized_transition (start, start_beats, end_beats, t_bbt, t_beats, start_samples, tmap, q)) {
		will_start = false;
		return Temporal::BBT_Time ();
	}

	will_start = true;
	return t_bbt;
}

bool
Trigger::compute_quantized_transition (samplepos_t start_sample, Temporal::Beats const & start_beats, Temporal::Beats const & end_beats,
                                       Temporal::BBT_Time& t_bbt, Temporal::Beats& t_beats, samplepos_t& t_samples,
                                       Temporal::TempoMap::SharedPtr const & tmap, Temporal::BBT_Offset const & q)
{
	/* XXX need to use global grid here is quantization == zero */

	/* Given the value of @param start, determine, based on the
	 * quantization, the next time for a transition.
	 */

	if (q < Temporal::BBT_Offset (0, 0, 0)) {
		/* negative quantization == do not quantize */

		t_samples = start_sample;
		t_beats = start_beats;
		t_bbt = tmap->bbt_at (t_beats);
	} else if (q.bars == 0) {
		t_beats = start_beats.round_up_to_multiple (Temporal::Beats (q.beats, q.ticks));
		t_bbt = tmap->bbt_at (t_beats);
		t_samples = tmap->sample_at (t_beats);
	} else {
		t_bbt = tmap->bbt_at (timepos_t (start_beats));
		t_bbt = t_bbt.round_up_to_bar ();
		/* bars are 1-based; 'every 4 bars' means 'on bar 1, 5, 9, ...' */
		t_bbt.bars = 1 + ((t_bbt.bars-1) / q.bars * q.bars);
		t_beats = tmap->quarters_at (t_bbt);
		t_samples = tmap->sample_at (t_bbt);
	}

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 quantized with %5 transition at %2, sb %3 eb %4\n", index(), t_samples, start_beats, end_beats, q));

	/* See if this time falls within the range of time given to us */

	if (t_beats < start_beats || t_beats > end_beats) {
		/* transition time not reached */
		return false;
	}


	return true;
}

pframes_t
Trigger::compute_next_transition (samplepos_t start_sample, Temporal::Beats const & start, Temporal::Beats const & end, pframes_t nframes,
                                  Temporal::BBT_Time& t_bbt, Temporal::Beats& t_beats, samplepos_t& t_samples,
                                  Temporal::TempoMap::SharedPtr const & tmap)
{
	using namespace Temporal;

	/* In these states, we are not waiting for a transition */

	if (_state == Stopped || _state == Running || _state == Stopping || _state == Playout) {
		/* no transition */
		return 0;
	}

	BBT_Offset q (_start_quantization);

	/* Clips don't stop on their own quantize; in Live they stop on the Global Quantize setting; we will choose 1 bar (Live's default) for now */
#warning when Global Quantize is implemented, use that instead of '1 bar' here
	if (_state == WaitingToStop) {
		q = BBT_Offset(1,0,0);
	}

	if (!compute_quantized_transition (start_sample, start, end, t_bbt, t_beats, t_samples, tmap, q)) {
		/* no transition */
		return 0;
	}

	switch (_state) {
	case WaitingToStop:
		nframes = t_samples - start_sample;
		break;

	case WaitingToStart:
		nframes -= std::max (samplepos_t (0), t_samples - start_sample);
		break;

	case WaitingForRetrigger:
		break;

	default:
		fatal << string_compose (_("programming error: %1"), "impossible trigger state in ::adjust_nframes()") << endmsg;
		abort();
	}

	return nframes;
}

void
Trigger::maybe_compute_next_transition (samplepos_t start_sample, Temporal::Beats const & start, Temporal::Beats const & end, pframes_t& nframes, pframes_t&  dest_offset)
{
	using namespace Temporal;

	/* This should never be called by a stopped trigger */

	assert (_state != Stopped);

	/* In these states, we are not waiting for a transition */

	if (_state == Running || _state == Stopping || _state == Playout) {
		/* will cover everything */
		return;
	}

	Temporal::BBT_Time transition_bbt;
	TempoMap::SharedPtr tmap (TempoMap::use());

	if (!compute_next_transition (start_sample, start, end, nframes, transition_bbt, transition_beats, transition_samples, tmap)) {
		return;
	}

	pframes_t extra_offset = 0;

	/* transition time has arrived! let's figure out what're doing:
	 * stopping, starting, retriggering
	 */

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 in range, should start/stop at %2 aka %3\n", index(), transition_samples, transition_beats));

	switch (_state) {

	case WaitingToStop:
		_state = Stopping;
		send_property_change (ARDOUR::Properties::running);

		/* trigger will reach it's end somewhere within this
		 * process cycle, so compute the number of samples it
		 * should generate.
		 */

		nframes = transition_samples - start_sample;

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 will stop somewhere in the middle of run(), specifically at %2 (%3) vs expected end at %4\n", name(), transition_beats, expected_end_sample));

		/* offset within the buffer(s) for output remains
		   unchanged, since we will write from the first
		   location corresponding to start
		*/
		break;

	case WaitingToStart:
		retrigger ();
		_state = Running;
		(void) compute_end (tmap, transition_bbt, transition_samples);
		send_property_change (ARDOUR::Properties::running);

		/* trigger will start somewhere within this process
		 * cycle. Compute the sample offset where any audio
		 * should end up, and the number of samples it should generate.
		 */

		extra_offset = std::max (samplepos_t (0), transition_samples - start_sample);

		nframes -= extra_offset;
		dest_offset += extra_offset;

		/* XXX need to silence start of buffers up to dest_offset */
		break;

	case WaitingForRetrigger:
		retrigger ();
		_state = Running;
		(void) compute_end (tmap, transition_bbt, transition_samples);
		send_property_change (ARDOUR::Properties::running);

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
				send_property_change (ARDOUR::Properties::running);
			}
		}
	}
}

template<typename TriggerType>
void
Trigger::start_and_roll_to (samplepos_t start_pos, samplepos_t end_position, TriggerType& trigger,
                            pframes_t (TriggerType::*run_method) (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample,
                                                                  Temporal::Beats const & start_beats, Temporal::Beats const & end_beats,
                                                                  pframes_t nframes, pframes_t dest_offset, double bpm))
{
	const pframes_t block_size = AudioEngine::instance()->samples_per_cycle ();
	BufferSet bufs;

	/* no need to allocate any space for BufferSet because we call
	   audio_run<false>() which is guaranteed to never use the buffers.

	   AudioTrigger::_startup() also does not use BufferSet (MIDITrigger
	   does, and we use virtual functions so the argument list is the same
	   for both, even though only the MIDI case needs the BufferSet).
	*/

	startup (bufs, 0, _quantization);
	_cue_launched = true;

	samplepos_t pos = start_pos;
	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());

	while (pos < end_position) {
		pframes_t nframes = std::min (block_size, (pframes_t) (end_position - pos));
		Temporal::Beats start_beats = tmap->quarters_at (timepos_t (pos));
		Temporal::Beats end_beats = tmap->quarters_at (timepos_t (pos+nframes));
		const double bpm = tmap->quarters_per_minute_at (timepos_t (start_beats));

		pframes_t n = (trigger.*run_method) (bufs, pos, pos+nframes, start_beats, end_beats, nframes, 0, bpm);

		/* We could have reached the end. Check and restart, because
		 * TriggerBox::fast_forward() already determined that we are
		 * the active trigger at @param end_position
		 */

		if (_state == Stopped) {
			retrigger ();
			_state = WaitingToStart;
			_cue_launched = true;
		}

		pos += n;
	}
}



/*--------------------*/

AudioTrigger::AudioTrigger (uint32_t n, TriggerBox& b)
	: Trigger (n, b)
	, _stretcher (0)
	, _start_offset (0)
	, read_index (0)
	, last_readable_sample (0)
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

void
AudioTrigger::set_stretch_mode (Trigger::StretchMode sm)
{
	if (_stretch_mode == sm) {
		return;
	}

	_stretch_mode = sm;
	send_property_change (Properties::stretch_mode);
	_box.session().set_dirty();
}

void
AudioTrigger::set_segment_tempo (double t)
{
	if (_segment_tempo != t) {

		_segment_tempo = t;

		/*beatcnt is a derived property from segment tempo and the file's length*/
		const double seconds = (double) data.length  / _box.session().sample_rate();
		_beatcnt = _segment_tempo * (seconds/60.0);

		send_property_change (ARDOUR::Properties::tempo_meter);
		_box.session().set_dirty();
	}

	/* TODO:  once we have a Region Trimmer, this could get more complicated:
	 *  this segment might overlap another SD (Coverage==Internal|Start|End)
	 *  in which case we might be setting both SDs, or not.  TBD*/
	if (_region) {
		SegmentDescriptor segment = get_segment_descriptor();
		for (auto & src : _region->sources()) {
			src->set_segment_descriptor (segment);
		}
	}
}

void
AudioTrigger::set_segment_beatcnt (double count)
{
	//given a beatcnt from the user, we use the data length to re-calc tempo internally
	// ... TODO:  provide a graphical trimmer to give the user control of data.length by dragging the start and end of the sample.
	const double seconds = (double) data.length  / _box.session().sample_rate();
	double tempo = count / (seconds/60.0);

	set_segment_tempo(tempo);
}

bool
AudioTrigger::stretching() const
{
	return (_segment_tempo != .0) && _stretchable;
}

SegmentDescriptor
AudioTrigger::get_segment_descriptor () const
{
	SegmentDescriptor sd;

	sd.set_extent (_region->start_sample(), _region->length_samples());
	sd.set_tempo (Temporal::Tempo (_segment_tempo, 4));

	return sd;
}

void
AudioTrigger::_startup (BufferSet& bufs, pframes_t dest_offset, Temporal::BBT_Offset const & start_quantization)
{
	Trigger::_startup (bufs, dest_offset, start_quantization);
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

	if (Trigger::set_state (node, version)) {
		return -1;
	}

	node.get_property (X_("start"), t);
	_start_offset = t.samples();

	return 0;
}

void
AudioTrigger::set_start (timepos_t const & s)
{
	/* XXX better minimum size needed */
	_start_offset = std::max (samplepos_t (4096), s.samples ());
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

void
AudioTrigger::start_and_roll_to (samplepos_t start_pos, samplepos_t end_position)
{
	Trigger::start_and_roll_to<AudioTrigger> (start_pos, end_position, *this, &AudioTrigger::audio_run<false>);
}

timepos_t
AudioTrigger::compute_end (Temporal::TempoMap::SharedPtr const & tmap, Temporal::BBT_Time const & transition_bbt, samplepos_t transition_sample)
{
	/* Our task here is to set:

	   expected_end_sample: (TIMELINE!) the sample position where the data for the clip should run out (taking stretch into account)
           last_readable_sample: the sample in the data where we stop reading
           final_processed_sample: the sample where the trigger stops and the follow action if any takes effect

           Things that affect these values:

           data.length : how many samples there are in the data  (AudioTime / samples)
           _follow_length : the (user specified) time after the start of the trigger when the follow action should take effect
           _use_follow_length : whether to use the follow_length value, or the clip's natural length
           _beatcnt : the expected duration of the trigger, based on analysis of its tempo .. can be overridden by the user later
	*/

	samplepos_t end_by_follow_length = tmap->sample_at (tmap->bbt_walk(transition_bbt, _follow_length));
	samplepos_t end_by_beatcnt = tmap->sample_at (tmap->bbt_walk(transition_bbt, Temporal::BBT_Offset (0, round (_beatcnt), 0)));  //OK?
	samplepos_t end_by_data_length = transition_sample + (data.length - _start_offset);

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 SO %9 @ %2 / %3 / %4 ends: FL %5 (from %6) BC %7 DL %8\n",
	                                              index(), transition_sample, transition_beats, transition_bbt,
	                                              end_by_follow_length, _follow_length, end_by_beatcnt, end_by_data_length, _start_offset));

	if (stretching()) {
		if (internal_use_follow_length()) {
			expected_end_sample = std::min (end_by_follow_length, end_by_beatcnt);
		} else {
			expected_end_sample = end_by_beatcnt;
		}
	} else {
		if (internal_use_follow_length()) {
			expected_end_sample = std::min (end_by_follow_length, end_by_data_length);
		} else {
			expected_end_sample = end_by_data_length;
		}
	}

	if (internal_use_follow_length()) {
		final_processed_sample = end_by_follow_length - transition_sample;
	} else {
		final_processed_sample = expected_end_sample - transition_sample;
	}

	samplecnt_t usable_length;

	if (internal_use_follow_length() && (end_by_follow_length < end_by_data_length)) {
		usable_length = end_by_follow_length - transition_samples;
	} else {
		usable_length = (data.length - _start_offset);
	}

	/* called from compute_end() when we know the time (audio &
	 * musical time domains when we start starting. Our job here is to
	 * define the last_readable_sample we can use as data.
	 */

	Temporal::BBT_Offset q (_quantization);

	if (launch_style() != Repeat || (q == Temporal::BBT_Offset())) {

		last_readable_sample = _start_offset + usable_length;

	} else {

		/* This is for Repeat mode only deliberately ignore the _follow_length
		 * here, because we'll be playing just the quantization distance no
		 * matter what.
		 */

		/* XXX MUST HANDLE BAR-LEVEL QUANTIZATION */

		timecnt_t len (Temporal::Beats (q.beats, q.ticks), timepos_t (Temporal::Beats()));
		last_readable_sample = _start_offset + len.samples();
	}

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1: final sample %2 vs ees %3 ls %4\n", index(), final_processed_sample, expected_end_sample, last_readable_sample));

	return timepos_t (expected_end_sample);
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

	estimate_tempo ();  /* NOTE: if this is an existing clip (D+D copy) then it will likely have a SD tempo, and that short-circuits minibpm for us */

	/* given an initial tempo guess, we need to set our operating tempo and beat_cnt value.
	 *  this may be reset momentarily with user-settings (UIState) from a d+d operation */
	set_segment_tempo(_estimated_tempo);

	setup_stretcher ();

	/* Given what we know about the tempo and duration, set the defaults
	 * for the trigger properties.
	 */

	if (_segment_tempo == 0.) {
		_stretchable = false;
		_quantization = Temporal::BBT_Offset (-1, 0, 0);
		_follow_action0 = FollowAction (FollowAction::None);
	} else {

		if (probably_oneshot()) {
			/* short trigger, treat as a one shot */
			_stretchable = false;
			_follow_action0 = FollowAction (FollowAction::None);
			_quantization = Temporal::BBT_Offset (-1, 0, 0);
		} else {
			_stretchable = true;
			_quantization = Temporal::BBT_Offset (1, 0, 0);
			_follow_action0 = FollowAction (FollowAction::Again);
		}
	}

	_follow_action_probability = 0; /* 100% left */

	send_property_change (ARDOUR::Properties::name);

	return 0;
}

void
AudioTrigger::estimate_tempo ()
{
	using namespace Temporal;
	TempoMap::SharedPtr tm (TempoMap::use());

	TimelineRange range (_region->start(), _region->start() + _region->length(), 0);
	SegmentDescriptor segment;
	bool have_segment;

	have_segment = _region->source (0)->get_segment_descriptor (range, segment);

	if (have_segment) {

		_estimated_tempo = segment.tempo().quarter_notes_per_minute ();
		_meter = segment.meter();
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1: tempo and meter from segment descriptor\n", index()));

	} else {
		/* not a great guess, but what else can we do? */

		TempoMetric const & metric (tm->metric_at (timepos_t (AudioTime)));

		_meter = metric.meter ();

		/* check the name to see if there's a (heuristically obvious) hint
		 * about the tempo.
		 */

		string str = _region->name();
		string::size_type bi;
		string::size_type ni;
		double text_tempo = -1.;

		std::cerr << "Determine tempo for " << name() << std::endl;

		if (((bi = str.find (" bpm")) != string::npos) ||
		    ((bi = str.find ("bpm")) != string::npos)  ||
		    ((bi = str.find (" BPM")) != string::npos) ||
		    ((bi = str.find ("BPM")) != string::npos)  ){

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
						_estimated_tempo = text_tempo;
						std::cerr << "from filename, tempo = " << _estimated_tempo << std::endl;
					}
				}
			}
		}

		/* We don't have too many good choices here. Triggers can fire at any
		 * time, so there's no special place on the tempo map that we can use
		 * to get the meter from and thus compute an estimated bar count for
		 * this region. Our solution for now: just use the first meter.
		 */

		if (text_tempo < 0) {

			breakfastquay::MiniBPM mbpm (_box.session().sample_rate());

			mbpm.setBPMRange (metric.tempo().quarter_notes_per_minute () * 0.75, metric.tempo().quarter_notes_per_minute() * 1.5);

			_estimated_tempo = mbpm.estimateTempoOfSamples (data[0], data.length);

			if (_estimated_tempo == 0.0) {
				/* no estimated tempo, just return since we'll use it as-is */
				std::cerr << "Could not determine tempo for " << name() << std::endl;
				return;
			}

			cerr << name() << " Estimated bpm " << _estimated_tempo << " from " << (double) data.length / _box.session().sample_rate() << " seconds\n";
		}
	}

	const double seconds = (double) data.length  / _box.session().sample_rate();

	/* now check the determined tempo and force it to a value that gives us
	   an integer beat/quarter count. This is a heuristic that tries to
	   avoid clips that slightly over- or underrun a quantization point,
	   resulting in small or larger gaps in output if they are repeating.
	*/

	double beatcount;
	if ((_estimated_tempo != 0.)) {
		/* fractional beatcnt */
		double maybe_beats = (seconds / 60.) * _estimated_tempo;
		beatcount = round (maybe_beats);
		double est = _estimated_tempo;
		_estimated_tempo = beatcount / (seconds/60.);
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("given original estimated tempo %1, rounded beatcnt is %2 : resulting in working bpm = %3\n", est, _beatcnt, _estimated_tempo));
	}

	/* initialize our follow_length to match the beatcnt ... user can later change this value to have the clip end sooner or later than its data length */
	set_follow_length(Temporal::BBT_Offset( 0, rint(beatcount), 0));

	/* use initial tempo in map (assumed for now to be the only one */

	cerr << "estimated tempo: " << _estimated_tempo << endl;

#if 0
	const samplecnt_t one_beat = tm->bbt_duration_at (timepos_t (AudioTime), BBT_Offset (0, 1, 0)).samples();
	cerr << "one beat in samples: " << one_beat << endl;
	cerr << "rounded beatcount = " << round (beatcount) << endl;
#endif
}

bool
AudioTrigger::probably_oneshot () const
{
	assert (_segment_tempo != 0.);

	if ((data.length < (_box.session().sample_rate()/2)) ||  //less than 1/2 second
        (_segment_tempo > 140) ||                            //minibpm thinks this is really fast
        (_segment_tempo < 60)) {                             //minibpm thinks this is really slow
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
AudioTrigger::reset_stretcher ()
{
	_stretcher->reset ();
	got_stretcher_padding = false;
	to_pad = 0;
	to_drop = 0;
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
	const uint32_t nchans = std::min (_box.input_streams().n_audio(), ar->n_channels());

	//map our internal enum to a rubberband option
	RubberBandStretcher::Option ro;
	switch (_stretch_mode) {
		case Trigger::Crisp  : ro = RubberBandStretcher::OptionTransientsCrisp; break;
		case Trigger::Mixed  : ro = RubberBandStretcher::OptionTransientsMixed; break;
		case Trigger::Smooth : ro = RubberBandStretcher::OptionTransientsSmooth; break;
	}

	RubberBandStretcher::Options options = RubberBandStretcher::Option (RubberBandStretcher::OptionProcessRealTime |
	                                                                    ro);

	delete _stretcher;
	_stretcher = new RubberBandStretcher (_box.session().sample_rate(), nchans, options, 1.0, 1.0);
	cerr << index() << " Set up stretcher for " << nchans << " channels\n";
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
	Trigger::retrigger ();

	update_properties ();
	reset_stretcher ();

	read_index = _start_offset + _legato_offset;
	retrieved = 0;
	_legato_offset = 0; /* used one time only */

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 retriggered to %2\n", _index, read_index));
}

template<bool in_process_context>
pframes_t
AudioTrigger::audio_run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample,
                         Temporal::Beats const & start, Temporal::Beats const & end,
                         pframes_t nframes, pframes_t dest_offset, double bpm)
{
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(_region);
	/* We do not modify the I/O of our parent route, so we process only min (bufs.n_audio(),region.channels()) */
	const uint32_t nchans = (in_process_context ? std::min (bufs.count().n_audio(), ar->n_channels()) : ar->n_channels());
	int avail = 0;
	BufferSet* scratch;
	std::unique_ptr<BufferSet> scratchp;
	std::vector<Sample*> bufp(nchans);
	const bool do_stretch = stretching();

	/* see if we're going to start or stop or retrigger in this run() call */
	maybe_compute_next_transition (start_sample, start, end, nframes, dest_offset);
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

	if (in_process_context) {
		scratch = &(_box.session().get_scratch_buffers (ChanCount (DataType::AUDIO, nchans)));
	} else {
		scratchp.reset (new BufferSet ());
		scratchp->ensure_buffers (DataType::AUDIO, nchans, nframes);
		/* have to set up scratch as a raw ptr so that the in_process_context
		   and !in_process_context case can use the same code syntax
		*/
		scratch = scratchp.get();
	}

	for (uint32_t chn = 0; chn < nchans; ++chn) {
		bufp[chn] = scratch->get_audio (chn).data();
	}

	/* tell the stretcher what we are doing for this ::run() call */

	if (do_stretch && _state != Playout) {

		const double stretch = _segment_tempo / bpm;
		_stretcher->setTimeRatio (stretch);

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("clip tempo %1 bpm %2 ratio %3%4\n", _segment_tempo, bpm, std::setprecision (6), stretch));

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
			const samplecnt_t limit = std::min ((samplecnt_t) scratch->get_audio (0).capacity(), to_pad);
			for (uint32_t chn = 0; chn < nchans; ++chn) {
				memset (bufp[chn], 0, sizeof (Sample) * limit);
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

			if (read_index < last_readable_sample) {

				/* still have data to push into the stretcher */

				to_stretcher = (pframes_t) std::min (samplecnt_t (rb_blocksize), (last_readable_sample - read_index));
				const bool at_end = (to_stretcher < rb_blocksize);

				while ((pframes_t) avail < nframes && (read_index < last_readable_sample)) {
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
						samplecnt_t this_drop = std::min (std::min ((samplecnt_t) avail, to_drop), (samplecnt_t) scratch->get_audio (0).capacity());
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
				// cerr << "FS#1 from nframes = " << from_stretcher << endl;
			} else {

				/* finished delivering data to stretcher, but may have not yet retrieved it all */
				avail = _stretcher->available ();
				from_stretcher = (pframes_t) std::min ((pframes_t) nframes, (pframes_t) avail);
				// cerr << "FS#X from avail " << avail << " nf " << nframes << " = " << from_stretcher << endl;
			}

			/* fetch the stretch */

			retrieved += _stretcher->retrieve (&bufp[0], from_stretcher);

			if (read_index >= last_readable_sample) {

				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 no more data to deliver to stretcher, but retrieved %2 to put current end at %3 vs %4 / %5 pi %6\n",
				                                              index(), retrieved, transition_samples + retrieved, expected_end_sample, final_processed_sample, process_index));

				if (transition_samples + retrieved > expected_end_sample) {
					/* final pull from stretched data into output buffers */
					from_stretcher = std::min ((samplecnt_t) from_stretcher, final_processed_sample - process_index);
					// cerr << "FS#2 from ees " << expected_end_sample << " - " << process_index << " = " << from_stretcher << endl;

					DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 total retrieved data %2 exceeds theoretical size %3, truncate from_stretcher to %4\n",
					                                              index(), retrieved, expected_end_sample - transition_samples, from_stretcher));

					if (from_stretcher == 0) {

						if (process_index < final_processed_sample) {
							DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached (EX) end, entering playout mode to cover %2 .. %3\n", index(), process_index, final_processed_sample));
							_state = Playout;
						} else {
							DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached (EX) end, now stopped, retrieved %2, avail %3 pi %4 vs fs %5\n", index(), retrieved, avail, process_index, final_processed_sample));
							_state = Stopped;
							_loop_cnt++;
						}

						break;
					}

				}
			}

		} else {
			/* no stretch */
			from_stretcher = (pframes_t) std::min ((samplecnt_t) nframes, (last_readable_sample - read_index));
			// cerr << "FS#3 from lrs " << last_readable_sample <<  " - " << read_index << " = " << from_stretcher << endl;

		}

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 ready with %2 ri %3 ls %4, will write %5\n", name(), avail, read_index, last_readable_sample, from_stretcher));

		/* deliver to buffers */

		if (in_process_context) { /* constexpr, will be handled at compile time */

			for (uint32_t chn = 0; chn < bufs.count().n_audio(); ++chn) {

				uint32_t channel = chn %  data.size();
				AudioBuffer& buf (bufs.get_audio (chn));
				Sample* src = do_stretch ? bufp[channel] : (data[channel] + read_index);

				gain_t gain = _velocity_gain * _gain;  //incorporate the gain from velocity_effect

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

		if (read_index >= last_readable_sample && (!do_stretch || avail <= 0)) {

			if (process_index < final_processed_sample) {
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end, entering playout mode to cover %2 .. %3\n", index(), process_index, final_processed_sample));
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
		const pframes_t remaining_frames_till_final = final_processed_sample - process_index;
		const pframes_t to_fill = std::min (remaining_frames_till_final, remaining_frames_for_run);

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 playout mode, remaining in run %2 till final %3 @ %5 ts %7 vs pi @ %6 to fill %4\n",
		                                              index(), remaining_frames_for_run, remaining_frames_till_final, to_fill, final_processed_sample, process_index, transition_samples));

		if (remaining_frames_till_final != 0) {

			process_index += to_fill;
			covered_frames += to_fill;

			if (process_index < final_processed_sample) {
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
	, data_length (Temporal::Beats())
	, last_event_beats (Temporal::Beats())
	, _start_offset (0, 0, 0)
	, _legato_offset (0, 0, 0)
{
#if 0 /* for prototype + testing only */
	Evoral::PatchChange<MidiBuffer::TimeType> pc (0, 0, 12, 0);
	set_patch_change (pc);
#endif

	_channel_map.assign (16, -1);
}

MIDITrigger::~MIDITrigger ()
{
}

void
MIDITrigger::set_channel_map (int channel, int target)
{
	if (channel < 0 || channel >= 16) {
		return;
	}

	if (target < 0 || target >= 16) {
		return;
	}

	if (_channel_map[channel] != target) {
		_channel_map[channel] = target;
		send_property_change (Properties::channel_map);
	}
}

void
MIDITrigger::unset_channel_map (int channel)
{
	if (channel < 0 || channel >= 16) {
		return;
	}

	if (_channel_map[channel] >= 0) {
		_channel_map[channel] = -1;
		send_property_change (Properties::channel_map);
	}
}

int
MIDITrigger::channel_map (int channel)
{
	if (channel < 0 || channel >= 16) {
		return -1;
	}
	return _channel_map[channel];
}

void
MIDITrigger::set_patch_change (Evoral::PatchChange<MidiBuffer::TimeType> const & pc)
{
	assert (pc.is_set());
	_patch_change[pc.channel()] = pc;
	send_property_change (Properties::patch_change);
}

void
MIDITrigger::unset_all_patch_changes ()
{
	bool changed = false;

	for (uint8_t chn = 0; chn < 16; ++chn) {
		changed |= _patch_change[chn].is_set();
		_patch_change[chn].unset ();
	}

	if (changed) {
		send_property_change (Properties::patch_change);
	}
}

void
MIDITrigger::unset_patch_change (uint8_t channel)
{
	assert (channel < 16);
	if (_patch_change[channel].is_set()) {
		_patch_change[channel].unset ();
		send_property_change (Properties::patch_change);
	}
}

bool
MIDITrigger::patch_change_set (uint8_t channel) const
{
	assert (channel < 16);
	return _patch_change[channel].is_set();
}

Evoral::PatchChange<MidiBuffer::TimeType> const &
MIDITrigger::patch_change (uint8_t channel) const
{
	assert (channel < 16);
	return _patch_change[channel];
}


bool
MIDITrigger::probably_oneshot () const
{
	/* XXX fix for short chord stabs */
	return false;
}

void
MIDITrigger::start_and_roll_to (samplepos_t start_pos, samplepos_t end_position)
{
	Trigger::start_and_roll_to (start_pos, end_position, *this, &MIDITrigger::midi_run<false>);
}

timepos_t
MIDITrigger::compute_end (Temporal::TempoMap::SharedPtr const & tmap, Temporal::BBT_Time const & transition_bbt, samplepos_t)
{
	Temporal::Beats end_by_follow_length = tmap->quarters_at (tmap->bbt_walk (transition_bbt, _follow_length));
	Temporal::Beats end_by_data_length = transition_beats + data_length;

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 ends: FL %2 DL %3 tbbt %4 fl %5\n", index(), end_by_follow_length, end_by_data_length, transition_bbt, _follow_length));

	Temporal::BBT_Offset q (_quantization);

	if (launch_style() != Repeat || (q == Temporal::BBT_Offset())) {

		if (internal_use_follow_length()) {
			final_beat = end_by_follow_length;
		} else {
			final_beat = end_by_data_length;
		}

	} else {

		/* XXX MUST HANDLE BAR-LEVEL QUANTIZATION */

		timecnt_t len (Temporal::Beats (q.beats, q.ticks), timepos_t (Temporal::Beats()));
		final_beat = len.beats ();
	}

	timepos_t e (final_beat);

	final_processed_sample = e.samples() - transition_samples;

	return e;
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
MIDITrigger::_startup (BufferSet& bufs, pframes_t dest_offset, Temporal::BBT_Offset const & start_quantization)
{
	Trigger::_startup (bufs, dest_offset, start_quantization);

	MidiBuffer* mb  = 0;

	if (bufs.count().n_midi() != 0) {
		mb = &bufs.get_midi (0);
	}

	/* Possibly inject patch changes, if set */

	for (int chn = 0; chn < 16; ++chn) {
		if (_patch_change[chn].is_set()) {
			_patch_change[chn].set_time (dest_offset);
			cerr << index() << " Injecting patch change " << _patch_change[chn].program() << " @ " << dest_offset << endl;
			for (int msg = 0; msg < _patch_change[chn].messages(); ++msg) {
				if (mb) {
					mb->insert_event (_patch_change[chn].message (msg));
				}
				_box.tracker->track (_patch_change[chn].message (msg).buffer());
			}
		}
	}
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

	if (bufs.count().n_midi()) {
		MidiBuffer& mb (bufs.get_midi (0));
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 shutdown, resolve notes @ %2\n", index(), dest_offset));
		_box.tracker->resolve_notes (mb, dest_offset);
	}

	_box.tracker->reset ();
}

void
MIDITrigger::jump_stop (BufferSet& bufs, pframes_t dest_offset)
{
	Trigger::jump_stop (bufs, dest_offset);

	MidiBuffer& mb (bufs.get_midi (0));
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 jump stop, resolve notes @ %2\n", index(), dest_offset));
	_box.tracker->resolve_notes (mb, dest_offset);

	retrigger ();
}

XMLNode&
MIDITrigger::get_state (void)
{
	XMLNode& node (Trigger::get_state());

	node.set_property (X_("start"), start_offset());

	XMLNode* patches_node = 0;

	for (int chn = 0; chn < 16; ++chn) {
		if (_patch_change[chn].is_set()) {
			if (!patches_node) {
				patches_node = new XMLNode (X_("PatchChanges"));
			}
			XMLNode* patch_node = new XMLNode (X_("PatchChange"));
			patch_node->set_property (X_("channel"), _patch_change[chn].channel());
			patch_node->set_property (X_("bank"), _patch_change[chn].bank());
			patch_node->set_property (X_("program"), _patch_change[chn].program());

			patches_node->add_child_nocopy (*patch_node);
		}
	}

	if (patches_node) {
		node.add_child_nocopy (*patches_node);
	}

	std::string cmstr;

	for (int chn = 0; chn < 16; ++chn) {
		char buf[4];

		if (chn > 0) {
			cmstr += ',';
		}

		snprintf (buf, sizeof (buf), "%d", _channel_map[chn]);
		cmstr += buf;
	}

	node.set_property (X_("channel-map"), cmstr);

	return node;
}

int
MIDITrigger::set_state (const XMLNode& node, int version)
{
	timepos_t t;

	if (Trigger::set_state (node, version)) {
		return -1;
	}

	node.get_property (X_("start"), t);
	Temporal::Beats b (t.beats());
	/* XXX need to deal with bar offsets */
	_start_offset = Temporal::BBT_Offset (0, b.get_beats(), b.get_ticks());

	XMLNode* patches_node = node.child (X_("PatchChanges"));

	if (patches_node) {
		XMLNodeList const & children = patches_node->children();
		for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
			if ((*i)->name() == X_("PatchChange")) {
				int c, p, b;
				if ((*i)->get_property (X_("channel"), c) &&
				    (*i)->get_property (X_("program"), p) &&
				    (*i)->get_property (X_("bank"), b)) {
					_patch_change[c] = Evoral::PatchChange<MidiBuffer::TimeType> (0, c, p, b);
				}
			}
		}
	}

	std::string cmstr;

	if (node.get_property (X_("channel-map"), cmstr)) {
		std::stringstream ss (cmstr);
		char comma;
		for (int chn = 0; chn < 16; ++chn) {
			ss >> _channel_map[chn];
			if (!ss) {
				break;
			}
			ss >> comma;
			if (!ss) {
				break;
			}
		}
	}

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

	send_property_change (ARDOUR::Properties::name);

	return 0;
}

void
MIDITrigger::retrigger ()
{
	Trigger::retrigger ();

	update_properties ();

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

template<bool in_process_context>
pframes_t
MIDITrigger::midi_run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample,
                       Temporal::Beats const & start_beats, Temporal::Beats const & end_beats,
                       pframes_t nframes, pframes_t dest_offset, double bpm)
{
	MidiBuffer* mb (in_process_context? &bufs.get_midi (0) : 0);
	typedef Evoral::Event<MidiModel::TimeType> MidiEvent;
	const timepos_t region_start_time = _region->start();
	const Temporal::Beats region_start = region_start_time.beats();
	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());
	samplepos_t last_event_samples = max_samplepos;

	/* see if we're going to start or stop or retrigger in this run() call */
	pframes_t ignore_computed_dest_offset = 0;
	maybe_compute_next_transition (start_sample, start_beats, end_beats, nframes, ignore_computed_dest_offset);
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

	Temporal::Beats last_event_timeline_beats;

	while (iter != model->end() && _state != Playout) {

		MidiEvent const & event (*iter);

		/* Event times are in beats, relative to start of source
		 * file. We need to convert to region-relative time, and then
		 * a session timeline time, which is defined by the time at
		 * which we last transitioned (in this case, to being active)
		 */

		const Temporal::Beats maybe_last_event_timeline_beats = transition_beats + (event.time() - region_start);

		if (maybe_last_event_timeline_beats > final_beat) {
			/* do this to "fake" having reached the end */
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 tlrr %2 >= fb %3, so at end with %4\n", index(), maybe_last_event_timeline_beats, final_beat, event));
			iter = model->end();
			break;
		}

		/* Now get samples */

		const samplepos_t timeline_samples = tmap->sample_at (maybe_last_event_timeline_beats);

		if (timeline_samples >= end_sample) {
			break;
		}

		if (in_process_context) { /* compile-time const expr */

			/* Now we have to convert to a position within the buffer we
			 * are writing to.
			 *
			 * (timeline_samples - start_sample) gives us the
			 * sample offset from the start of our run() call. But
			 * since we may be executing after another trigger in
			 * the same process() cycle, we must take dest_offset
			 * into account to get an actual buffer position.
			 */

			samplepos_t buffer_samples = (timeline_samples - start_sample) + dest_offset;

			Evoral::Event<MidiBuffer::TimeType> ev (Evoral::MIDI_EVENT, buffer_samples, event.size(), const_cast<uint8_t*>(event.buffer()), false);

			if (_gain != 1.0f && ev.is_note()) {
				ev.scale_velocity (_gain);
			}

			if (_channel_map[ev.channel()] > 0) {
				ev.set_channel (_channel_map[ev.channel()]);
			}

			if (ev.is_pgm_change() || (ev.is_cc() && ((ev.cc_number() == MIDI_CTL_LSB_BANK) || (ev.cc_number() == MIDI_CTL_MSB_BANK)))) {
				if (_patch_change[ev.channel()].is_set() || _box.ignore_patch_changes ()) {
					/* skip pgm change info in data because trigger has its own */
					++iter;
					continue;
				}
			}

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("given et %1 TS %7 rs %8 ts %2 bs %3 ss %4 do %5, inserting %6\n", maybe_last_event_timeline_beats, timeline_samples, buffer_samples, start_sample, dest_offset, ev, transition_beats, region_start));
			mb->insert_event (ev);
		}

		_box.tracker->track (event.buffer());

		last_event_beats = event.time();
		last_event_timeline_beats = maybe_last_event_timeline_beats;
		last_event_samples = timeline_samples;

		++iter;
	}


	if (in_process_context && _state == Stopping) { /* first clause is a compile-time constexpr */
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 was stopping, now stopped, resolving notes @ %2\n", index(), nframes-1));
		_box.tracker->resolve_notes (*mb, nframes-1);
	}

	if (iter == model->end()) {

		/* We reached the end */

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end, leb %2 les %3 fb %4 dl %5\n", index(), last_event_timeline_beats, last_event_samples, final_beat, data_length));

		if (last_event_timeline_beats <= final_beat) {

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 entering playout because ... leb %2 <= fb %3\n", index(), last_event_timeline_beats, final_beat));

			if (_state != Playout) {
				_state = Playout;

			}

			if (_state == Playout) {
				if (final_beat > end_beats) {
					/* not finished with playout yet, all frames covered */
					nframes = 0;
					DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 not done with playout, all frames covered\n", index()));
				} else {
					/* finishing up playout */
					samplepos_t final_processed_sample = tmap->sample_at (timepos_t (final_beat));
					nframes = orig_nframes - (final_processed_sample - start_sample);
					_loop_cnt++;
					_state = Stopped;
					DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 playout done, nf = %2 fb %3 fs %4 %5\n", index(), nframes, final_beat, final_processed_sample, start_sample));
				}
			}

		} else {

			samplepos_t final_processed_sample = tmap->sample_at (timepos_t (final_beat));
			nframes = orig_nframes - (final_processed_sample - start_sample);
			_loop_cnt++;
			_state = Stopped;
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached final event, now stopped, nf = %2 fb %3 fs %4 %5\n", index(), nframes, final_beat, final_processed_sample, start_sample));
		}

	} else {
		/* we didn't reach the end of the MIDI data, ergo we covered
		   the entire timespan passed into us.
		*/
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 did not reach end, nframes left at %2, next event is %3\n", index(), nframes, *iter));
		nframes = 0;
	}

	const samplecnt_t covered_frames = orig_nframes - nframes;

	if (_state == Stopped || _state == Stopping) {
		when_stopped_during_run (bufs, dest_offset + covered_frames);
	}

	process_index += covered_frames;

	return covered_frames;
}

/**************/

void
Trigger::make_property_quarks ()
{
	Properties::running.property_id = g_quark_from_static_string (X_("running"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for running = %1\n", Properties::running.property_id));
	Properties::follow_count.property_id = g_quark_from_static_string (X_("follow-count"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for follow_count = %1\n", Properties::follow_count.property_id));
	Properties::use_follow_length.property_id = g_quark_from_static_string (X_("use-follow-length"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for use_follow_length = %1\n", Properties::use_follow_length.property_id));
	Properties::follow_length.property_id = g_quark_from_static_string (X_("follow-length"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for follow_length = %1\n", Properties::follow_length.property_id));
	Properties::legato.property_id = g_quark_from_static_string (X_("legato"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for legato = %1\n", Properties::legato.property_id));
	Properties::velocity_effect.property_id = g_quark_from_static_string (X_("velocity-effect"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for velocity_effect = %1\n", Properties::velocity_effect.property_id));
	Properties::follow_action_probability.property_id = g_quark_from_static_string (X_("follow-action-probability"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for follow_action_probability = %1\n", Properties::follow_action_probability.property_id));
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
	Properties::cue_isolated.property_id = g_quark_from_static_string (X_("cue_isolated"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for cue_isolated = %1\n", Properties::cue_isolated.property_id));
	Properties::stretch_mode.property_id = g_quark_from_static_string (X_("stretch_mode"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for stretch_mode = %1\n", Properties::stretch_mode.property_id));
	Properties::patch_change.property_id = g_quark_from_static_string (X_("patch_change"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for patch_change = %1\n", Properties::patch_change.property_id));
	Properties::channel_map.property_id = g_quark_from_static_string (X_("channel_map"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for channel_map = %1\n", Properties::channel_map.property_id));
}

Temporal::BBT_Offset TriggerBox::_assumed_trigger_duration (4, 0, 0);
//TriggerBox::TriggerMidiMapMode TriggerBox::_midi_map_mode (TriggerBox::AbletonPush);
TriggerBox::TriggerMidiMapMode TriggerBox::_midi_map_mode (TriggerBox::SequentialNote);
int TriggerBox::_first_midi_note = 60;
std::atomic<int> TriggerBox::active_trigger_boxes (0);
TriggerBoxThread* TriggerBox::worker = 0;
CueRecords TriggerBox::cue_records (256);
std::atomic<bool> TriggerBox::_cue_recording (false);
PBD::Signal0<void> TriggerBox::CueRecordingChanged;

typedef std::map <boost::shared_ptr<Region>, boost::shared_ptr<Trigger::UIState>> RegionStateMap;
RegionStateMap enqueued_state_map;

void
TriggerBox::init ()
{
	worker = new TriggerBoxThread;
	TriggerBoxThread::init_request_pool ();
	init_pool ();
}

TriggerBox::TriggerBox (Session& s, DataType dt)
	: Processor (s, _("TriggerBox"), Temporal::BeatTime)
	, tracker (dt == DataType::MIDI ? new MidiStateTracker : 0)
	, _data_type (dt)
	, _order (-1)
	, explicit_queue (64)
	, _currently_playing (0)
	, _stop_all (false)
	, _active_scene (-1)
	, _active_slots (0)
	, _ignore_patch_changes (false)
	, _locate_armed (false)
	, _fast_fowarding (false)

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
TriggerBox::set_cue_recording (bool yn)
{
	if (yn != _cue_recording) {
		_cue_recording = yn;
		CueRecordingChanged ();
	}
}

void
TriggerBox::set_ignore_patch_changes (bool yn)
{
	if (_data_type != DataType::MIDI) {
		return;
	}
	if (yn != _ignore_patch_changes) {
		_ignore_patch_changes = yn;
	}
}

void
TriggerBox::fast_forward (CueEvents const & cues, samplepos_t transport_position)
{
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1: ffwd to %2\n", order(), transport_position));
	if (cues.empty() || !(Config->get_cue_behavior() & FollowCues) || (cues.front().time > transport_position)) {
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1: nothing to be done\n", order()));
		_locate_armed = false;
		if (tracker) {
			tracker->reset ();
		}
		return;
	}

	PBD::Unwinder<bool> uw (_fast_fowarding, true);

	using namespace Temporal;
	TempoMap::SharedPtr tmap (TempoMap::use());

	CueEvents::const_iterator c = cues.begin();
	samplepos_t pos = c->time;
	TriggerPtr prev;
	Temporal::BBT_Time start_bbt;
	samplepos_t start_samples;

	while (pos < transport_position && c != cues.end() && c->time < transport_position) {

		CueEvents::const_iterator nxt_cue = c; ++nxt_cue;

		if (c->cue == INT32_MAX) {
			/* "stop all cues" marker encountered.  This ends the
			   duration of whatever slot might have been running
			   when we hit the cue.
			*/
			prev.reset ();
			c = nxt_cue;
			continue;
		}

		TriggerPtr trig (all_triggers[c->cue]);

		if (trig->cue_isolated()) {
			c = nxt_cue;
			continue;
		}

		if (!trig->region()) {
			/* the cue-identified slot is empty for this
			   triggerbox. This effectively ends the duration of
			   whatever slot might have been running when we hit
			   the cue.
			*/
			prev.reset ();
			c = nxt_cue;
			continue;
		}

		samplepos_t limit;

		if (nxt_cue == cues.end()) {
			limit = transport_position;
		} else {
			limit = nxt_cue->time;
		}

		bool will_start = true;

		start_bbt = trig->compute_start (tmap, pos, limit, trig->quantization(), start_samples, will_start);

		if (!will_start) {
			/* trigger will not start between this cue and the next */
			c = nxt_cue;
			pos = limit;
			continue;
		}

		/* XXX need to determine when the trigger will actually start
		 * (due to its quantization)
		 */

		/* we now consider this trigger to be running. Let's see when
		 * it ends...
		 */

		samplepos_t trig_ends_at = trig->compute_end (tmap, start_bbt, start_samples).samples();

		if (nxt_cue != cues.end() && trig_ends_at >= nxt_cue->time) {
			/* trigger will be interrupted by next cue .
			 *
			 */
			trig_ends_at = tmap->sample_at (tmap->bbt_at (timepos_t (nxt_cue->time)).round_up_to_bar ());
		}

		if (trig_ends_at >= transport_position) {
			prev = trig;
			/* we're done. prev now indicates the trigger that
			   would have started most recently before the
			   transport position.
			*/
			break;
		}

		int dnt = determine_next_trigger (trig->index());

		if (dnt < 0) {
			/* no trigger follows the current one. Back to
			   looking for another cue.
			*/
			c = nxt_cue;
			continue;
		}

		prev = trig;
		pos = trig_ends_at;
		trig = all_triggers[dnt];
		c = nxt_cue;
	}

	if (pos >= transport_position || !prev) {
		/* nothing to do */
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1: no trigger to be rolled\n", order()));
		_currently_playing = 0;
		_locate_armed = false;
		if (tracker) {
			tracker->reset ();
		}
		return;
	}

	/* prev now points to a trigger that would start before
	 * transport_position and would still be running at
	 * transport_position. We need to run it in a special mode that ensures
	 * that
	 *
	 * 1) for MIDI, we know the state at transport position
	 * 2) for audio, the stretcher is in the correct state
	 */

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1: roll trigger %2 to %3\n", order(), prev->index(), transport_position));
	prev->start_and_roll_to (start_samples, transport_position);

	_currently_playing = prev;
	_locate_armed = true;
	/* currently playing is now ready to keep running at transport position
	 *
	 * Note that a MIDITrigger will have set a flag so that when we call
	 * ::run() again, it will dump its current MIDI state before anything
	 * else.
	 */
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

	/* set_region_in_worker_thread estimates a tempo, and makes some guesses about whether a clip is a one-shot or looping*/
	t->set_region_in_worker_thread (region);

	/* if we are the target of a drag&drop from another Trigger Slot, we need the name, color and other properties to carry over with the region */
	RegionStateMap::iterator rs;
	if ((rs = enqueued_state_map.find (region)) != enqueued_state_map.end()) {
		Trigger::UIState copy; copy = *(rs->second);
		t->set_ui_state(*(rs->second));
		enqueued_state_map.erase(rs);
	}

	//* always preserve the launch-style and cue_isolate status. It's likely to be right, but if it's wrong the user can "see" it's wrong anyway */
	t->set_launch_style(all_triggers[slot]->launch_style());
	t->set_cue_isolated(all_triggers[slot]->cue_isolated());

	//* if the existing slot seems to be part of a FA 'arrangement', preserve the settings */
	if (all_triggers[slot]->follow_action0().is_arrangement()) {
		t->set_follow_action0(all_triggers[slot]->follow_action0());
		t->set_follow_action1(all_triggers[slot]->follow_action1());
		t->set_follow_action_probability(all_triggers[slot]->follow_action_probability());
		t->set_quantization(all_triggers[slot]->quantization());
		//color ?
		//we could try to match the prior clip's length by playing with the follow_count and follow_length (?)
	}

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
	bool empty_changed = false;

	p = all_triggers[slot]->swap_pending (p);

	if (p) {

		if (p == Trigger::MagicClearPointerValue) {
			if (all_triggers[slot]->region()) {
				if (_active_slots) {
					_active_slots--;
				}
				if (_active_slots == 0) {
					empty_changed = true;
				}
			}
			all_triggers[slot]->clear_region ();
		} else {
			if (!all_triggers[slot]->region()) {
				if (_active_slots == 0) {
					empty_changed = true;
				}
				_active_slots++;
			}
			/* Note use of a custom delete function. We cannot
			   delete the old trigger from the RT context where the
			   trigger swap will happen, so we will ask the trigger
			   helper thread to take care of it.
			*/
			all_triggers[slot].reset (p, Trigger::request_trigger_delete);
			TriggerSwapped (slot); /* EMIT SIGNAL */
		}
	}

	if (empty_changed) {
		EmptyStatusChanged (); /* EMIT SIGNAL */
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

TriggerPtr
TriggerBox::trigger_by_id (PBD::ID check)
{
	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		if (trigger (n)->id() == check) {
			return trigger (n);
		}
	}
	return TriggerPtr();
}

void
TriggerBox::enqueue_trigger_state_for_region (boost::shared_ptr<Region> region, boost::shared_ptr<Trigger::UIState> state)
{
	enqueued_state_map.insert (std::make_pair(region, state));
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
TriggerBox::set_all_follow_action (ARDOUR::FollowAction const & fa, uint32_t fa_n)
{
	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		if (fa_n == 0) {
			all_triggers[n]->set_follow_action0 (fa);
		} else {
			all_triggers[n]->set_follow_action1 (fa);
		}
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
			if (!Config->get_default_trigger_input_port().empty ()) {
				p->connect (Config->get_default_trigger_input_port());
			}
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
		_sidechain->configure_io (in, out + ChanCount (DataType::MIDI, 1));
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
	/* check MIDI port input buffer for triggers. This is always the last
	 * MIDI buffer of the BufferSet
	 */

	MidiBuffer& mb (bufs.get_midi (bufs.count().n_midi() - 1 /* due to zero-based index*/));

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

			if (t->velocity_effect() != 0.0) {
				/* if MVE is zero, MIDI velocity has no
				   impact on gain. If it is small, it
				   has a small effect on gain. As it
				   approaches 1.0, it has full control
				   over the trigger gain.
				*/
				t->set_velocity_gain (1.0 - (t->velocity_effect() * (*ev).velocity() / 127.f));
			}
			t->bang ();

		} else if ((*ev).is_note_off()) {

			t->unbang ();
		}
	}
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

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("**** Triggerbox::run() for %6, ss %1 es %2 sb %3 eb %4 bpm %5 nf %7\n", start_sample, end_sample, __start_beats, __end_beats, __bpm, order(), nframes));
	}
#endif

	bool allstop = _requests.stop_all.exchange (false);

	/* STEP TWO: if latency compensation tells us that we haven't really
	 * started yet, do nothing, because we can't make sense of a negative
	 * start sample time w.r.t the tempo map.
	 */

	if (start_sample < 0) {
		return;
	}

	/* STEP THREE: triggers in audio tracks need a MIDI sidechain to be
	 * able to receive inbound MIDI for triggering etc. This needs to run
	 * before anything else, since we may need data just received to launch
	 * a trigger (or stop it)
	 */

	if (_sidechain) {
		_sidechain->run (bufs, start_sample, end_sample, speed, nframes, true);
	}

	bool    was_recorded;
	int32_t cue_bang = _session.first_cue_within (start_sample, end_sample, was_recorded);

	if (!_cue_recording || !was_recorded) {

		if (cue_bang == INT32_MAX) {

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 sees STOP ALL!\n", order()));

			/* reached a "stop all cue-launched cues from playing"
			 * marker.The stop is quantized, not immediate.
			 */

			if (_currently_playing && _currently_playing->cue_launched()) {
				_currently_playing->unbang ();
			}

		} else if (cue_bang >= 0) {
			_active_scene = cue_bang;
		}

	}

	/* STEP SIX: if at this point there is an active cue, make it trigger
	 * our corresponding slot
	 */

	if (_active_scene >= 0) {
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("tb noticed active scene %1\n", _active_scene));
		if (_active_scene < (int32_t) all_triggers.size()) {
			if (!all_triggers[_active_scene]->cue_isolated()) {
				if (all_triggers[_active_scene]->region()) {
					all_triggers[_active_scene]->bang ();
				} else {
					stop_all_quantized ();  //empty slot, this should work as a Stop for the running clips
					//TODO:  can we set a flag so the UI reports that we are stopping?
				}
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

	for (auto & trig : all_triggers) {
		trig->process_state_requests (bufs, nframes - 1);
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
			_currently_playing->startup (bufs, 0);
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
		DEBUG_TRACE (DEBUG::Triggers, "nothing currently playing 1, reset stop_all to false\n");
		_stop_all = false;

		/* nobody is active, but we should catch up on changes
		 * requested by the UI
		 */

		for (auto & trig : all_triggers) {
			trig->update_properties();
		}

		return;
	}

	/* some trigger is active, but the others should catch up on changes
	 * requested by the UI
	 */

	for (auto & trig : all_triggers) {
		if (trig != _currently_playing) {
			trig->update_properties();
		}
	}

	/* transport must be active for triggers */

	if (!_locate_armed) {
		if (!_session.transport_state_rolling() && !allstop) {
			_session.start_transport_from_trigger ();
		}
	} else {

		/* _locate_armed is true, so _currently_playing has been
		   fast-forwarded to our position, and is ready to
		   play. However, for MIDI triggers, we may need to dump a
		   bunch of state into our BufferSet to ensure that the state
		   of things matches the way it would have been had we actually
		   played the trigger/slot from the start.
		*/

		if (_session.transport_state_rolling()) {
			if (tracker && bufs.count().n_midi()) {
				tracker->flush (bufs.get_midi (0), 0, true);
			}
			_locate_armed = false;
		} else {
			return;
		}
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

						nxt->startup (bufs, dest_offset);
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

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("currently playing: %1, state now %2 stop all ? %3\n", _currently_playing->name(), enum_2_string (_currently_playing->state()), _stop_all));

		/* if we're not in the process of stopping all active triggers,
		 * but the current one has stopped, decide which (if any)
		 * trigger to play next.
		 */

		if (_currently_playing->state() == Trigger::Stopped) {

			if (!_stop_all && !_currently_playing->explicitly_stopped()) {

				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 has stopped, need next...\n", _currently_playing->name()));

				if (_currently_playing->will_follow()) {
					int n = determine_next_trigger (_currently_playing->index());
					Temporal::BBT_Offset start_quantization;
					std::cerr << "dnt = " << n << endl;
					if (n < 0) {
						DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 finished, no next trigger\n", _currently_playing->name()));
						_currently_playing = 0;
						PropertyChanged (Properties::currently_playing);
						break; /* no triggers to come next, break out of nframes loop */
					}
					if ((int) _currently_playing->index() == n) {
						start_quantization = Temporal::BBT_Offset ();
						DEBUG_TRACE (DEBUG::Triggers, string_compose ("switching to next trigger %1, will use start immediately \n", all_triggers[n]->name()));
					} else {
						DEBUG_TRACE (DEBUG::Triggers, string_compose ("switching to next trigger %1\n", all_triggers[n]->name()));
					}
					_currently_playing = all_triggers[n];
					_currently_playing->startup (bufs, dest_offset, start_quantization);
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

		frames_covered = _currently_playing->run (bufs, start_sample, end_sample, start_beats, end_beats, nframes, dest_offset, bpm);

		nframes -= frames_covered;
		start_sample += frames_covered;
		dest_offset += frames_covered;

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("trig %1 ran, covered %2 state now %3 nframes now %4\n",
		                                              _currently_playing->name(), frames_covered, enum_2_string (_currently_playing->state()), nframes));

	}

	if (!_currently_playing) {
		DEBUG_TRACE (DEBUG::Triggers, "nothing currently playing 2, reset stop_all to false\n");
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
	std::vector<int32_t> possible_targets;

	possible_targets.reserve (default_triggers_per_box);

	/* count number of triggers that can actually be run (i.e. they have a region) */

	for (uint32_t n = 0; n < all_triggers.size(); ++n) {
		if (all_triggers[n]->region()) {
			runnable++;
		}
	}

	if (runnable == 0 || !all_triggers[current]->region()) {
		return -1;
	}

	if (all_triggers[current]->follow_action0 ().type == FollowAction::None) {
		/* when left follow action is disabled, no follow action */
		return -1;
	}

	/* decide which of the two follow actions we're going to use (based on
	 * random number and the probability setting)
	 */

	int r = _pcg.rand (100); // 0 .. 99
	FollowAction fa;

	if (r >= all_triggers[current]->follow_action_probability()) {
		fa = all_triggers[current]->follow_action0 ();
	} else {
		fa = all_triggers[current]->follow_action1 ();
	}

	/* first switch: deal with the "special" cases where we either do
	 * nothing or just repeat the current trigger
	 */

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("choose next trigger using follow action %1 given prob %2 and rnd %3\n", fa.to_string(), all_triggers[current]->follow_action_probability(), r));

	if (fa.type == FollowAction::Stop) {
		return -1;
	}

	if (runnable == 1) {
		/* there's only 1 runnable trigger, so the "next" one
		   is the same as the current one.
		*/
		return current;
	}

	/* second switch: handle the "real" follow actions */

	switch (fa.type) {
	case FollowAction::None:
		return -1;

	case FollowAction::Again:
		return current;

	case FollowAction::ForwardTrigger:
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

	case FollowAction::ReverseTrigger:
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

	case FollowAction::FirstTrigger:
		for (n = 0; n < all_triggers.size(); ++n) {
			if (all_triggers[n]->region() && !all_triggers[n]->active ()) {
				return n;
			}
		}
		break;
	case FollowAction::LastTrigger:
		for (int i = all_triggers.size() - 1; i >= 0; --i) {
			if (all_triggers[i]->region() && !all_triggers[i]->active ()) {
				return i;
			}
		}
		break;

	case FollowAction::JumpTrigger:
		for (std::size_t n = 0; n < default_triggers_per_box; ++n) {
			if (fa.targets.test (n) && all_triggers[n]->region()) {
				possible_targets.push_back (n);
			}
		}
		if (possible_targets.empty()) {
			return 1;
		}
		return possible_targets[_pcg.rand (possible_targets.size())];


	/* NOTREACHED */
	case FollowAction::Stop:
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
	node.set_property (X_("ignore_patch_changes"), _ignore_patch_changes);

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
	node.get_property (X_("ignore_patch_changes"), _ignore_patch_changes);

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
			if (trig->region ()) {
				_active_slots++;
			}
		}
	}

	/* sidechain is a Processor (IO) */
	XMLNode* scnode = node.child (Processor::state_node_name.c_str ());
	if (scnode) {
		add_midi_sidechain ();
		assert (_sidechain);
		if (!regenerate_xml_or_string_ids ()) {
			_sidechain->set_state (*scnode, version);
		} else {
			update_sidechain_name ();
		}
	}

	/* Since _active_slots may have changed, we could consider sending
	 * EmptyStatusChanged, but for now we don't consider ::set_state() to
	 * be used except at session load.
	 */

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

void
TriggerBox::realtime_handle_transport_stopped ()
{
	Processor::realtime_handle_transport_stopped ();
	stop_all ();
	_currently_playing = 0;
}

void
TriggerBox::non_realtime_transport_stop (samplepos_t now, bool /*flush*/)
{
	fast_forward (_session.cue_events(), now);
}

void
TriggerBox::non_realtime_locate (samplepos_t now)
{
	fast_forward (_session.cue_events(), now);
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
