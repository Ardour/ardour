/*
 * Copyright (C) 2021-2022 Paul Davis <paul@linuxaudiosystems.com>
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
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <sstream>

#include <glibmm.h>

#include <rubberband/RubberBandStretcher.h>

#include "pbd/basename.h"
#include "pbd/compose.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"
#include "pbd/types_convert.h"
#include "pbd/unwind.h"

#include "temporal/tempo.h"

#include "ardour/async_midi_port.h"
#include "ardour/auditioner.h"
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
		PBD::PropertyDescriptor<uint32_t> queued;
		PBD::PropertyDescriptor<uint32_t> follow_count;
		PBD::PropertyDescriptor<int> follow_action_probability;
		PBD::PropertyDescriptor<float> velocity_effect;
		PBD::PropertyDescriptor<gain_t> gain;
		PBD::PropertyDescriptor<bool> stretchable;
		PBD::PropertyDescriptor<bool> cue_isolated;
		PBD::PropertyDescriptor<bool> allow_patch_changes;
		PBD::PropertyDescriptor<Trigger::StretchMode> stretch_mode;
		PBD::PropertyDescriptor<bool> tempo_meter;  /* only to transmit updates, not storage */
		PBD::PropertyDescriptor<bool> patch_change;  /* only to transmit updates, not storage */
		PBD::PropertyDescriptor<bool> channel_map;  /* only to transmit updates, not storage */
		PBD::PropertyDescriptor<bool> used_channels;  /* only to transmit updates, not storage */
	}
}

PropertyChange
TriggerBox::all_trigger_props()
{
	PropertyChange all;
	all.add(Properties::name);
	all.add(Properties::color);
	all.add(Properties::legato);
	all.add(Properties::use_follow_length);
	all.add(Properties::quantization);
	all.add(Properties::follow_length);
	all.add(Properties::follow_count);
	all.add(Properties::launch_style);
	all.add(Properties::follow_action0);
	all.add(Properties::follow_action1);
	all.add(Properties::follow_action_probability);
	all.add(Properties::velocity_effect);
	all.add(Properties::gain);
	all.add(Properties::stretchable);
	all.add(Properties::cue_isolated);
	all.add(Properties::allow_patch_changes);
	all.add(Properties::stretch_mode);
	all.add(Properties::tempo_meter);
	all.add(Properties::stretchable);
	all.add(Properties::patch_change);
	all.add(Properties::channel_map);
	all.add(Properties::used_channels);

	return all;
}

std::string
ARDOUR::cue_marker_name (int32_t index)
{
	/* this somewhat weird code structure is intended to allow for easy and
	 * correct translation.
	 */

	using std::string;

	if (index == CueRecord::stop_all) {
		/* this is a reasonable "stop" icon */
		return string (X_(u8"\u25a1"));
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
PBD::Signal2<void,PropertyChange,Trigger*> Trigger::TriggerPropertyChange;

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
	, _allow_patch_changes (Properties::allow_patch_changes, true)
	, _stretch_mode (Properties::stretch_mode, Trigger::Crisp)
	, _name (Properties::name, "")
	, _color (Properties::color, 0xBEBEBEFF)
	, process_index (0)
	, final_processed_sample (0)
	, _box (b)
	, _state (Stopped)
	, _playout (false)
	, _bang (0)
	, _unbang (0)
	, _index (n)
	, _loop_cnt (0)
	, _ui (0)
	, _explicitly_stopped (false)
	, _pending_velocity_gain (1.0)
	, _velocity_gain (1.0)
	, _cue_launched (false)
	, _used_channels (Evoral::SMF::UsedChannels())
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
	add_property (_allow_patch_changes);
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
	state.allow_patch_changes = _allow_patch_changes;
	state.cue_isolated = _cue_isolated;
	state.stretch_mode = _stretch_mode;

	state.name = _name;
	state.color = _color;

	state.used_channels = used_channels();
	for (int i = 0; i<16; i++) {
		if (_patch_change[i].is_set()) {
			state.patch_change[i] = _patch_change[i];
		}
	}

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
		_allow_patch_changes = ui_state.allow_patch_changes;
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
		 *   ...but in the interim, we have likely been assigned a name in a separate thread (importing the region)
		 *   ...so don't overwrite our name if ui_state.name is empty
		 */
		if (ui_state.name != "" ) {
			_name = ui_state.name;
		}

		_used_channels = ui_state.used_channels;

		for (int chan = 0; chan<16; chan++) {
			if (ui_state.patch_change[chan].is_set()) {
				_patch_change[chan] = ui_state.patch_change[chan];
			}
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
	ui_state.allow_patch_changes = _allow_patch_changes;
	ui_state.stretch_mode = _stretch_mode;
	ui_state.name = _name;
	ui_state.color = _color;

	ui_state.used_channels = _used_channels;
	for (int i = 0; i<16; i++) {
		if (_patch_change[i].is_set()) {
			ui_state.patch_change[i] = _patch_change[i];
		}
	}
}

void
Trigger::send_property_change (PropertyChange pc)
{
	if (_box.fast_forwarding()) {
		return;
	}

	PropertyChanged (pc);
	/* emit static signal for global observers */
	TriggerPropertyChange (pc, this);
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

/* these params are central to the triggerbox behavior and must only be applied at ::retrigger() via ::update_properties()  */
TRIGGER_UI_SET (cue_isolated,bool)
TRIGGER_UI_SET (stretchable, bool)
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
	ui_state.name = val; \
	unsigned int g = ui_state.generation.load(); \
	do { ui_state.name = val; } while (!ui_state.generation.compare_exchange_strong (g, g+1)); \
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("trigger %1 property& cas-set: %2 gen %3\n", index(), _ ## name.property_name(), ui_state.generation.load())); \
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
	ui_state.name = val; \
	unsigned int g = ui_state.generation.load(); \
	do { ui_state.name = val; } while (!ui_state.generation.compare_exchange_strong (g, g+1)); \
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("trigger %1 property& cas-set: %2 gen %3\n", index(), _ ## name.property_name(), ui_state.generation.load())); \
	send_property_change (Properties::name); /* EMIT SIGNAL */ \
	_box.session().set_dirty (); \
} \
type \
Trigger::name () const \
{ \
	return _ ## name; \
}

/* these params can take effect outside the scope of ::retrigger
 * BUT they still need to set the ui_state variables as well as the associated member variable
 * otherwise an incoming ui_state change will overwrite your changes
 * */
TRIGGER_DIRECT_SET_CONST_REF (name, std::string)
TRIGGER_DIRECT_SET (color, color_t)
TRIGGER_DIRECT_SET (gain, gain_t)
TRIGGER_DIRECT_SET (allow_patch_changes, bool)
/* patch_change[] is implemented manually but it needs to operate the same as above */

void
Trigger::set_ui (void* p)
{
	_ui = p;
}

void
Trigger::bang (float velocity)
{
	if (!_region) {
		return;
	}
	_pending_velocity_gain = velocity;
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
Trigger::get_state () const
{
	XMLNode* node = new XMLNode (X_("Trigger"));

	/* XXX possible locking problems here if trigger is active, because
	 * properties could be overwritten
	 */

	for (OwnedPropertyList::iterator i = _properties->begin(); i != _properties->end(); ++i) {
		i->second->get_value (*node);
	}

	node->set_property (X_("index"), _index);

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

	std::shared_ptr<Region> r = RegionFactory::region_by_id (rid);

	if (r) {
		set_region (r, false);  //this results in a call to estimate_tempo()
	}

	double tempo;
	if (node.get_property (X_("segment-tempo"), tempo)) {
		/* this is the user-selected tempo which overrides estimated_tempo */
		set_segment_tempo(tempo);
	}

	node.get_property (X_("index"), _index);
	set_values (node);

	return 0;
}

bool
Trigger::internal_use_follow_length () const
{
	return (_follow_action0.val().type != FollowAction::None) && _use_follow_length;
}

void
Trigger::set_region (std::shared_ptr<Region> r, bool use_thread)
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
Trigger::set_region_internal (std::shared_ptr<Region> r)
{
	/* No whole file regions in the triggerbox, just like we do not allow
	 * them in playlists either.
	 */

	if (r->whole_file ()) {
		_region = RegionFactory::create (r, r->derive_properties ());
	} else {
		_region = r;
	}
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
	_playout = false;
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
Trigger::startup_from_ffwd (BufferSet& bufs, uint32_t loop_cnt)
{
	_startup (bufs, 0, _quantization);
	_loop_cnt = loop_cnt;
	_cue_launched = true;
	/* if we just fast-forwarded, any pending stop request is irrelevant */
	_requests.stop = false;
}

void
Trigger::_startup (BufferSet& bufs, pframes_t dest_offset, Temporal::BBT_Offset const & start_quantization)
{
	_state = WaitingToStart;
	_playout = false;
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
Trigger::shutdown_from_fwd ()
{
	_state = Stopped;
	_playout = false;
	_loop_cnt = 0;
	_cue_launched = false;
	_pending_velocity_gain = _velocity_gain = 1.0;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 shuts down\n", name()));
	send_property_change (ARDOUR::Properties::running);
}

void
Trigger::shutdown (BufferSet& /*bufs*/, pframes_t /*dest_offset*/)
{
	shutdown_from_fwd ();
}

void
Trigger::jump_start()
{
	/* this is used when we start a new trigger in legato mode. We do not
	   wait for quantization.
	*/
	_state = Running;
	/* XXX set expected_end_sample */
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 jump_start() requested state %2\n", index(), enum_2_string (_state)));
	send_property_change (ARDOUR::Properties::running);
}

void
Trigger::jump_stop (BufferSet& bufs, pframes_t dest_offset)
{
	/* this is used when we start a new trigger in legato mode. We do not
	   wait for quantization.
	*/
	shutdown (bufs, dest_offset);
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 jump_stop() requested state %2\n", index(), enum_2_string (_state)));
	send_property_change (ARDOUR::Properties::running);
}

void
Trigger::begin_stop (bool explicit_stop)
{
	if (_state == Stopped) {
		return;  /* nothing to do */
	}

	/* this is used when we start a tell a currently playing trigger to
	   stop, but wait for quantization first.
	*/
	_state = WaitingToStop;
	_explicitly_stopped = explicit_stop;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 begin_stop() requested state %2\n", index(), enum_2_string (_state)));
	send_property_change (ARDOUR::Properties::running);
}

void
Trigger::stop_quantized ()
{
	begin_stop(true);
}

void
Trigger::begin_switch (TriggerPtr nxt)
{
	/* this is used when we start a tell a currently playing trigger to
	   stop, but wait for quantization first.
	*/
	_state = WaitingToSwitch;
	_nxt_quantization = nxt->_quantization;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 begin_switch() requested state %2\n", index(), enum_2_string (_state)));
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
			switch (launch_style()) {
			case OneShot:
				/* do nothing, just let it keep playing */
				break;
			case ReTrigger:
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 oneshot %2 => %3\n", index(), enum_2_string (Running), enum_2_string (WaitingForRetrigger)));
				_state = WaitingForRetrigger;
				send_property_change (ARDOUR::Properties::running);
				break;
			case Toggle:
				stop_quantized ();
				break;
			case Gate:
			case Repeat:
				if (_box.active_scene() >= 0) {
					std::cerr << "should not happen, cue launching but launch_style() said " << enum_2_string (launch_style()) << std::endl;
				} else {
					DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 gate/repeat => %3\n", index(), enum_2_string (Running), enum_2_string (WaitingToStop)));
					stop_quantized ();
				}
			}
			break;

		case Stopped:
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 %2 stopped => %3\n", index(), enum_2_string (Stopped), enum_2_string (WaitingToStart)));
			_box.queue_explict (index());
			_cue_launched = (_box.active_scene() >= 0);
			break;

		case WaitingToStart:
		case WaitingToStop:
		case WaitingToSwitch:
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
		case WaitingToStart:
			switch (launch_style()) {
			case OneShot:
			case ReTrigger:
			case Toggle:
				/* unbang does nothing, just let it keep playing */
				break;
			case Gate:
				request_stop ();  /* stop now */
				break;
			case Repeat:
				stop_quantized ();
				break;
			}
			break;

		case Stopped:
		case Stopping: /* theoretically not possible */
		case WaitingToStop:
		case WaitingToSwitch:
		case WaitingForRetrigger:
			/* do nothing */
			break;
		}
	}
}

Temporal::BBT_Argument
Trigger::compute_start (Temporal::TempoMap::SharedPtr const & tmap, samplepos_t start, samplepos_t end, Temporal::BBT_Offset const & q, samplepos_t& start_samples, bool& will_start)
{
	Temporal::Beats start_beats (tmap->quarters_at (timepos_t (start)));
	Temporal::Beats end_beats (tmap->quarters_at (timepos_t (end)));

	Temporal::BBT_Argument t_bbt;
	Temporal::Beats t_beats;

	if (!compute_quantized_transition (start, start_beats, end_beats, t_bbt, t_beats, start_samples, tmap, q)) {
		will_start = false;
		return Temporal::BBT_Argument ();
	}

	will_start = true;
	return t_bbt;
}

bool
Trigger::compute_quantized_transition (samplepos_t start_sample, Temporal::Beats const & start_beats, Temporal::Beats const & end_beats,
                                       Temporal::BBT_Argument& t_bbt, Temporal::Beats& t_beats, samplepos_t& t_samples,
                                       Temporal::TempoMap::SharedPtr const & tmap, Temporal::BBT_Offset const & q)
{
	/* XXX need to use global grid here is quantization == zero */

	/* Given the value of @p start, determine, based on the
	 * quantization, the next time for a transition.
	 */

	Temporal::BBT_Argument possible_bbt;
	Temporal::Beats possible_beats;
	samplepos_t possible_samples;

	if (q < Temporal::BBT_Offset (0, 0, 0)) {
		/* negative quantization == do not quantize */

		possible_samples = start_sample;
		possible_beats = start_beats;
		possible_bbt = tmap->bbt_at (possible_beats);

	} else if (q.bars == 0) {

		possible_beats = start_beats.round_up_to_multiple (Temporal::Beats (q.beats, q.ticks));
		possible_bbt = tmap->bbt_at (possible_beats);
		possible_samples = tmap->sample_at (possible_beats);

	} else {

		possible_bbt = tmap->bbt_at (timepos_t (start_beats));
		possible_bbt = Temporal::BBT_Argument (possible_bbt.reference(), possible_bbt.round_up_to_bar ());
		/* bars are 1-based; 'every 4 bars' means 'on bar 1, 5, 9, ...' */
		possible_bbt.bars = 1 + ((possible_bbt.bars-1) / q.bars * q.bars);
		possible_beats = tmap->quarters_at (possible_bbt);
		possible_samples = tmap->sample_at (possible_bbt);

	}

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%6/%1 quantized with %5 transition at %2, sb %3 eb %4 (would be %7 aka %8)\n", index(), possible_samples, start_beats, end_beats, q, _box.order(), possible_bbt, possible_beats));

	/* See if this time falls within the range of time given to us */

	if (possible_beats < start_beats || possible_beats > end_beats) {
		/* transition time not reached */
		return false;
	}

	t_bbt = possible_bbt;
	t_beats = possible_beats;
	t_samples = possible_samples;

	return true;
}

pframes_t
Trigger::compute_next_transition (samplepos_t start_sample, Temporal::Beats const & start, Temporal::Beats const & end, pframes_t nframes,
                                  Temporal::BBT_Argument& t_bbt, Temporal::Beats& t_beats, samplepos_t& t_samples,
                                  Temporal::TempoMap::SharedPtr const & tmap)
{
	using namespace Temporal;

	/* In these states, we are not waiting for a transition */

	if (_state == Stopped || _state == Running || _state == Stopping) {
		/* no transition */
		return 0;
	}

	BBT_Offset q (_start_quantization);

	/* Clips don't stop on their own quantize; in Live they stop on the Global Quantize setting; we will choose 1 bar (Live's default) for now */
#warning when Global Quantize is implemented, use that instead of '1 bar' here
	if (_state == WaitingToStop) {

		q = BBT_Offset(1,0,0);

	} else if (_state == WaitingToSwitch) {

		q = _nxt_quantization;

	}

	if (!compute_quantized_transition (start_sample, start, end, t_bbt, t_beats, t_samples, tmap, q)) {
		/* no transition */
		return 0;
	}

	switch (_state) {
	case WaitingToStop:
	case WaitingToSwitch:
		nframes = t_samples - start_sample;
		break;

	case WaitingToStart:
		nframes -= std::max (samplepos_t (0), t_samples - start_sample);
		break;

	case WaitingForRetrigger:
		break;

	default:
		fatal << string_compose (_("programming error: %1 %2 %3"), "impossible trigger state (", enum_2_string (_state), ") in ::adjust_nframes()") << endmsg;
		abort();
	}

	return nframes;
}

void
Trigger::maybe_compute_next_transition (samplepos_t start_sample, Temporal::Beats const & start, Temporal::Beats const & end, pframes_t& nframes, pframes_t&  quantize_offset)
{
	using namespace Temporal;

	/* This should never be called by a stopped trigger */

	assert (_state != Stopped);

	/* In these states, we are not waiting for a transition */

	if ((_state == Running) || (_state == Stopping)) {
		/* will cover everything */
		return;
	}

	Temporal::BBT_Argument transition_bbt;
	TempoMap::SharedPtr tmap (TempoMap::use());

	if (!compute_next_transition (start_sample, start, end, nframes, transition_bbt, transition_beats, transition_samples, tmap)) {
		return;
	}

	Temporal::Beats elen_ignored;

	/* transition time has arrived! let's figure out what're doing:
	 * stopping, starting, retriggering
	 */

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 in range, should start/stop at %2 aka %3\n", index(), transition_samples, transition_beats));

	switch (_state) {

	case WaitingToStop:
	case WaitingToSwitch:
		_state = Stopping;
		send_property_change (ARDOUR::Properties::running);

		/* trigger will reach it's end somewhere within this
		 * process cycle, so compute the number of samples it
		 * should generate.
		 */

		nframes = transition_samples - start_sample;

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1/%2 will stop somewhere in the middle of run(), specifically at %3 (%4) vs expected end at %5\n", index(), name(), transition_beats, transition_samples, expected_end_sample));

		/* offset within the buffer(s) for output remains
		   unchanged, since we will write from the first
		   location corresponding to start
		*/
		break;

	case WaitingToStart:
		retrigger ();
		_state = Running;
		(void) compute_end (tmap, transition_bbt, transition_samples, elen_ignored);
		send_property_change (ARDOUR::Properties::running);

		/* trigger will start somewhere within this process
		 * cycle. Compute the sample offset where any audio
		 * should end up, and the number of samples it should generate.
		 */

		quantize_offset = std::max (samplepos_t (0), transition_samples - start_sample);
		nframes -= quantize_offset;

		break;

	case WaitingForRetrigger:
		retrigger ();
		_state = Running;
		(void) compute_end (tmap, transition_bbt, transition_samples, elen_ignored);
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

				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 loop cnt %2 satisfied, now stopped with ls %3\n", index(), _follow_count, enum_2_string (launch_style())));
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
                                                                  pframes_t nframes, pframes_t dest_offset, double bpm, pframes_t&), uint32_t cnt)
{
	const pframes_t block_size = AudioEngine::instance()->samples_per_cycle ();
	BufferSet bufs;

	/* no need to allocate any space for BufferSet because we call
	   audio_run<false>() which is guaranteed to never use the buffers.

	   AudioTrigger::_startup() also does not use BufferSet (MIDITrigger
	   does, and we use virtual functions so the argument list is the same
	   for both, even though only the MIDI case needs the BufferSet).
	*/

	startup_from_ffwd (bufs, 0);
	_loop_cnt = cnt;
	_cue_launched = true;

	samplepos_t pos = start_pos;
	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());
	pframes_t quantize_offset;

	while (pos < end_position) {
		pframes_t nframes = std::min (block_size, (pframes_t) (end_position - pos));
		Temporal::Beats start_beats = tmap->quarters_at (timepos_t (pos));
		Temporal::Beats end_beats = tmap->quarters_at (timepos_t (pos+nframes));
		const double bpm = tmap->quarters_per_minute_at (timepos_t (start_beats));

		pframes_t n = (trigger.*run_method) (bufs, pos, pos+nframes, start_beats, end_beats, nframes, 0, bpm, quantize_offset);

		/* We could have reached the end. Check and restart, because
		 * TriggerBox::fast_forward() already determined that we are
		 * the active trigger at @p end_position
		 */

		if (_state == Stopped) {
			retrigger ();
			_state = WaitingToStart;
			_cue_launched = true;
		}

		pos += n;
		pos += quantize_offset;
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
	if (!_region) {
		_segment_tempo = 0;
		return;
	}

	if (t<=0.) {
		/*special case: we're told the file has no defined tempo.
		 * this can happen from crazy user input (0 beat length or somesuch), or if estimate_tempo() fails entirely
		 * in either case, we need to make a sensible _beatcnt, and that means we need a tempo */
		const double seconds = (double) data.length  / _box.session().sample_rate();
		double beats = ceil(4. * 120. * (seconds/60.0));  //how many (rounded up) 16th-notes would this be at 120bpm?
		beats /= 4.;  //convert to quarter notes
		t = beats / (seconds/60); /* our operating tempo. note that _estimated_tempo probably retains the 0bpm */
	}

	if (_segment_tempo != t) {

		_segment_tempo = t;

		/*beatcnt is a derived property from segment tempo and the file's length*/
		const double seconds = (double) data.length  / _box.session().sample_rate();
		_beatcnt = _segment_tempo * (seconds/60.0);

		/*initialize follow_length to match the length of the clip */
		_follow_length = Temporal::BBT_Offset (0, _beatcnt, 0);

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
AudioTrigger::get_state () const
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

	/* we've changed our internal values; we need to update our queued UIState or they will be lost when UIState is applied */
	copy_to_ui_state ();

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
AudioTrigger::start_and_roll_to (samplepos_t start_pos, samplepos_t end_position, uint32_t cnt)
{
	Trigger::start_and_roll_to<AudioTrigger> (start_pos, end_position, *this, &AudioTrigger::audio_run<false>, cnt);
}

timepos_t
AudioTrigger::compute_end (Temporal::TempoMap::SharedPtr const & tmap, Temporal::BBT_Time const & transition_bbt, samplepos_t transition_sample, Temporal::Beats & effective_length)
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

	const Temporal::BBT_Argument transition_bba (superclock_t (0), transition_bbt);

	samplepos_t end_by_follow_length = tmap->sample_at (tmap->bbt_walk (transition_bba, _follow_length));
	samplepos_t end_by_data_length = transition_sample + (data.length - _start_offset);
	/* this could still blow up if the data is less than 1 tick long, but
	   we should handle that elsewhere.
	*/
	const Temporal::Beats bc (Temporal::Beats::from_double (_beatcnt));
	samplepos_t end_by_beatcnt = tmap->sample_at (tmap->bbt_walk (transition_bba, Temporal::BBT_Offset (0, bc.get_beats(), bc.get_ticks())));

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

	effective_length = tmap->quarters_at_sample (transition_sample + final_processed_sample) - tmap->quarters_at_sample (transition_sample);

	_transition_bbt = transition_bbt;

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
AudioTrigger::set_region_in_worker_thread (std::shared_ptr<Region> r)
{
	assert (!active());

	std::shared_ptr<AudioRegion> ar = std::dynamic_pointer_cast<AudioRegion> (r);

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

	if (_estimated_tempo == 0.) {
		_stretchable = false;
		_quantization = Temporal::BBT_Offset (1, 0, 0);
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

	/* we've changed our internal values; we need to update our queued UIState or they will be lost when UIState is applied */
	copy_to_ui_state ();

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
					}
				}
			}
		}

		if (text_tempo < 0) {

			breakfastquay::MiniBPM mbpm (_box.session().sample_rate());

			_estimated_tempo = mbpm.estimateTempoOfSamples (data[0], data.length);

			//cerr << name() << "MiniBPM Estimated: " << _estimated_tempo << " bpm from " << (double) data.length / _box.session().sample_rate() << " seconds\n";
		}
	}

	const double seconds = (double) data.length  / _box.session().sample_rate();

	/* now check the determined tempo and force it to a value that gives us
	   an integer beat/quarter count. This is a heuristic that tries to
	   avoid clips that slightly over- or underrun a quantization point,
	   resulting in small or larger gaps in output if they are repeating.
	*/

	if ((_estimated_tempo != 0.)) {
		/* fractional beatcnt */
		double maybe_beats = (seconds / 60.) * _estimated_tempo;
		double beatcount = round (maybe_beats);

		/* the vast majority of third-party clips are 1,2,4,8, or 16-bar 'beats'.
		 *  Given no other metadata, it makes things 'just work' if we assume 4/4 time signature, and power-of-2 bars  (1,2,4,8 or 16)
		 *  TODO:  someday we could provide a widget for users who have unlabeled, un-metadata'd, clips that they *know* are 3/4 or 5/4 or 11/4 */
		{
			double barcount = round (beatcount/4);
			if (barcount <= 18) {  /* why not 16 here? fuzzy logic allows minibpm to misjudge the clip a bit */
				for (int pwr = 0; pwr <= 4; pwr++) {
					float bc = pow(2,pwr);
					if (barcount <= bc) {
						barcount = bc;
						break;
					}
				}
			}
			beatcount = round(barcount * 4);
		}

		DEBUG_RESULT (double, est, _estimated_tempo);
		_estimated_tempo = beatcount / (seconds/60.);
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("given original estimated tempo %1, rounded beatcnt is %2 : resulting in working bpm = %3\n", est, _beatcnt, _estimated_tempo));

		/* initialize our follow_length to match the beatcnt ... user can later change this value to have the clip end sooner or later than its data length */
		set_follow_length(Temporal::BBT_Offset( 0, rint(beatcount), 0));
	}

#if 0
	cerr << "estimated tempo: " << _estimated_tempo << endl;
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

	std::shared_ptr<AudioRegion> ar (std::dynamic_pointer_cast<AudioRegion> (_region));
	const uint32_t nchans = std::min (_box.input_streams().n_audio(), ar->n_channels());

	//map our internal enum to a rubberband option
	RubberBandStretcher::Option ro = RubberBandStretcher::Option (0);
	switch (_stretch_mode) {
		case Trigger::Crisp  : ro = RubberBandStretcher::OptionTransientsCrisp; break;
		case Trigger::Mixed  : ro = RubberBandStretcher::OptionTransientsMixed; break;
		case Trigger::Smooth : ro = RubberBandStretcher::OptionTransientsSmooth; break;
	}

	RubberBandStretcher::Options options = RubberBandStretcher::Option (RubberBandStretcher::OptionProcessRealTime |
	                                                                    ro);

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
AudioTrigger::load_data (std::shared_ptr<AudioRegion> ar)
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
                         pframes_t nframes, pframes_t dest_offset, double bpm, pframes_t& quantize_offset)
{
	std::shared_ptr<AudioRegion> ar = std::dynamic_pointer_cast<AudioRegion>(_region);
	/* We do not modify the I/O of our parent route, so we process only min (bufs.n_audio(),region.channels()) */
	const uint32_t nchans = (in_process_context ? std::min (bufs.count().n_audio(), ar->n_channels()) : ar->n_channels());
	int avail = 0;
	BufferSet* scratch;
	std::unique_ptr<BufferSet> scratchp;
	std::vector<Sample*> bufp(nchans);
	const bool do_stretch = stretching() && _segment_tempo > 1;

	quantize_offset = 0;

	/* see if we're going to start or stop or retrigger in this run() call */
	maybe_compute_next_transition (start_sample, start, end, nframes, quantize_offset);
	const pframes_t orig_nframes = nframes;

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1/%2 after checking for transition, state = %3, start = %9 will stretch %4, nf will be %5 of %6, dest_offset %7 q-offset %8\n",
	                                              index(), name(), enum_2_string (_state), do_stretch, nframes,  orig_nframes, dest_offset, quantize_offset, start_sample));

	dest_offset += quantize_offset;

	switch (_state) {
	case Stopped:
	case WaitingForRetrigger:
	case WaitingToStart:
		/* did everything we could do */
		return nframes;
	case Running:
	case WaitingToStop:
	case WaitingToSwitch:
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

	if (do_stretch && !_playout) {

		const double stretch = _segment_tempo / bpm;
		_stretcher->setTimeRatio (stretch);

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("clip tempo %1 bpm %2 ratio %3%4\n", _segment_tempo, bpm, std::setprecision (6), stretch));

		if ((avail = _stretcher->available()) < 0) {
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1/%2 stretcher->available() returned %3 - not configured!\n", index(), name(), avail));
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
#ifdef HAVE_RUBBERBAND_3_0_0
			to_pad  = _stretcher->getPreferredStartPad();
			to_drop = _stretcher->getStartDelay();
#else
			to_pad = _stretcher->getLatency();
			to_drop = to_pad;
#endif
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

	while (nframes && !_playout) {

		pframes_t to_stretcher;
		pframes_t from_stretcher;

		if (do_stretch) {

			if (read_index < last_readable_sample) {

				/* still have data to push into the stretcher */

				while ((pframes_t) avail < nframes && (read_index < last_readable_sample)) {

					to_stretcher = (pframes_t) std::min (samplecnt_t (rb_blocksize), (last_readable_sample - read_index));
					bool at_end = (to_stretcher < rb_blocksize);

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
					// cerr << "FS#2 from ees " << final_processed_sample << " - " << process_index << " & " << from_stretcher;
					from_stretcher = std::min<samplecnt_t> (from_stretcher, std::max<samplecnt_t> (0, final_processed_sample - process_index));
					// cerr << " => " << from_stretcher << endl;

					DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 total retrieved data %2 exceeds theoretical size %3, truncate from_stretcher to %4\n",
					                                              index(), retrieved, expected_end_sample - transition_samples, from_stretcher));

					if (from_stretcher == 0) {

						if (process_index < final_processed_sample) {
							DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached (EX) end, entering playout mode to cover %2 .. %3\n", index(), process_index, final_processed_sample));
							_playout = true;
						} else {
							_state = Stopped;
							_loop_cnt++;
							DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached (EX) end, now stopped, retrieved %2, avail %3 pi %4 vs fs %5 LC now %6\n", index(), retrieved, avail, process_index, final_processed_sample, _loop_cnt));
						}

						break;
					}

				}
			}

		} else {
			/* no stretch */
			assert (last_readable_sample >= read_index);
			from_stretcher = std::min<samplecnt_t> (nframes, last_readable_sample - read_index);
			// cerr << "FS#3 from lrs " << last_readable_sample <<  " - " << read_index << " = " << from_stretcher << endl;

		}

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 ready with %2 ri %3 ls %4, will write %5\n", name(), avail, read_index, last_readable_sample, from_stretcher));

		/* deliver to buffers */

		if (in_process_context) { /* constexpr, will be handled at compile time */

			for (uint32_t chn = 0; chn < bufs.count().n_audio(); ++chn) {

				uint32_t channel = chn %  data.size();
				AudioBuffer& buf (bufs.get_audio (chn));
				Sample* src = do_stretch ? bufp[channel] : (data[channel] + read_index);

				gain_t gain;

				if (_velocity_effect) {
					gain = (_velocity_effect * _velocity_gain) * _gain;
				} else {
					gain = _gain;
				}

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
				_playout = true;
			} else {
				_state = Stopped;
				_loop_cnt++;
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end, now stopped, retrieved %2, avail %3 LC now %4\n", index(), retrieved, avail, _loop_cnt));
			}
			break;
		}
	}

	pframes_t covered_frames =  orig_nframes - nframes;

	if (_playout) {

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
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 playout finished, LC now %4\n", index(), _loop_cnt));
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
	, last_event_samples (0)
	, _start_offset (0, 0, 0)
	, _legato_offset (0, 0, 0)
	, map_change (false)
{
	_channel_map.assign (16, -1);
}

MIDITrigger::~MIDITrigger ()
{
}

void
MIDITrigger::set_used_channels (Evoral::SMF::UsedChannels used)
{
	if (ui_state.used_channels != used) {

		/* increment ui_state generation so vals will get loaded when the trigger stops */
		unsigned int g = ui_state.generation.load();
		while (!ui_state.generation.compare_exchange_strong (g, g+1));

		ui_state.used_channels = used;

		send_property_change (ARDOUR::Properties::used_channels);
		_box.session().set_dirty();
	}
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
	/* this must recreate the behavior of TRIGGER_SET, but it requires special handling because its an array */
	/* specifically, we need to make sure and set the ui_state as well as the internal property, so the triggerbox won't overwrite these changes when it loads the trigger state */
	assert (pc.is_set());

	ui_state.patch_change[pc.channel()] = pc;

	/* increment ui_state generation so vals will get loaded when the trigger stops */
	unsigned int g = ui_state.generation.load();
	while (!ui_state.generation.compare_exchange_strong (g, g+1));

	send_property_change (Properties::patch_change);
}

void
MIDITrigger::unset_all_patch_changes ()
{
	/* this must recreate the behavior of TRIGGER_SET, but it requires special handling because its an array */
	/* specifically, we need to make sure and set the ui_state as well as the internal property, so the triggerbox won't overwrite these changes when it loads the trigger state */
	for (uint8_t chn = 0; chn < 16; ++chn) {
		if (ui_state.patch_change[chn].is_set ()) {
			ui_state.patch_change[chn].unset ();
		}
	}

	/* increment ui_state generation so vals will get loaded when the trigger stops */
	unsigned int g = ui_state.generation.load();
	while (!ui_state.generation.compare_exchange_strong (g, g+1));

	send_property_change (Properties::patch_change);
}

void
MIDITrigger::unset_patch_change (uint8_t channel)
{
	/* this must recreate the behavior of TRIGGER_SET_DIRECT, but it requires special handling because its an array */
	/* specifically, we need to make sure and set the ui_state as well as the internal property, so the triggerbox won't overwrite these changes when it loads the trigger state */
	assert (channel < 16);

	/* increment ui_state generation so vals will get loaded when the trigger stops */
	unsigned int g = ui_state.generation.load();
	while (!ui_state.generation.compare_exchange_strong (g, g+1));

	if (ui_state.patch_change[channel].is_set()) {
		ui_state.patch_change[channel].unset ();
	}

	send_property_change (Properties::patch_change);
}

bool
MIDITrigger::patch_change_set (uint8_t channel) const
{
	assert (channel < 16);
	return ui_state.patch_change[channel].is_set();
}

Evoral::PatchChange<MidiBuffer::TimeType> const
MIDITrigger::patch_change (uint8_t channel) const
{
	Evoral::PatchChange<MidiBuffer::TimeType> ret;

	assert (channel < 16);
	if (ui_state.patch_change[channel].is_set()) {
		ret = ui_state.patch_change[channel];
	}

	return ret;
}


bool
MIDITrigger::probably_oneshot () const
{
	/* XXX fix for short chord stabs */
	return false;
}

void
MIDITrigger::start_and_roll_to (samplepos_t start_pos, samplepos_t end_position, uint32_t cnt)
{
	Trigger::start_and_roll_to (start_pos, end_position, *this, &MIDITrigger::midi_run<false>, cnt);
}

timepos_t
MIDITrigger::compute_end (Temporal::TempoMap::SharedPtr const & tmap, Temporal::BBT_Time const & transition_bbt, samplepos_t, Temporal::Beats & effective_length)
{
	const Temporal::BBT_Argument transition_bba (superclock_t (0), transition_bbt);

	Temporal::Beats end_by_follow_length = tmap->quarters_at (tmap->bbt_walk (transition_bba, _follow_length));
	Temporal::Beats end_by_data_length = tmap->quarters_at (tmap->bbt_walk (transition_bba, Temporal::BBT_Offset (0, data_length.get_beats(), data_length.get_ticks())));

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 ends: TB %2 FL %3 EBFL %4 DL %5 EBDL %6 tbbt %7 fl %8\n",
	                                              index(), transition_beats, _follow_length, end_by_follow_length, data_length, end_by_data_length, transition_bbt, _follow_length));
	Temporal::BBT_Offset q (_quantization);

	if (launch_style() != Repeat || (q == Temporal::BBT_Offset())) {

		if (internal_use_follow_length()) {
			final_beat = end_by_follow_length;
			effective_length = tmap->bbtwalk_to_quarters (transition_bba, _follow_length);
		} else {
			final_beat = end_by_data_length;
			effective_length = tmap->bbtwalk_to_quarters (transition_bba, Temporal::BBT_Offset (0, data_length.get_beats(), data_length.get_ticks()));
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
	std::shared_ptr<MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion> (_region);
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
		if (_used_channels.test(chn) && allow_patch_changes() && _patch_change[chn].is_set()) {
			_patch_change[chn].set_time (dest_offset);
			DEBUG_TRACE (DEBUG::MidiTriggers, string_compose ("Injecting patch change c:%1 b:%2 p:%3\n", (uint32_t) _patch_change[chn].channel(), (uint32_t) _patch_change[chn].bank(), (uint32_t) _patch_change[chn].program()));
			for (int msg = 0; msg < _patch_change[chn].messages(); ++msg) {
				if (mb) {
					mb->insert_event (_patch_change[chn].message (msg));
					_box.tracker->track (_patch_change[chn].message (msg).buffer());
				}
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
MIDITrigger::get_state () const
{
	XMLNode& node (Trigger::get_state());

	node.set_property (X_("start"), start_offset());

	std::string uchan = string_compose ("%1", _used_channels.to_ulong());
	node.set_property (X_("used-channels"), uchan);

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

	std::string uchan;
	if (node.get_property (X_("used-channels"), uchan)) {
	} else {
		unsigned long ul;
		std::stringstream ss (uchan);
		ss >> ul;
		if (!ss) {
			return -1;
		}
		set_used_channels( Evoral::SMF::UsedChannels(ul) );
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

	/* we've changed our internal values; we need to update our queued UIState or they will be lost when UIState is applied */
	copy_to_ui_state ();

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

void
MIDITrigger::estimate_midi_patches ()
{
	/* first, initialize all our slot's patches to GM defaults, to make playback deterministic */
	for (uint8_t chan = 0; chan < 16; ++chan) {
		_patch_change[chan].set_channel(chan);
		_patch_change[chan].set_bank( chan == 9 ? 120 : 0 );
		_patch_change[chan].set_program( 0 );
	}

	std::shared_ptr<SMFSource> smfs = std::dynamic_pointer_cast<SMFSource> (_region->source(0));
	if (smfs) {
		/* second, apply any patches that the Auditioner has in its memory
		 * ...this handles the case where the user chose patches for a file that itself lacked patch-settings
		 * (it's possible that the user didn't audition the actual file they dragged in, but this is still the best starting-point we have)
		 * */
		std::shared_ptr<ARDOUR::Auditioner> aud = _box.session().the_auditioner();
		if (aud) {
			for (uint8_t chan = 0; chan < 16; ++chan) {
				if (aud->patch_change (chan).is_set()) {
					_patch_change[chan] = aud->patch_change (chan);
				}
			}
		}

		/* thirdly, apply the patches from the file itself (if it has any) */
		std::shared_ptr<MidiModel> model = smfs->model();
		for (MidiModel::PatchChanges::const_iterator i = model->patch_changes().begin(); i != model->patch_changes().end(); ++i) {
			if ((*i)->is_set()) {
				int chan = (*i)->channel();  /* behavior is undefined for SMF's with multiple patch changes. I'm not sure that we care */
				_patch_change[chan].set_channel ((*i)->channel());
				_patch_change[chan].set_bank((*i)->bank());
				_patch_change[chan].set_program((*i)->program());
			}
		}

		/* finally, store the used_channels so the UI can show patches only for those chans actually used */
		DEBUG_TRACE (DEBUG::MidiTriggers, string_compose ("%1 estimate_midi_patches(), using channels %2\n", name(), smfs->used_channels().to_string().c_str()));
		_used_channels = smfs->used_channels();
	}

	//we've changed some of our internal values; the calling code must call copy_to_ui_state ... ::set_region_in_worker_thread  does it

}

int
MIDITrigger::set_region_in_worker_thread (std::shared_ptr<Region> r)
{
	std::shared_ptr<MidiRegion> mr = std::dynamic_pointer_cast<MidiRegion> (r);

	if (!mr) {
		return -1;
	}

	set_region_internal (r);
	set_name (mr->name());
	data_length = mr->length().beats();
	_follow_length = Temporal::BBT_Offset (0, data_length.get_beats(), 0);
	set_length (mr->length());
	model = mr->model ();

	estimate_midi_patches ();

	/* we've changed some of our internal values; we need to update our queued UIState or they will be lost when UIState is applied */
	copy_to_ui_state ();

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
	last_event_samples = 0;
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 retriggered to %2, ts = %3\n", _index, iter->time(), transition_beats));
}

void
MIDITrigger::reload (BufferSet&, void*)
{
}

void
MIDITrigger::tempo_map_changed ()
{
	/* called from process context, but before Session::process(), and only
	 * on an active trigger.
	 */

	iter = model->begin();
	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());
	const timepos_t region_start_time = _region->start();
	const Temporal::Beats region_start = region_start_time.beats();

	while (iter != model->end()) {

		/* Find the first event whose sample time is equal-to or
		 * greater than the last played event sample. That is the
		 * event we wish to use next, after the tempo map change.
		 *
		 * Note that the sample time is being computed with the *new*
		 * tempo map, while last_event_samples we computed with the old
		 * one.
		 */

		const Temporal::Beats iter_timeline_beats = transition_beats + ((*iter).time() - region_start);
		samplepos_t iter_timeline_samples = tmap->sample_at (iter_timeline_beats);

		if (iter_timeline_samples >= last_event_samples) {
			break;
		}

		++iter;
	}

	if (iter != model->end()) {
		Temporal::Beats elen_ignored;
		(void) compute_end (tmap, _transition_bbt, transition_samples, elen_ignored);
	}

	map_change = true;
}

template<bool in_process_context>
pframes_t
MIDITrigger::midi_run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample,
                       Temporal::Beats const & start_beats, Temporal::Beats const & end_beats,
                       pframes_t nframes, pframes_t dest_offset, double bpm, pframes_t& quantize_offset)
{
	MidiBuffer* mb (in_process_context? &bufs.get_midi (0) : 0);
	typedef Evoral::Event<MidiModel::TimeType> MidiEvent;
	const timepos_t region_start_time = _region->start();
	const Temporal::Beats region_start = region_start_time.beats();
	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());

	last_event_samples = end_sample;

	/* see if we're going to start or stop or retrigger in this run() call */
	quantize_offset = 0;
	maybe_compute_next_transition (start_sample, start_beats, end_beats, nframes, quantize_offset);
	const pframes_t orig_nframes = nframes;

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 after checking for transition, state = %2\n", name(), enum_2_string (_state)));

	switch (_state) {
	case Stopped:
	case WaitingForRetrigger:
	case WaitingToStart:
		return nframes;
	case Running:
	case WaitingToStop:
	case WaitingToSwitch:
	case Stopping:
		break;
	}

	Temporal::Beats last_event_timeline_beats = final_beat; /* will indicate "done" if there is nothing to do */

	while (iter != model->end() && !_playout) {

		MidiEvent const & event (*iter);

		/* Event times are in beats, relative to start of source
		 * file. We need to convert to region-relative time, and then
		 * a session timeline time, which is defined by the time at
		 * which we last transitioned (in this case, to being active)
		 */

		Temporal::Beats maybe_last_event_timeline_beats = transition_beats + (event.time() - region_start);


		/* check that the event is within the bounds for this run() call */

		if (maybe_last_event_timeline_beats < start_beats) {
			break;
		}

		if (maybe_last_event_timeline_beats > final_beat) {
			iter = model->end();
			break;
		}

		if (maybe_last_event_timeline_beats >= end_beats) {
			break;
		}

		/* Now get the sample position of the event, on the timeline */

		const samplepos_t timeline_samples = tmap->sample_at (maybe_last_event_timeline_beats);

		if (in_process_context) { /* compile-time const expr */

			/* Now we have to convert to a position within the buffer we
			 * are writing to.
			 */
			samplepos_t buffer_samples;

			/* HACK time: in the argument list, we have

			   start_sample, end_sample: computed from Session::process()
			   adjusted by latency

			   start_beats, end_beats: computed from the above two
			   sample values, using the tempo map.

			   When we compute the buffer/sample offset for event, we are
			   converting from beats to samples, the opposite direction of
			   the computation of start/end_beats from
			   start/end_sample.

			   These conversions are not reversible (the precision
			   of audio time exceeds that of music time). As a
			   result, we may end up in a situation where the beat
			   position of the event confirms that it is to be
			   delivered within this ::run() call, but the sample
			   value says that it was to be delivered in the
			   previous call. As an example, given some tempo map
			   parameters, start_sample 6160 converts to 0:536, but
			   event time 0:536 converts to 6156 (earlier by 4
			   samples).

			   We consider the beat position to be "more canonical"
			   than the sample position, and so if this happens,
			   treat the event as occuring at start_sample, not
			   before it.

			   Note that before this test, we've already
			   established that the event time in beats is within range.
			*/


			if (timeline_samples < start_sample) {
				buffer_samples = dest_offset;
			} else {

				/* (timeline_samples - start_sample) gives us the
				 * sample offset from the start of our run() call. But
				 * since we may be executing after another trigger in
				 * the same process() cycle, we must take dest_offset
				 * into account to get an actual buffer position.
				 */

				buffer_samples = (timeline_samples - start_sample) + dest_offset;
			}

			assert (buffer_samples >= 0);

			Evoral::Event<MidiBuffer::TimeType> ev (Evoral::MIDI_EVENT, buffer_samples, event.size(), const_cast<uint8_t*>(event.buffer()), false);

			if (_gain != 1.0f && ev.is_note()) {
				ev.scale_velocity (_gain);
			}

			int chn = ev.channel();

			if (_channel_map[ev.channel()] > 0) {
				ev.set_channel (_channel_map[chn]);
			}

			if (ev.is_pgm_change() || (ev.is_cc() && ((ev.cc_number() == MIDI_CTL_LSB_BANK) || (ev.cc_number() == MIDI_CTL_MSB_BANK)))) {
				if (!allow_patch_changes ()) {
					/* do not send ANY patch or bank messages, just skip them */
					DEBUG_TRACE (DEBUG::MidiTriggers, string_compose ("Ignoring patch change on chn:%1\n", (uint32_t) _patch_change[chn].channel()));
					++iter;
					continue;
				} else if ( _patch_change[chn].is_set() ) {
					/* from this context we don't know if a pgm message in the midi buffer is from the file or from triggerbox */
					/* so when a bank or pgm message is recognized, just replace it with the desired patch */
					DEBUG_TRACE (DEBUG::MidiTriggers, string_compose ("Replacing patch change c:%1 b:%2 p:%3\n", (uint32_t) _patch_change[chn].channel(), (uint32_t) _patch_change[chn].bank(), (uint32_t) _patch_change[chn].program()));
					if (ev.is_cc() && (ev.cc_number() == MIDI_CTL_MSB_BANK)) {
						ev.set_cc_value(_patch_change[chn].bank_msb());
					} else if (ev.is_cc() && (ev.cc_number() == MIDI_CTL_LSB_BANK)) {
						ev.set_cc_value(_patch_change[chn].bank_lsb());
					} else if (ev.is_pgm_change()) {
						ev.set_pgm_number(_patch_change[chn].program());
					}
				}
			}

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("given et %1 TS %7 rs %8 ts %2 bs %3 ss %4 do %5, inserting %6\n", maybe_last_event_timeline_beats, timeline_samples, buffer_samples, start_sample, dest_offset, ev, transition_beats, region_start));
			mb->insert_event (ev);
		}

		_box.tracker->track (event.buffer());

		last_event_beats = event.time();
		last_event_timeline_beats = maybe_last_event_timeline_beats;

		++iter;
	}


	if (in_process_context && _state == Stopping) { /* first clause is a compile-time constexpr */
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 was stopping, now stopped, resolving notes @ %2\n", index(), nframes-1));
		_box.tracker->resolve_notes (*mb, nframes-1);
	}

	if (iter == model->end()) {

		/* We reached the end */

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached end, leb %2 les %3 fb %4 dl %5\n", index(), last_event_timeline_beats, last_event_samples, final_beat, data_length));

		/* "final_beat" is an inclusive end of the trigger, not
		 * exclusive, so we must use <= here. That is, any last event
		 * (remember, iter == model->end() here, so we have already read
		 * through the entire MIDI model) that is up to AND INCLUDING
		 * final_beat counts as "haven't reached the end".
		 */

		if (last_event_timeline_beats <= final_beat) {

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 entering playout because ... leb %2 <= fb %3\n", index(), last_event_timeline_beats, final_beat));

			_playout = true;

			if (final_beat > end_beats) {
				/* no more events to come before final_beat,
				 * and that is beyond the end of this ::run()
				 * call. Not finished with playout yet, but
				 * all frames covered.
				 */
				nframes = 0;
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 not done with playout, all frames covered\n", index()));
			} else {
				/* finishing up playout */
				samplepos_t final_processed_sample = tmap->sample_at (timepos_t (final_beat));

				if (map_change) {
					if ((start_sample > final_processed_sample) || (final_processed_sample - start_sample > orig_nframes)) {
						nframes = 0;
						_loop_cnt++;
						_state = Stopping;
					} else {
						nframes = orig_nframes - (final_processed_sample - start_sample);
					}
				} else {
					nframes = orig_nframes - (final_processed_sample - start_sample);
					_loop_cnt++;
					_state = Stopped;
				}
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 playout done, nf = %2 fb %3 fs %4 %5 LC %6\n", index(), nframes, final_beat, final_processed_sample, start_sample, _loop_cnt));
			}

		} else {

			const samplepos_t final_processed_sample = tmap->sample_at (timepos_t (final_beat));
			const samplecnt_t nproc = (final_processed_sample - start_sample);

			if (nproc > orig_nframes) {
				/* tempo map changed, probably */
				nframes = nproc > orig_nframes ? 0 : orig_nframes - nproc;
			} else {
				nframes = orig_nframes - nproc;
			}
			_loop_cnt++;
			_state = Stopped;
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 reached final event, now stopped, nf = %2 fb %3 fs %4 %5 LC %6\n", index(), nframes, final_beat, final_processed_sample, start_sample, _loop_cnt));
		}

	} else {
		/* we didn't reach the end of the MIDI data, ergo we covered
		   the entire timespan passed into us.
		*/
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 did not reach end, nframes left at %2, next event is %3\n", index(), nframes, *iter));
		nframes = 0;
	}

	/* tempo map changes could lead to nframes > orig_nframes */

	const samplecnt_t covered_frames = nframes > orig_nframes ? orig_nframes : orig_nframes - nframes;

	if (_state == Stopped || _state == Stopping) {
		when_stopped_during_run (bufs, (dest_offset + covered_frames) ? (dest_offset + covered_frames - 1) : 0);
	}

	process_index += covered_frames;

	map_change = false;

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
	Properties::allow_patch_changes.property_id = g_quark_from_static_string (X_("allow_patch_changes"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for allow_patch_changes = %1\n", Properties::allow_patch_changes.property_id));
	Properties::stretch_mode.property_id = g_quark_from_static_string (X_("stretch_mode"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for stretch_mode = %1\n", Properties::stretch_mode.property_id));
	Properties::patch_change.property_id = g_quark_from_static_string (X_("patch_change"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for patch_change = %1\n", Properties::patch_change.property_id));
	Properties::channel_map.property_id = g_quark_from_static_string (X_("channel_map"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for channel_map = %1\n", Properties::channel_map.property_id));
	Properties::currently_playing.property_id = g_quark_from_static_string (X_("currently_playing"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for currently_playing = %1\n", Properties::currently_playing.property_id));
	Properties::queued.property_id = g_quark_from_static_string (X_("queued"));
	DEBUG_TRACE (DEBUG::Properties, string_compose ("quark for queued = %1\n", Properties::queued.property_id));
}

Temporal::BBT_Offset TriggerBox::_assumed_trigger_duration (4, 0, 0);
TriggerBox::TriggerMidiMapMode TriggerBox::_midi_map_mode (TriggerBox::Custom);
int TriggerBox::_first_midi_note = 60;
std::atomic<int> TriggerBox::active_trigger_boxes (0);
TriggerBoxThread* TriggerBox::worker = 0;
CueRecords TriggerBox::cue_records (256);
std::atomic<bool> TriggerBox::_cue_recording (false);
PBD::Signal0<void> TriggerBox::CueRecordingChanged;
bool TriggerBox::roll_requested = false;
bool TriggerBox::_learning = false;
TriggerBox::CustomMidiMap TriggerBox::_custom_midi_map;
std::pair<int,int> TriggerBox::learning_for;
PBD::Signal0<void> TriggerBox::TriggerMIDILearned;

std::shared_ptr<MIDI::Parser> TriggerBox::input_parser;
PBD::ScopedConnectionList TriggerBox::static_connections;
PBD::ScopedConnection TriggerBox::midi_input_connection;
std::shared_ptr<MidiPort> TriggerBox::current_input;
PBD::Signal2<void,PBD::PropertyChange,int> TriggerBox::TriggerBoxPropertyChange;

typedef std::map <std::shared_ptr<Region>, std::shared_ptr<Trigger::UIState>> RegionStateMap;
RegionStateMap enqueued_state_map;


void
TriggerBox::init ()
{
	worker = new TriggerBoxThread;
	TriggerBoxThread::init_request_pool ();
	init_pool ();
}

void
TriggerBox::static_init (Session & s)
{
	input_parser = std::shared_ptr<MIDI::Parser>(new MIDI::Parser); /* leak */
	Config->ParameterChanged.connect_same_thread (static_connections, boost::bind (&TriggerBox::static_parameter_changed, _1));
	input_parser->any.connect_same_thread (midi_input_connection, boost::bind (&TriggerBox::midi_input_handler, _1, _2, _3, _4));
	std::dynamic_pointer_cast<MidiPort> (s.trigger_input_port())->set_trace (input_parser);
	std::string const& dtip (Config->get_default_trigger_input_port());
	if (!dtip.empty () && s.engine().get_port_by_name (dtip)) {
		s.trigger_input_port()->connect (dtip);
	}
}

void
TriggerBox::send_property_change (PBD::PropertyChange pc)
{
	PropertyChanged (pc);
	TriggerBoxPropertyChange (pc, _order);
}

TriggerBox::TriggerBox (Session& s, DataType dt)
	: Processor (s, _("TriggerBox"), Temporal::TimeDomainProvider (Temporal::BeatTime))
	, tracker (dt == DataType::MIDI ? new MidiStateTracker : 0)
	, _data_type (dt)
	, _order (-1)
	, explicit_queue (64)
	, _currently_playing (0)
	, _stop_all (false)
	, _active_scene (-1)
	, _active_slots (0)
	, _locate_armed (false)
	, _cancel_locate_armed (false)
	, _fast_forwarding (false)
	, requests (1024)
{
	set_display_to_user (false);

	/* default number of possible triggers. call ::add_trigger() to increase */

	if (_data_type == DataType::AUDIO) {
		for (uint32_t n = 0; n < TriggerBox::default_triggers_per_box; ++n) {
			all_triggers.push_back (std::make_shared<AudioTrigger> (n, *this));
		}
	} else {
		for (uint32_t n = 0; n < TriggerBox::default_triggers_per_box; ++n) {
			all_triggers.push_back (std::make_shared<MIDITrigger> (n, *this));
		}
	}

	while (pending.size() < all_triggers.size()) {
		pending.push_back (std::atomic<Trigger*>(0));
	}

	Config->ParameterChanged.connect_same_thread (*this, boost::bind (&TriggerBox::parameter_changed, this, _1));
	_session.config.ParameterChanged.connect_same_thread (*this, boost::bind (&TriggerBox::parameter_changed, this, _1));
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
TriggerBox::input_port_check ()
{
	if (Config->get_default_trigger_input_port().empty()) {
		return;
	}

	Session* session = AudioEngine::instance()->session();

	if (!session) {
		return;
	}

	std::cerr << "Reconnect to "  << Config->get_default_trigger_input_port() << std::endl;
	session->trigger_input_port()->connect (Config->get_default_trigger_input_port());
}

void
TriggerBox::static_parameter_changed (std::string const & param)
{
	if (param == X_("default-trigger-input-port")) {
		input_port_check ();
	}
}

void
TriggerBox::parameter_changed (std::string const & param)
{
	if (param == "cue-behavior") {
		const bool follow = (_session.config.get_cue_behavior() & FollowCues);
		if (!follow) {
			cancel_locate_armed ();
		}
	}
}

void
TriggerBox::cancel_locate_armed ()
{
	_cancel_locate_armed = true;
}

void
TriggerBox::fast_forward_nothing_to_do ()
{
	cancel_locate_armed ();
	if (tracker) {
		tracker->reset ();
	}
}

void
TriggerBox::fast_forward (CueEvents const & cues, samplepos_t transport_position)
{
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1: ffwd to %2\n", order(), transport_position));

	if (!(_session.config.get_cue_behavior() & FollowCues)) {
		/* do absolutely nothing */
		return;
	}

	if (cues.empty() || (cues.front().time > transport_position)) {
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1: nothing to be done, cp = %2\n", order(), _currently_playing));
		fast_forward_nothing_to_do ();
		return;
	}

	CueEvents::const_reverse_iterator c = cues.rbegin ();
	samplepos_t pos = c->time;
	TriggerPtr trig;
	Temporal::BBT_Argument start_bbt;
	samplepos_t start_samples;
	Temporal::Beats effective_length;
	bool will_start;
	uint32_t cnt = 0;

	using namespace Temporal;
	TempoMap::SharedPtr tmap (TempoMap::use());

	PBD::Unwinder<bool> uw (_fast_forwarding, true);

	/* Walk backwards through cues to find the first one that is either a
	 * stop-all cue or references a non-cue-isolated trigger, and is
	 * positioned before transport_position
	 */

	while (c != cues.rend()) {

		if (c->time <= transport_position) {

			if (c->cue == CueRecord::stop_all) {
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("Found stop-all cues at %1\n", c->time));
				break;
			}

			if (!all_triggers[c->cue]->cue_isolated()) {
				DEBUG_TRACE (DEBUG::Triggers, string_compose ("Found first non-CI cue for %1 at %2\n", c->cue, c->time));
				break;
			}
		}

		++c;
	}

	/* if we reach the rend (beginning), or the cue is a stop-all, or the
	 * cue is precisely at the transport position, there is nothing to do
	 */

	if (c == cues.rend() || (c->cue == CueRecord::stop_all) | (c->time == transport_position)) {
		fast_forward_nothing_to_do ();
		return;
	}

	trig = all_triggers[c->cue];
	pos = c->time;
	cnt = 0;

	if (!trig->region()) {
		fast_forward_nothing_to_do ();
		return;
	}

	while (pos < transport_position) {

		if (cnt >= trig->follow_count()) {

			/* trigger has completed follow-count
			 * iterations, and it is time to decide what to
			 * do next.
			 */

			int dnt = determine_next_trigger (trig->index());
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 selected next as %2\n",  order(), dnt));
			if (dnt >= 0) {
				/* new trigger, reset the counter used
				 * to track iterations run.
				 */
				trig = all_triggers[dnt];
				cnt = 0;
			} else {
				/* for whatever reason, there's no
				   subsequent trigger to follow this
				   one.
				*/
				fast_forward_nothing_to_do ();
				return;
			}

		} else {
			/* this trigger has not reached its follow count yet: just let it play again */
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("have not reached follow count yet, play %1 again\n", trig->index()));
		}

		/* determine when it starts */

		will_start = true;

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1/%2 compite start give pos %3 transport position %4 ss %5 q %6\n", order(), trig->index(), pos, transport_position, start_samples, trig->quantization()));

		/* we don't care when it actually starts, so we give a "far
		 * off" time to ::compute_start(). It is entirely possible that
		 * due to quantization, the trigger will not actually start
		 * before the transport position, but that doesn't mean it
		 * should not be considered as "started" and merely in the
		 * WaitingToStart state.
		 */

		const samplepos_t far_off = transport_position + (10.0 * _session.sample_rate());
		start_bbt = trig->compute_start (tmap, pos, far_off, trig->quantization(), start_samples, will_start);

		if (!will_start) {
			/* nothing to do. This suggests something very weird
			 * about the trigger, but we don't address that here
			 */
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 will not start even far from %2\n", trig->index(), transport_position));
			return;
		}

		/* we now consider this trigger to be running. Let's see when
		 * it ends...
		 */

		samplepos_t trig_ends_at = trig->compute_end (tmap, start_bbt, start_samples, effective_length).samples();

		if (trig_ends_at >= transport_position) {
			/* we're done. trig now references the trigger that
			   would have started most recently before the
			   transport position (which could be null).
			*/

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("trigger %1 ends after %2\n", trig->index(), transport_position));
			break;
		} else {
			DEBUG_TRACE (DEBUG::Triggers,  "trigger ends before transport pos\n");
			cnt++;
		}

		pos = trig_ends_at;
	}

	if (pos >= transport_position || !trig) {
		/* nothing to do */
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1: no trigger to be rolled (%2 >= %3, trigger = %4)\n", order(), pos, transport_position, trig));
		_currently_playing = 0;
		fast_forward_nothing_to_do ();
		return;
	}

	/* trig is the trigger that would start or still be running at
	 * transport_position. We need to run it in a special mode that ensures
	 * that
	 *
	 * 1) for MIDI, we know the state at transport position
	 * 2) for audio, the stretcher is in the correct state
	 */

	/* find the closest start (retrigger) position for this trigger */

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("trig %1 should be rolling at %2 ss = %3\n", trig->index(), transport_position, start_samples));

	if (start_samples < transport_position) {
		samplepos_t s = start_samples;
		BBT_Argument ns = start_bbt;
		const BBT_Offset step (0, effective_length.get_beats(), effective_length.get_ticks());

		do {
			start_samples = s;
			ns = tmap->bbt_walk (ns, step);
			s = tmap->sample_at (ns);
		} while (s < transport_position);

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1: roll trigger %2 from %3 to %4 with cnt = %5\n", order(), trig->index(), start_samples, transport_position, cnt));

		if (std::dynamic_pointer_cast<MIDITrigger> (trig)) {
			std::dynamic_pointer_cast<MIDITrigger> (trig)->_transition_bbt = ns;
		}

		trig->start_and_roll_to (start_samples, transport_position, cnt);

		_currently_playing = trig;
		_locate_armed = true;
		_cancel_locate_armed = false;
		/* currently playing is now ready to keep running at transport position
		 *
		 * Note that a MIDITrigger will have set a flag so that when we call
		 * ::run() again, it will dump its current MIDI state before anything
		 * else.
		 */
	} else {
		DEBUG_TRACE (DEBUG::Triggers, string_compose ("trig %1 will start after transport position, so just start it up for now\n", trig->index()));
		BufferSet bufs;
		trig->startup_from_ffwd (bufs, cnt);
		_currently_playing = trig;
		_locate_armed = true;
		_cancel_locate_armed = false;
	}

	_stop_all = false;

	return;
}

void
TriggerBox::set_region (uint32_t slot, std::shared_ptr<Region> region)
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

		t->set_follow_count(all_triggers[slot]->follow_count());
		t->set_follow_length(all_triggers[slot]->follow_length());
		t->set_use_follow_length(all_triggers[slot]->use_follow_length());
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

	send_property_change (ARDOUR::Properties::queued);

	DEBUG_TRACE (DEBUG::Triggers, string_compose ("explicit queue %1, EQ = %2\n", n, explicit_queue.read_space()));

	if (_currently_playing) {
		_currently_playing->begin_stop (false);  /* @paul is this necessary/desired?  the current clip should stop (only) when the new one starts */
	}
}

TriggerPtr
TriggerBox::peek_next_trigger ()
{
	RingBuffer<uint32_t>::rw_vector rwv;
	explicit_queue.get_read_vector (&rwv);

	if (rwv.len[0] > 0) {

		/* peek at it without dequeing it */

		uint32_t n = *(rwv.buf[0]);
		return trigger (n);
	}

	return TriggerPtr();
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
	return TriggerPtr();
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
TriggerBox::deep_sources (std::set<std::shared_ptr<Source> >& sources)
{
	Glib::Threads::RWLock::ReaderLock lm (trigger_lock);

	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		std::shared_ptr<Region> r (trigger(n)->region ());
		if (r) {
			r->deep_sources (sources);
		}
	}
}

void
TriggerBox::used_regions (std::set<std::shared_ptr<Region> >& regions)
{
	Glib::Threads::RWLock::ReaderLock lm (trigger_lock);

	for (uint64_t n = 0; n < all_triggers.size(); ++n) {
		std::shared_ptr<Region> r (trigger(n)->region ());
		if (r) {
			regions.insert (r);
		}
	}
}


void
TriggerBox::enqueue_trigger_state_for_region (std::shared_ptr<Region> region, std::shared_ptr<Trigger::UIState> state)
{
	enqueued_state_map.insert (std::make_pair(region, state));
}

void
TriggerBox::set_from_selection (uint32_t slot, std::shared_ptr<Region> region)
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
		status.import_markers = false;
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

		std::shared_ptr<Region> the_region (RegionFactory::create (src_list, plist, true));

		all_triggers[slot]->set_region (the_region);

	} catch (std::exception& e) {
		error << string_compose ("loading sample from %1 failed (%2)\n", path, e.what()) << endmsg;
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
		all_triggers[n]->set_region (std::shared_ptr<Region>());
	}
}

void
TriggerBox::clear_cue (int cue)
{
	all_triggers[cue]->set_region (std::shared_ptr<Region>());
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
		all_triggers[n]->stop_quantized ();
	}
}

void
TriggerBox::bang_trigger_at (Triggers::size_type row, float velocity)
{
	TriggerPtr t = trigger(row);
	if (t && t->region()) {
		t->bang (velocity);
	} else {
		/* by convention, an empty slot is effectively a STOP button */
		stop_all_quantized();
	}
}

void
TriggerBox::unbang_trigger_at (Triggers::size_type row)
{
	TriggerPtr t = trigger(row);
	if (t && t->region()) {
		t->unbang();
	} else {
		/* you shouldn't be able to unbang an empty slot; but if this somehow happens we'll just treat it as a */
		stop_all_quantized();
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

bool
TriggerBox::lookup_custom_midi_binding (std::vector<uint8_t> const & msg, int& x, int& y)
{
	CustomMidiMap::iterator i = _custom_midi_map.find (msg);

	if (i == _custom_midi_map.end()) {
		return false;
	}

	x = i->second.first;
	y = i->second.second;

	return true;
}

void
TriggerBox::midi_input_handler (MIDI::Parser&, MIDI::byte* buf, size_t sz, samplecnt_t)
{
	if (_learning) {

		if ((buf[0] & 0xf0) == MIDI::on) {
			/* throw away velocity */
			std::vector<uint8_t> msg { buf[0], buf[1] };
			add_custom_midi_binding (msg, learning_for.first, learning_for.second);
			_learning = false;
			TriggerMIDILearned (); /* EMIT SIGNAL */
		}

		return;
	}

	Evoral::Event<samplepos_t> ev (Evoral::MIDI_EVENT, 0, sz, buf);

	if (ev.is_note_on()) {

		std::vector<uint8_t> msg { uint8_t (MIDI::on | ev.channel()), (uint8_t) ev.note() };
		int x;
		int y;

		if (lookup_custom_midi_binding (msg, x, y)) {
			AudioEngine::instance()->session()->bang_trigger_at (x, y, ev.velocity());
		}
	}

	return;
}

void
TriggerBox::begin_midi_learn (int index)
{
	learning_for.first = order(); /* x */
	learning_for.second = index;  /* y */
	_learning = true;
}

void
TriggerBox::stop_midi_learn ()
{
	_learning = false;
}

void
TriggerBox::midi_unlearn (int index)
{
	remove_custom_midi_binding (order(), index);
}

void
TriggerBox::clear_custom_midi_bindings ()
{
	_custom_midi_map.clear ();
}

int
TriggerBox::save_custom_midi_bindings (std::string const & path)
{
	XMLTree tree;

	tree.set_root (get_custom_midi_binding_state());

	if (!tree.write (path)) {
		return -1;
	}

	return 0;
}

XMLNode*
TriggerBox::get_custom_midi_binding_state ()
{
	XMLTree tree;
	XMLNode* root = new XMLNode (X_("TriggerBindings"));

	for (auto const & b : _custom_midi_map) {

		XMLNode* n = new XMLNode (X_("Binding"));
		n->set_property (X_("col"), b.second.first);
		n->set_property (X_("row"), b.second.second);

		std::stringstream str;

		for (auto const & v : b.first) {
			str << std::hex << "0x" << (int) v << ' ';
		}

		n->set_property (X_("msg"), str.str());

		root->add_child_nocopy (*n);
	}

	return root;
}

int
TriggerBox::load_custom_midi_bindings (XMLNode const & root)
{
	if (root.name() != X_("TriggerBindings")) {
		return -1;
	}

	_custom_midi_map.clear ();

	for (auto const & n : root.children()) {
		int x;
		int y;

		if (n->name() != X_("Binding")) {
			continue;
		}

		if (!n->get_property (X_("col"), x)) {
			continue;
		}

		if (!n->get_property (X_("row"), y)) {
			continue;
		}

		std::string str;

		if (!n->get_property (X_("msg"), str)) {
			continue;
		}

		std::istringstream istr (str);
		std::vector<uint8_t> msg;

		do {
			int x;
			istr >> std::setbase (16) >> x;
			if (!istr) {
				break;
			}
			msg.push_back (uint8_t (x));

		} while (true);

		add_custom_midi_binding (msg, x, y);
	}

	return 0;
}

void
TriggerBox::add_custom_midi_binding (std::vector<uint8_t> const & msg, int x, int y)
{
	std::pair<CustomMidiMap::iterator,bool> res = _custom_midi_map.insert (std::make_pair (msg, std::make_pair (x, y)));

	if (!res.second) {
		_custom_midi_map[msg] = std::make_pair (x, y);
	}
}

void
TriggerBox::remove_custom_midi_binding (int x, int y)
{
	/* this searches the whole map in case there are multiple entries
	 *(keyed by note/channel) for the same pad (x,y)
	 */

	for (CustomMidiMap::iterator i = _custom_midi_map.begin(); i != _custom_midi_map.end(); ++i) {
		if (i->second.first == x && i->second.second == y) {
			_custom_midi_map.erase (i);
			break;
		}
	}
}

void
TriggerBox::maybe_request_roll (Session& s)
{
	if (!roll_requested) {
		s.request_roll ();
	}
}

void
TriggerBox::begin_process_cycle ()
{
	roll_requested = false;
}


int
TriggerBox::handle_stopped_trigger (BufferSet& bufs, pframes_t dest_offset)
{
	if (_currently_playing->will_follow()) {
		int n = determine_next_trigger (_currently_playing->index());
		Temporal::BBT_Offset start_quantization;
		if (n < 0) {
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 finished, no next trigger\n", _currently_playing->name()));
			_currently_playing = 0;
			send_property_change (Properties::currently_playing);
			return 1; /* no triggers to come next, break out of nframes loop */
		}
		if ((int) _currently_playing->index() == n) {
			start_quantization = Temporal::BBT_Offset ();
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("switching to next trigger %1, will use start immediately \n", all_triggers[n]->name()));
		} else {
			DEBUG_TRACE (DEBUG::Triggers, string_compose ("switching to next trigger %1\n", all_triggers[n]->name()));
		}
		_currently_playing = all_triggers[n];
		_currently_playing->startup (bufs, dest_offset, start_quantization);
		send_property_change (Properties::currently_playing);
	} else {
		_currently_playing = 0;
		send_property_change (Properties::currently_playing);
		DEBUG_TRACE (DEBUG::Triggers, "currently playing was stopped, but stop_all was set #1, leaving nf loop\n");
		/* leave nframes loop */
		return 1;
	}

	return 0;
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

	if (_session.transport_locating()) {
		/* nothing to do here at all. We do not run triggers while
		   locate is still taking place.
		*/
		return;
	}

	if (speed < 0.) {
		/* do absolutely nothing when in reverse playback */
		return;
	}

	/* STEP TWO: if latency compensation tells us that we haven't really
	 * started yet, do nothing, because we can't make sense of a negative
	 * start sample time w.r.t the tempo map.
	 */

	if (start_sample < 0) {
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
	bool    was_recorded;
	int32_t cue_bang = _session.first_cue_within (start_sample, end_sample, was_recorded);

	if (!_cue_recording || !was_recorded) {

		if (cue_bang == CueRecord::stop_all) {

			DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 sees STOP ALL!\n", order()));

			/* reached a "stop all cue-launched cues from playing"
			 * marker.The stop is quantized, not immediate.
			 */

			if (_currently_playing) {
				_currently_playing->stop_quantized ();
			}

			_locate_armed = false;

		} else if (cue_bang >= 0) {
			_active_scene = cue_bang;
			_locate_armed = false;
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
			send_property_change (Properties::currently_playing);
			active_trigger_boxes.fetch_add (1);
		}
	}

	/* STEP NINE: if we've been told to stop all slots, do so
	 */

	if (allstop) {
		stop_all ();
	}

	if (_locate_armed && _cancel_locate_armed) {
		if (_currently_playing) {
			_currently_playing->shutdown (bufs, 0);
			_currently_playing = 0;
			send_property_change (Properties::currently_playing);
		}

		_cancel_locate_armed = false;

	} else if (!_locate_armed) {

		_cancel_locate_armed = false;
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
		if (speed == 0.0 && !allstop) {
			// if (_currently_playing->state() != Trigger::WaitingToStart) {
			// std::cerr <<"transport not rolling and trigger in state " << enum_2_string (_currently_playing->state()) << std::endl;
			//}
			maybe_request_roll (_session);
			return;
		}
	} else {

		/* _locate_armed is true, so _currently_playing has been
		   fast-forwarded to our position, and is ready to
		   play. However, for MIDI triggers, we may need to dump a
		   bunch of state into our BufferSet to ensure that the state
		   of things matches the way it would have been had we actually
		   played the trigger/slot from the start.
		*/

		if (speed != 0.0) {
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
					send_property_change (Properties::currently_playing);

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
						send_property_change (Properties::currently_playing);

					} else if (_currently_playing->state() != Trigger::WaitingToSwitch) {

						/* Notice that this condition
						 * leaves the next trigger to
						 * run in the queue.
						 */

						/* but just begin stoppingthe currently playing slot */
						_currently_playing->begin_switch (nxt);
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
				if (handle_stopped_trigger (bufs, dest_offset)) {
					break;
				}

			} else {

				_currently_playing = 0;
				send_property_change (Properties::currently_playing);
				DEBUG_TRACE (DEBUG::Triggers, "currently playing was stopped, but stop_all was set #2, leaving nf loop\n");
				/* leave nframes loop */
				break;
			}
		}

		pframes_t frames_covered;


		std::shared_ptr<AudioRegion> ar = std::dynamic_pointer_cast<AudioRegion> (_currently_playing->region());
		if (ar) {
			max_chans = std::max (ar->n_channels(), max_chans);
		}

		/* Quantize offset will generally be zero, but if non-zero, it
		 * represents an initial "skip" in the output buffers required
		 * to align the trigger output with its quantization. This
		 * means that the total portion of the buffer covered by the
		 * trigger is the sum of the quantize offset and "frames
		 * covered".
		 *
		 * Quantize offset will only be non-zero for a trigger that is
		 * actually going to output its initial samples somewhere in
		 * this process cycle (but after the beginning of the cycle).
		 */

		pframes_t quantize_offset;

		frames_covered = _currently_playing->run (bufs, start_sample, end_sample, start_beats, end_beats, nframes, dest_offset, bpm, quantize_offset);

		nframes -= quantize_offset;
		nframes -= frames_covered;
		start_sample += frames_covered;
		dest_offset += frames_covered;

		DEBUG_TRACE (DEBUG::Triggers, string_compose ("trig %1 ran, covered %2 state now %3 nframes now %4\n",
		                                              _currently_playing->name(), frames_covered, enum_2_string (_currently_playing->state()), nframes));

		/* it is possible that the current trigger stopped right on our
		 * run() call boundary. If so, be sure to notice because
		 * otherwise we were already set to break from this
		 * nframes-testing while loop; _currently_playing
		 * will still be set, and we will never progress on subsequent
		 * calls to ::run()
		 */

		if (nframes == 0 && _currently_playing->state() == Trigger::Stopped) {
			if (!_stop_all && !_currently_playing->explicitly_stopped()) {
				std::cerr << "stopped, do handle thing\n";
				(void) handle_stopped_trigger (bufs, dest_offset);
			} else {
				_currently_playing = 0;
				send_property_change (Properties::currently_playing);
			}
		}
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

	possible_targets.reserve (TriggerBox::default_triggers_per_box);

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
				n = 0;
			}

			if (n == current) {
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
		for (std::size_t n = 0; n < TriggerBox::default_triggers_per_box; ++n) {
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
TriggerBox::get_state () const
{
	XMLNode& node (Processor::get_state ());

	node.set_property (X_("type"), X_("triggerbox"));
	node.set_property (X_("data-type"), _data_type.to_string());
	node.set_property (X_("order"), _order);
	XMLNode* trigger_child (new XMLNode (X_("Triggers")));

	{
		Glib::Threads::RWLock::ReaderLock lm (trigger_lock);
		for (auto const & t : all_triggers) {
			trigger_child->add_child_nocopy (t->get_state());
		}
	}

	node.add_child_nocopy (*trigger_child);

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
			if (trig->region ()) {
				_active_slots++;
			}
		}
	}

	/* Since _active_slots may have changed, we could consider sending
	 * EmptyStatusChanged, but for now we don't consider ::set_state() to
	 * be used except at session load.
	 */

	return 0;
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
		break;
	case Request::Reload:
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
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 (%3): non-realtime stop at %2 (lat-adjusted to %4) PO %5 OL %6\n", order(), now, this, now + playback_offset(), playback_offset(), output_latency()));

	for (auto & t : all_triggers) {
		t->shutdown_from_fwd ();
	}

	fast_forward (_session.cue_events(), now);
}

void
TriggerBox::non_realtime_locate (samplepos_t now)
{
	DEBUG_TRACE (DEBUG::Triggers, string_compose ("%1 (%3): non-realtime locate at %2 (lat-adjusted to %4) PO %5 OL %6\n", order(), now, this, now + playback_offset(), playback_offset(), output_latency()));

	for (auto & t : all_triggers) {
		t->shutdown_from_fwd ();
	}

	fast_forward (_session.cue_events(), now);
}

void
TriggerBox::tempo_map_changed ()
{
	/* called from process context, but before Session::process() */

	if (_currently_playing) {
		_currently_playing->tempo_map_changed ();
	}
}

void
TriggerBox::dump (std::ostream & ostr) const
{
	ostr << "TriggerBox " << order() << std::endl;
	for (auto const & t : all_triggers) {
		ostr << "\tTrigger " << t->index() << " state " << enum_2_string (t->state()) << std::endl;
	}
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
TriggerBoxThread::set_region (TriggerBox& box, uint32_t slot, std::shared_ptr<Region> r)
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
