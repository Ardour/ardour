/*
 * Copyright (C) 2019-2020 Robin Gareus <robin@gareus.org>
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

#include "pbd/gstdio_compat.h"
#include <glibmm.h>

#include "pbd/basename.h"
#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/debug.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/tokenizer.h"

#ifdef PLATFORM_WINDOWS
#include "pbd/windows_special_dirs.h"
#include <shlobj.h> // CSIDL_*
#endif

#include "ardour/audio_buffer.h"
#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/rc_configuration.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/stripable.h"
#include "ardour/tempo.h"
#include "ardour/utils.h"
#include "ardour/vst3_module.h"
#include "ardour/vst3_plugin.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace Temporal;
using namespace Steinberg;
using namespace Presonus;

VST3Plugin::VST3Plugin (AudioEngine& engine, Session& session, VST3PI* plug)
	: Plugin (engine, session)
	, _plug (plug)
{
	init ();
}

VST3Plugin::VST3Plugin (const VST3Plugin& other)
	: Plugin (other)
{
	boost::shared_ptr<VST3PluginInfo> nfo = boost::dynamic_pointer_cast<VST3PluginInfo> (other.get_info ());
	_plug = new VST3PI (nfo->m, nfo->unique_id);
	init ();
}

VST3Plugin::~VST3Plugin ()
{
	delete _plug;
}

void
VST3Plugin::init ()
{
#ifndef NDEBUG
	if (DEBUG_ENABLED (DEBUG::VST3Config)) {
		char fuid[33];
		_plug->fuid ().toString (fuid);
		DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3 instantiating FUID: %1\n", fuid));
	}
#endif
	Vst::ProcessContext& context (_plug->context ());
	context.sampleRate = _session.nominal_sample_rate ();
	_plug->set_block_size (_session.get_block_size ());
	_plug->OnResizeView.connect_same_thread (_connections, boost::bind (&VST3Plugin::forward_resize_view, this, _1, _2));
	_plug->OnParameterChange.connect_same_thread (_connections, boost::bind (&VST3Plugin::parameter_change_handler, this, _1, _2, _3));

	/* assume all I/O is connected by default */
	for (int32_t i = 0; i < (int32_t)_plug->n_audio_inputs (); ++i) {
		_connected_inputs.push_back (true);
	}
	for (int32_t i = 0; i < (int32_t)_plug->n_audio_outputs (); ++i) {
		_connected_outputs.push_back (true);
	}
	/* pre-configure from GUI thread */
	_plug->enable_io (_connected_inputs, _connected_outputs);
}

void
VST3Plugin::forward_resize_view (int w, int h)
{
	OnResizeView (w, h); /* EMIT SINAL */
}

void
VST3Plugin::parameter_change_handler (VST3PI::ParameterChange t, uint32_t param, float value)
{
	switch (t) {
		case VST3PI::BeginGesture:
			Plugin::StartTouch (param);
			break;
		case VST3PI::EndGesture:
			Plugin::EndTouch (param);
			break;
		case VST3PI::ValueChange:
			/* emit ParameterChangedExternally, mark preset dirty */
			Plugin::parameter_changed_externally (param, value);
			break;
		case VST3PI::InternalChange:
			Plugin::state_changed ();
			break;
		case VST3PI::PresetChange:
			PresetsChanged (unique_id (), this, false); /* EMIT SIGNAL */
			size_t n_presets = _plug->n_factory_presets (); // ths may be old, invalidated count
			if (_plug->program_change_port ().id != Vst::kNoParamId) {
				int                         pgm  = value * (n_presets > 1 ? (n_presets - 1) : 1);
				std::string                 uri  = string_compose (X_("VST3-P:%1:%2"), unique_id (), std::setw (4), std::setfill ('0'), pgm);
				const Plugin::PresetRecord* pset = Plugin::preset_by_uri (uri);
				if (pset && n_presets == _plug->n_factory_presets ()) {
					Plugin::load_preset (*pset);
					// XXX TODO notify replicated instances, unless plugin implements ISlaveControllerHandler
				}
			}
			break;
	}
}

/* ****************************************************************************
 * Parameter API
 */

uint32_t
VST3Plugin::parameter_count () const
{
	return _plug->parameter_count ();
}

float
VST3Plugin::default_value (uint32_t port)
{
	assert (port < parameter_count ());
	return _plug->default_value (port);
}

void
VST3Plugin::set_parameter (uint32_t port, float val, sampleoffset_t when)
{
	_plug->set_parameter (port, val, when);
	Plugin::set_parameter (port, val, when);
}

float
VST3Plugin::get_parameter (uint32_t port) const
{
	return _plug->get_parameter (port);
}

int
VST3Plugin::get_parameter_descriptor (uint32_t port, ParameterDescriptor& desc) const
{
	assert (port < parameter_count ());
	_plug->get_parameter_descriptor (port, desc);
	desc.update_steps ();
	return 0;
}

uint32_t
VST3Plugin::nth_parameter (uint32_t port, bool& ok) const
{
	if (port < parameter_count ()) {
		ok = true;
		return port;
	}
	ok = false;
	return 0;
}

bool
VST3Plugin::parameter_is_input (uint32_t port) const
{
	return !_plug->parameter_is_readonly (port);
}

bool
VST3Plugin::parameter_is_output (uint32_t port) const
{
	return _plug->parameter_is_readonly (port);
}

uint32_t
VST3Plugin::designated_bypass_port ()
{
	return _plug->designated_bypass_port ();
}

void
VST3Plugin::set_automation_control (uint32_t port, boost::shared_ptr<ARDOUR::AutomationControl> ac)
{
	if (!ac->alist () || !_plug->subscribe_to_automation_changes ()) {
		return;
	}
	ac->alist ()->automation_state_changed.connect_same_thread (_connections, boost::bind (&VST3PI::automation_state_changed, _plug, port, _1, boost::weak_ptr<AutomationList> (ac->alist ())));
}

std::set<Evoral::Parameter>
VST3Plugin::automatable () const
{
	std::set<Evoral::Parameter> automatables;
	for (uint32_t i = 0; i < parameter_count (); ++i) {
		if (parameter_is_input (i) && _plug->parameter_is_automatable (i)) {
			automatables.insert (automatables.end (), Evoral::Parameter (PluginAutomation, 0, i));
		}
	}
	return automatables;
}

std::string
VST3Plugin::describe_parameter (Evoral::Parameter param)
{
	if (param.type () == PluginAutomation && param.id () < parameter_count ()) {
		return _plug->parameter_label (param.id ());
	}
	return "??";
}

bool
VST3Plugin::print_parameter (uint32_t port, std::string& rv) const
{
	rv = _plug->print_parameter (port);
	return rv.size () > 0;
}

Plugin::IOPortDescription
VST3Plugin::describe_io_port (ARDOUR::DataType dt, bool input, uint32_t id) const
{
	if ((dt == DataType::AUDIO &&
	        ((input && id >= _plug->n_audio_inputs())
	          || (!input && id >= _plug->n_audio_outputs())))
	    || (dt == DataType::MIDI &&
	        ((input && id >= _plug->n_midi_inputs())
	          || (!input && id >= _plug->n_midi_outputs())))) {
		return Plugin::describe_io_port(dt, input, id);
	}

	return _plug->describe_io_port (dt, input, id);
}

PluginOutputConfiguration
VST3Plugin::possible_output () const
{
	return Plugin::possible_output (); // TODO
}

/* ****************************************************************************
 * Plugin UI
 */

bool
VST3Plugin::has_editor () const
{
	return _plug->has_editor ();
}

Steinberg::IPlugView*
VST3Plugin::view ()
{
	return _plug->view ();
}

void
VST3Plugin::close_view ()
{
	_plug->close_view ();
}

#if SMTG_OS_LINUX
void
VST3Plugin::set_runloop (Steinberg::Linux::IRunLoop* run_loop)
{
	return _plug->set_runloop (run_loop);
}
#endif

void
VST3Plugin::update_contoller_param ()
{
	/* GUI Thread */
	_plug->update_contoller_param ();
}

/* ****************************************************************************
 * MIDI converters
 */

bool
VST3PI::evoral_to_vst3 (Vst::Event& e, Evoral::Event<samplepos_t> const& ev, int32_t bus)
{
	const uint32_t size = ev.size ();
	if (size == 0) {
		return false;
	}

	const uint8_t* data   = ev.buffer ();
	uint8_t        status = data[0];

	if (status >= 0x80 && status < 0xF0) {
		status &= 0xf0;
	}

	if (size == 2 || size == 3) {
		Vst::ParamID id = Vst::kNoParamId;

		const uint8_t channel = data[0] & 0x0f;
		const uint8_t data1   = data[1] & 0x7f;
		const uint8_t data2   = size == 3 ? (data[2] & 0x7f) : 0;

		switch (status) {
			case MIDI_CMD_NOTE_OFF:
				e.type             = Vst::Event::kNoteOffEvent;
				e.noteOff.channel  = channel;
				e.noteOff.noteId   = -1;
				e.noteOff.pitch    = data1;
				e.noteOff.velocity = data2 / 127.f;
				e.noteOff.tuning   = 0.f;
				return true;
			case MIDI_CMD_NOTE_ON:
				e.type            = Vst::Event::kNoteOnEvent;
				e.noteOn.channel  = channel;
				e.noteOn.noteId   = -1;
				e.noteOn.pitch    = data1;
				e.noteOn.velocity = data2 / 127.f;
				e.noteOn.length   = 0;
				e.noteOn.tuning   = 0.f;
				return true;
			case MIDI_CMD_NOTE_PRESSURE:
				e.type                  = Vst::Event::kPolyPressureEvent;
				e.polyPressure.channel  = channel;
				e.polyPressure.pitch    = data1;
				e.polyPressure.pressure = data2 / 127.f;
				e.polyPressure.noteId   = -1;
				return true;
			case MIDI_CMD_CONTROL:
				if (ev.is_live_midi () /* live input -- no playback */) {
					live_midi_cc (bus, channel, data1);
				}
				if (midi_controller (bus, channel, data1, id)) {
					set_parameter_by_id (id, data2 / 127.f, ev.time ());
				}
				return false;
			case MIDI_CMD_PGM_CHANGE:
				assert (size == 2);
				set_program (data2, ev.time ());
				return false;
			case MIDI_CMD_CHANNEL_PRESSURE:
				assert (size == 2);
				if (midi_controller (bus, channel, Vst::kAfterTouch, id)) {
					set_parameter_by_id (id, data1 / 127.f, ev.time ());
				}
				return false;
			case MIDI_CMD_BENDER:
				if (midi_controller (bus, channel, Vst::kPitchBend, id)) {
					uint32_t m14 = (data2 << 7) | data1;
					set_parameter_by_id (id, m14 / 16383.f, ev.time ());
				}
				return false;
		}
	} else if (status == MIDI_CMD_COMMON_SYSEX) {
		memset (&e, 0, sizeof (Vst::Event));
		e.type       = Vst::Event::kDataEvent;
		e.data.type  = Vst::DataEvent::kMidiSysEx;
		e.data.bytes = ev.buffer (); // TODO copy ?!
		e.data.size  = ev.size ();
		return true;
	}
	return false;
}

#define vst_to_midi(x) (static_cast<uint8_t> ((x)*127.f) & 0x7f)

void
VST3PI::vst3_to_midi_buffers (BufferSet& bufs, ChanMapping const& out_map)
{
	for (int32 i = 0; i < _output_events.getEventCount (); ++i) {
		Vst::Event e;
		if (_output_events.getEvent (i, e) == kResultFalse) {
			continue;
		}

		bool     valid = false;
		uint32_t index = out_map.get (DataType::MIDI, e.busIndex, &valid);
		if (!valid || bufs.count ().n_midi () <= index) {
			DEBUG_TRACE (DEBUG::VST3Process, string_compose ("VST3PI::vst3_to_midi_buffers - Invalid MIDI Bus %1\n", e.busIndex));
			continue;
		}

		MidiBuffer& mb = bufs.get_midi (index);
		uint8_t     data[3];

		switch (e.type) {
			case Vst::Event::kDataEvent:
				/* sysex */
				mb.push_back (e.sampleOffset, Evoral::MIDI_EVENT, e.data.size, (uint8_t const*)e.data.bytes);
				break;
			case Vst::Event::kNoteOffEvent:
				data[0] = MIDI_CMD_NOTE_OFF | e.noteOff.channel;
				data[1] = e.noteOff.pitch;
				data[2] = vst_to_midi (e.noteOff.velocity);
				mb.push_back (e.sampleOffset, Evoral::MIDI_EVENT, 3, data);
				break;
			case Vst::Event::kNoteOnEvent:
				data[0] = MIDI_CMD_NOTE_ON | e.noteOn.channel;
				data[1] = e.noteOn.pitch;
				data[2] = vst_to_midi (e.noteOn.velocity);
				mb.push_back (e.sampleOffset, Evoral::MIDI_EVENT, 3, data);
				break;
			case Vst::Event::kPolyPressureEvent:
				data[0] = MIDI_CMD_NOTE_PRESSURE | e.noteOff.channel;
				data[1] = e.polyPressure.pitch;
				data[2] = vst_to_midi (e.polyPressure.pressure);
				mb.push_back (e.sampleOffset, Evoral::MIDI_EVENT, 3, data);
				break;
			case Vst::Event::kLegacyMIDICCOutEvent:
				switch (e.midiCCOut.controlNumber) {
					case Vst::kCtrlPolyPressure:
						data[0] = MIDI_CMD_NOTE_PRESSURE | e.midiCCOut.channel;
						data[1] = e.midiCCOut.value;
						data[2] = e.midiCCOut.value2;
						break;
					default: /* Control Change */
						data[0] = MIDI_CMD_CONTROL | e.midiCCOut.channel;
						data[1] = e.midiCCOut.controlNumber;
						data[2] = e.midiCCOut.value;
						break;
					case Vst::kCtrlProgramChange:
						data[0] = MIDI_CMD_PGM_CHANGE | e.midiCCOut.channel;
						data[1] = e.midiCCOut.value;
						data[2] = e.midiCCOut.value2;
						break;
					case Vst::kAfterTouch:
						data[0] = MIDI_CMD_CHANNEL_PRESSURE | e.midiCCOut.channel;
						data[1] = e.midiCCOut.value;
						data[2] = e.midiCCOut.value2;
						break;
					case Vst::kPitchBend:
						data[0] = MIDI_CMD_BENDER | e.midiCCOut.channel;
						data[1] = e.midiCCOut.value;
						data[2] = e.midiCCOut.value2;
						break;
				}
				mb.push_back (e.sampleOffset, Evoral::MIDI_EVENT, e.midiCCOut.controlNumber == Vst::kCtrlProgramChange ? 2 : 3, data);
				break;

			case Vst::Event::kNoteExpressionValueEvent:
			case Vst::Event::kNoteExpressionTextEvent:
			case Vst::Event::kChordEvent:
			case Vst::Event::kScaleEvent:
			default:
				/* unsupported, unhandled event */
				break;
		}
	}
}

/* ****************************************************************************/

void
VST3Plugin::add_state (XMLNode* root) const
{
	XMLNode* child;
	for (uint32_t i = 0; i < parameter_count (); ++i) {
		if (!parameter_is_input (i)) {
			continue;
		}
		child = new XMLNode ("Port");
		child->set_property ("id", (uint32_t)_plug->index_to_id (i));
		child->set_property ("value", _plug->get_parameter (i));
		root->add_child_nocopy (*child);
	}

	RAMStream stream;
	if (_plug->save_state (stream)) {
		gchar* data = g_base64_encode (stream.data (), stream.size ());
		if (data == 0) {
			return;
		}

		XMLNode* chunk_node = new XMLNode (X_("chunk"));
		chunk_node->add_content (data);
		g_free (data);
		root->add_child_nocopy (*chunk_node);
	}
}

int
VST3Plugin::set_state (const XMLNode& node, int version)
{
	XMLNodeConstIterator iter;

	if (node.name () != state_node_name ()) {
		error << string_compose (_("VST3<%1>: Bad node sent to VST3Plugin::set_state"), name ()) << endmsg;
		return -1;
	}

	const Plugin::PresetRecord* r = 0;
	std::string                 preset_uri;
	if (node.get_property (X_("last-preset-uri"), preset_uri)) {
		r = preset_by_uri (preset_uri);
	}
	if (r && _plug->program_change_port ().id != Vst::kNoParamId) {
		std::vector<std::string> tmp;
		if (PBD::tokenize (r->uri, std::string (":"), std::back_inserter (tmp)) && tmp.size () == 3 && tmp[0] == "VST3-P") {
			float  value     = PBD::atoi (tmp[2]);
			size_t n_presets = _plug->n_factory_presets ();
			if (n_presets > 1) {
				value /= (n_presets - 1.f);
			}
			DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3Plugin::set_state: set_program (pgm: %1 plug: %2)\n", value, name ()));
			_plug->controller ()->setParamNormalized (_plug->program_change_port ().id, value);
		}
	}

	XMLNodeList nodes = node.children ("Port");
	for (iter = nodes.begin (); iter != nodes.end (); ++iter) {
		XMLNode* child = *iter;

		uint32_t param_id;
		float    value;

		if (!child->get_property ("id", param_id)) {
			warning << string_compose (_("VST3<%1>: Missing parameter-id in VST3Plugin::set_state"), name ()) << endmsg;
			continue;
		}

		if (!child->get_property ("value", value)) {
			warning << string_compose (_("VST3<%1>: Missing parameter value in VST3Plugin::set_state"), name ()) << endmsg;
			continue;
		}

		if (!_plug->try_set_parameter_by_id (param_id, value)) {
			warning << string_compose (_("VST3<%1>: Invalid Vst::ParamID in VST3Plugin::set_state"), name ()) << endmsg;
		}
	}

	XMLNode* chunk;
	if ((chunk = find_named_node (node, X_("chunk"))) != 0) {
		for (iter = chunk->children ().begin (); iter != chunk->children ().end (); ++iter) {
			if ((*iter)->is_content ()) {
				gsize     size  = 0;
				guchar*   _data = g_base64_decode ((*iter)->content ().c_str (), &size);
				RAMStream stream (_data, size);
				if (!_plug->load_state (stream)) {
					error << string_compose (_("VST3<%1>: failed to load chunk-data"), name ()) << endmsg;
				}
			}
		}
	}

	return Plugin::set_state (node, version);
}

/* ****************************************************************************/

void
VST3Plugin::set_owner (ARDOUR::SessionObject* o)
{
	Plugin::set_owner (o);
	_plug->set_owner (o);
}

int
VST3Plugin::set_block_size (pframes_t n_samples)
{
	_plug->set_block_size (n_samples);
	return 0;
}

samplecnt_t
VST3Plugin::plugin_latency () const
{
	return _plug->plugin_latency ();
}

void
VST3Plugin::add_slave (boost::shared_ptr<Plugin> p, bool rt)
{
	boost::shared_ptr<VST3Plugin> vst = boost::dynamic_pointer_cast<VST3Plugin> (p);
	if (vst) {
		_plug->add_slave (vst->_plug->controller (), rt);
	}
}

void
VST3Plugin::remove_slave (boost::shared_ptr<Plugin> p)
{
	boost::shared_ptr<VST3Plugin> vst = boost::dynamic_pointer_cast<VST3Plugin> (p);
	if (vst) {
		_plug->remove_slave (vst->_plug->controller ());
	}
}

int
VST3Plugin::connect_and_run (BufferSet&  bufs,
                             samplepos_t start, samplepos_t end, double speed,
                             ChanMapping const& in_map, ChanMapping const& out_map,
                             pframes_t n_samples, samplecnt_t offset)
{
	DEBUG_TRACE (DEBUG::VST3Process, string_compose ("%1 run %2 offset %3\n", name (), n_samples, offset));
	Plugin::connect_and_run (bufs, start, end, speed, in_map, out_map, n_samples, offset);

	Vst::ProcessContext& context (_plug->context ());

	/* clear event ports */
	_plug->cycle_start ();

	context.state =
	    Vst::ProcessContext::kContTimeValid | Vst::ProcessContext::kSystemTimeValid | Vst::ProcessContext::kSmpteValid | Vst::ProcessContext::kProjectTimeMusicValid | Vst::ProcessContext::kBarPositionValid | Vst::ProcessContext::kTempoValid | Vst::ProcessContext::kTimeSigValid | Vst::ProcessContext::kClockValid;

	context.projectTimeSamples   = start;
	context.continousTimeSamples = _engine.processed_samples ();
	context.systemTime           = g_get_monotonic_time ();

	{
		TempoMap::SharedPtr tmap (TempoMap::use());
		const TempoMetric&  metric (tmap->metric_at (start));
		const BBT_Time&     bbt (metric.bbt_at (timepos_t (start)));

		context.tempo              = metric.tempo().quarter_notes_per_minute ();
		context.timeSigNumerator   = metric.meter().divisions_per_bar ();
		context.timeSigDenominator = metric.meter().note_value ();
		context.projectTimeMusic   = DoubleableBeats (metric.tempo().quarters_at_sample (start)).to_double();
		context.barPositionMusic   = bbt.bars * 4; // PPQN, NOT tmap.metric_at(bbt).meter().divisions_per_bar()
	}

	const double tcfps                = _session.timecode_frames_per_second ();
	context.frameRate.framesPerSecond = ceil (tcfps);
	context.frameRate.flags           = 0;
	if (_session.timecode_drop_frames ()) {
		context.frameRate.flags = Vst::FrameRate::kDropRate; /* 29.97 */
	} else if (tcfps > context.frameRate.framesPerSecond) {
		context.frameRate.flags = Vst::FrameRate::kPullDownRate; /* 23.976 etc */
	}

	if (_session.get_play_loop ()) {
		Location* looploc = _session.locations ()->auto_loop_location ();
		try {
			/* loop start/end in quarter notes */

			TempoMap::SharedPtr tmap (TempoMap::use());
			context.cycleStartMusic = DoubleableBeats (tmap->quarters_at (looploc->start ())).to_double ();
			context.cycleEndMusic   = DoubleableBeats (tmap->quarters_at (looploc->end ())).to_double ();
			 context.state |= Vst::ProcessContext::kCycleValid;
			context.state |= Vst::ProcessContext::kCycleActive;
		} catch (...) {
		}
	}
	if (speed != 0) {
		context.state |= Vst::ProcessContext::kPlaying;
	}
	if (_session.actively_recording ()) {
		context.state |= Vst::ProcessContext::kRecording;
	}
#if 0 // TODO
	context.state |= Vst::ProcessContext::kClockValid;
	context.samplesToNextClock = 0 // MIDI Clock Resolution (24 Per Quarter Note), can be negative (nearest);
#endif

	ChanCount bufs_count;
	bufs_count.set (DataType::AUDIO, 1);
	bufs_count.set (DataType::MIDI, 1);

	BufferSet& silent_bufs  = _session.get_silent_buffers (bufs_count);
	BufferSet& scratch_bufs = _session.get_scratch_buffers (bufs_count);

	uint32_t n_bin  = std::max<uint32_t> (1, _plug->n_audio_inputs ());
	uint32_t n_bout = std::max<uint32_t> (1, _plug->n_audio_outputs ());

	float** ins  = (float**)alloca (n_bin  * sizeof (float*));
	float** outs = (float**)alloca (n_bout * sizeof (float*));

	uint32_t in_index = 0;
	for (int32_t i = 0; i < (int32_t)_plug->n_audio_inputs (); ++i) {
		uint32_t index;
		bool     valid = false;
		index          = in_map.get (DataType::AUDIO, in_index++, &valid);
		ins[i]         = (valid)
		             ? bufs.get_audio (index).data (offset)
		             : silent_bufs.get_audio (0).data (offset);
		_connected_inputs[i] = valid;
	}

	uint32_t out_index = 0;
	for (int32_t i = 0; i < (int32_t)_plug->n_audio_outputs (); ++i) {
		uint32_t index;
		bool     valid = false;
		index          = out_map.get (DataType::AUDIO, out_index++, &valid);
		outs[i]        = (valid)
		              ? bufs.get_audio (index).data (offset)
		              : scratch_bufs.get_audio (0).data (offset);
		_connected_outputs[i] = valid;
	}

	in_index = 0;
	for (int32_t i = 0; i < (int32_t)_plug->n_midi_inputs (); ++i) {
		bool     valid = false;
		uint32_t index = in_map.get (DataType::MIDI, in_index++, &valid);
		if (valid && bufs.count ().n_midi () > index) {
			for (MidiBuffer::iterator m = bufs.get_midi (index).begin (); m != bufs.get_midi (index).end (); ++m) {
				const Evoral::Event<samplepos_t> ev (*m, false);
				_plug->add_event (ev, i);
			}
		}
	}

	_plug->enable_io (_connected_inputs, _connected_outputs);

	_plug->process (ins, outs, n_samples);

	/* handle outgoing MIDI events */
	if (_plug->n_midi_outputs () > 0 && bufs.count ().n_midi () > 0) {
		/* clear valid in-place MIDI buffers (forward MIDI otherwise) */
		in_index = 0;
		for (int32_t i = 0; i < (int32_t)_plug->n_midi_inputs (); ++i) {
			bool     valid = false;
			uint32_t index = in_map.get (DataType::MIDI, in_index++, &valid);
			if (valid && bufs.count ().n_midi () > index) {
				bufs.get_midi (index).clear ();
			}
		}

		_plug->vst3_to_midi_buffers (bufs, out_map);
	}

	return 0;
}

/* ****************************************************************************/

bool
VST3Plugin::load_preset (PresetRecord r)
{
	bool ok = false;

	/* Extract the UUID of this preset from the URI */
	std::vector<std::string> tmp;
	if (!PBD::tokenize (r.uri, std::string (":"), std::back_inserter (tmp))) {
		return false;
	}
	if (tmp.size () != 3) {
		return false;
	}

	std::string const& unique_id = tmp[1];

	FUID fuid;
	if (!fuid.fromString (unique_id.c_str ()) || fuid != _plug->fuid ()) {
		assert (0);
		return false;
	}

	if (tmp[0] == "VST3-P") {
		int program = PBD::atoi (tmp[2]);
		assert (!r.user);
		if (!_plug->set_program (program, 0)) {
			DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3Plugin::load_preset: set_program failed (pgm: %1 plug: %2)\n", program, name ()));
			return false;
		}
		ok = true;
	} else if (tmp[0] == "VST3-S") {
		if (_preset_uri_map.find (r.uri) == _preset_uri_map.end ()) {
			/* build _preset_uri_map for replicated instances */
			find_presets ();
		}
		assert (_preset_uri_map.find (r.uri) != _preset_uri_map.end ());
		std::string const& fn = _preset_uri_map[r.uri];

		if (Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
			RAMStream stream (fn);
			ok = _plug->load_state (stream);
			DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3Plugin::load_preset: file %1 status %2\n", fn, ok ? "OK" : "error"));
		}
	}

	if (ok) {
		Plugin::load_preset (r);
	}
	return ok;
}

std::string
VST3Plugin::do_save_preset (std::string name)
{
	assert (!preset_search_path ().empty ());
	std::string dir = preset_search_path ().front ();
	std::string fn  = Glib::build_filename (dir, legalize_for_universal_path (name) + ".vstpreset");

	if (g_mkdir_with_parents (dir.c_str (), 0775)) {
		error << string_compose (_("Could not create VST3 Preset Folder '%1'"), dir) << endmsg;
	}

	RAMStream stream;
	if (_plug->save_state (stream)) {
		GError* err = NULL;
		if (!g_file_set_contents (fn.c_str (), (const gchar*)stream.data (), stream.size (), &err)) {
			::g_unlink (fn.c_str ());
			if (err) {
				error << string_compose (_("Could not save VST3 Preset (%1)"), err->message) << endmsg;
				g_error_free (err);
			}
			return "";
		}
		std::string uri      = string_compose (X_("VST3-S:%1:%2"), unique_id (), PBD::basename_nosuffix (fn));
		_preset_uri_map[uri] = fn;
		return uri;
	}
	return "";
}

void
VST3Plugin::do_remove_preset (std::string name)
{
	assert (!preset_search_path ().empty ());
	std::string dir = preset_search_path ().front ();
	std::string fn  = Glib::build_filename (dir, legalize_for_universal_path (name) + ".vstpreset");
	::g_unlink (fn.c_str ());
	std::string uri = string_compose (X_("VST3-S:%1:%2"), unique_id (), PBD::basename_nosuffix (fn));
	if (_preset_uri_map.find (uri) != _preset_uri_map.end ()) {
		_preset_uri_map.erase (_preset_uri_map.find (uri));
	}
}

static bool
vst3_preset_filter (const std::string& str, void*)
{
	return str[0] != '.' && (str.length () > 9 && str.find (".vstpreset") == (str.length () - 10));
}

void
VST3Plugin::find_presets ()
{
	_presets.clear ();
	_preset_uri_map.clear ();

	/* read vst3UnitPrograms */
	IPtr<Vst::IUnitInfo> nfo = _plug->unit_info ();
	if (nfo && _plug->program_change_port ().id != Vst::kNoParamId) {
		Vst::UnitID program_unit_id = _plug->program_change_port ().unitId;

		int32 unit_count = nfo->getUnitCount ();

		for (int32 idx = 0; idx < unit_count; ++idx) {
			Vst::UnitInfo unit_info;
			if (!(nfo->getUnitInfo (idx, unit_info) == kResultOk && unit_info.id == program_unit_id)) {
				continue;
			}

			int32 count = nfo->getProgramListCount ();
			for (int32 i = 0; i < count; ++i) {
				Vst::ProgramListInfo pli;
				if (nfo->getProgramListInfo (i, pli) != kResultTrue) {
					continue;
				}
				if (pli.id != unit_info.programListId) {
					continue;
				}

				for (int32 j = 0; j < pli.programCount; ++j) {
					Vst::String128 pname;
					if (nfo->getProgramName (pli.id, j, pname) == kResultTrue) {
						std::string preset_name = tchar_to_utf8 (pname);
						if (preset_name.empty ()) {
							warning << string_compose (_("VST3<%1>: ignored unnamed factory preset/program"), name ()) << endmsg;
							continue;
						}
						std::string  uri = string_compose (X_("VST3-P:%1:%2"), unique_id (), std::setw (4), std::setfill ('0'), j);
						PresetRecord r (uri, preset_name, false);
						_presets.insert (make_pair (uri, r));
					}
					if (nfo->hasProgramPitchNames (pli.id, j)) {
						// TODO -> midnam
					}
				}
				break; // only one program list
			}
			break; // only one unit
		}
	}

	if (_presets.empty () && _plug->program_change_port ().id != Vst::kNoParamId) {
		/* fill in presets by number */
		Vst::ParameterInfo const& pi (_plug->program_change_port ());
		int32                     n_programs = pi.stepCount + 1;
		for (int32 i = 0; i < n_programs; ++i) {
			float       value       = static_cast<Vst::ParamValue> (i) / static_cast<Vst::ParamValue> (pi.stepCount);
			std::string preset_name = _plug->print_parameter (pi.id, value);
			if (!preset_name.empty ()) {
				std::string  uri = string_compose (X_("VST3-P:%1:%2"), unique_id (), std::setw (4), std::setfill ('0'), i);
				PresetRecord r (uri, preset_name, false);
				_presets.insert (make_pair (uri, r));
			}
		}
	}

	_plug->set_n_factory_presets (_presets.size ());

	// TODO check _plug->unit_data()
	// IUnitData: programDataSupported -> setUnitProgramData (IBStream)

	PBD::Searchpath          psp = preset_search_path ();
	std::vector<std::string> preset_files;
	find_paths_matching_filter (preset_files, psp, vst3_preset_filter, 0, false, true, false);

	for (std::vector<std::string>::iterator i = preset_files.begin (); i != preset_files.end (); ++i) {
		bool        is_user     = PBD::path_is_within (psp.front (), *i);
		std::string preset_name = PBD::basename_nosuffix (*i);
		std::string uri         = string_compose (X_("VST3-S:%1:%2"), unique_id (), preset_name);
		if (_presets.find (uri) != _presets.end ()) {
			continue;
		}
		PresetRecord r (uri, preset_name, is_user);
		_presets.insert (make_pair (uri, r));
		_preset_uri_map[uri] = *i;
	}
}

PBD::Searchpath
VST3Plugin::preset_search_path () const
{
	boost::shared_ptr<VST3PluginInfo> nfo = boost::dynamic_pointer_cast<VST3PluginInfo> (get_info ());

	std::string vendor = legalize_for_universal_path (nfo->creator);
	std::string name   = legalize_for_universal_path (nfo->name);

	/* first listed is used to save custom user-presets */
	PBD::Searchpath preset_path;
#ifdef __APPLE__
	preset_path += Glib::build_filename (Glib::get_home_dir (), "Library/Audio/Presets", vendor, name);
	preset_path += Glib::build_filename ("/Library/Audio/Presets", vendor, name);
#elif defined PLATFORM_WINDOWS
	std::string documents = PBD::get_win_special_folder_path (CSIDL_PERSONAL);
	if (!documents.empty ()) {
		preset_path += Glib::build_filename (documents, "VST3 Presets", vendor, name);
		preset_path += Glib::build_filename (documents, "vst3 presets", vendor, name);
	}

	preset_path += Glib::build_filename (Glib::get_user_data_dir (), "VST3 Presets", vendor, name);

	std::string appdata = PBD::get_win_special_folder_path (CSIDL_APPDATA);
	if (!appdata.empty ()) {
		preset_path += Glib::build_filename (appdata, "VST3 Presets", vendor, name);
		preset_path += Glib::build_filename (appdata, "vst3 presets", vendor, name);
	}
#else
	preset_path += Glib::build_filename (Glib::get_home_dir (), ".vst3", "presets", vendor, name);
	preset_path += Glib::build_filename ("/usr/share/vst3/presets", vendor, name);
	preset_path += Glib::build_filename ("/usr/local/share/vst3/presets", vendor, name);
#endif

	return preset_path;
}

/* ****************************************************************************/

VST3PluginInfo::VST3PluginInfo ()
{
	type = ARDOUR::VST3;
}

PluginPtr
VST3PluginInfo::load (Session& session)
{
	try {
		if (!m) {
			DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3 Loading: %1\n", path));
			m = VST3PluginModule::load (path);
		}
		PluginPtr          plugin;
		Steinberg::VST3PI* plug = new VST3PI (m, unique_id);
		plugin.reset (new VST3Plugin (session.engine (), session, plug));
		plugin->set_info (PluginInfoPtr (new VST3PluginInfo (*this)));
		return plugin;
	} catch (failed_constructor& err) {
		;
	}

	return PluginPtr ();
}

std::vector<Plugin::PresetRecord>
VST3PluginInfo::get_presets (bool /*user_only*/) const
{
	std::vector<Plugin::PresetRecord> p;
	return p;
}

bool
VST3PluginInfo::is_instrument () const
{
	if (category.find (Vst::PlugType::kInstrument) != std::string::npos) {
		return true;
	}

	return PluginInfo::is_instrument ();
}

/* ****************************************************************************/

VST3PI::VST3PI (boost::shared_ptr<ARDOUR::VST3PluginModule> m, std::string unique_id)
	: _module (m)
	, _component (0)
	, _controller (0)
	, _view (0)
#if SMTG_OS_LINUX
	, _run_loop (0)
#endif
	, _is_processing (false)
	, _block_size (0)
	, _port_id_bypass (UINT32_MAX)
	, _owner (0)
	, _add_to_selection (false)
	, _n_factory_presets (0)
{
	using namespace std;
	IPluginFactory* factory = m->factory ();

	if (!factory) {
		throw failed_constructor ();
	}

	if (!_fuid.fromString (unique_id.c_str ())) {
		throw failed_constructor ();
	}

#ifndef NDEBUG
	if (DEBUG_ENABLED (DEBUG::VST3Config)) {
		char fuid[33];
		_fuid.toString (fuid);
		DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI create instance %1\n", fuid));
	}
#endif

	if (factory->createInstance (_fuid.toTUID (), Vst::IComponent::iid, (void**)&_component) != kResultTrue) {
		DEBUG_TRACE (DEBUG::VST3Config, "VST3PI create instance failed\n");
		throw failed_constructor ();
	}

	if (!_component || _component->initialize (HostApplication::getHostContext ()) != kResultOk) {
		DEBUG_TRACE (DEBUG::VST3Config, "VST3PI component initialize failed\n");
		throw failed_constructor ();
	}

	_controller = FUnknownPtr<Vst::IEditController> (_component).take ();

	if (!_controller) {
		TUID controllerCID;
		if (_component->getControllerClassId (controllerCID) == kResultTrue) {
			if (factory->createInstance (controllerCID, Vst::IEditController::iid, (void**)&_controller) != kResultTrue) {
				_component->terminate ();
				_component->release ();
				throw failed_constructor ();
			}
		}
	}

	if (!_controller) {
		DEBUG_TRACE (DEBUG::VST3Config, "VST3PI no controller was found\n");
		_component->terminate ();
		_component->release ();
		throw failed_constructor ();
	}

	/* The official Steinberg SDK's source/vst/hosting/plugprovider.cpp
	 * only initializes the controller if it is separate of the component.
	 *
	 * However some plugins expect and unconditional call and other
	 * hosts incl. JUCE based one initialize a controller separately because
	 * FUnknownPtr<> cast may return a new obeject.
	 *
	 * So do not check for errors.
	 * if Vst::IEditController is-a Vst::IComponent the Controller
	 * may or may not already be initialized.
	 */
	_controller->initialize (HostApplication::getHostContext ());

	if (_controller->setComponentHandler (this) != kResultOk) {
		_controller->terminate ();
		_controller->release ();
		_component->terminate ();
		_component->release ();
		throw failed_constructor ();
	}

	if (!(_processor = FUnknownPtr<Vst::IAudioProcessor> (_component).take ())) {
		_controller->terminate ();
		_controller->release ();
		_component->terminate ();
		_component->release ();
		throw failed_constructor ();
	}

	_processor->addRef ();

	/* prepare process context */
	memset (&_context, 0, sizeof (Vst::ProcessContext));

	/* bus-count for process-context */
	_n_bus_in  = _component->getBusCount (Vst::kAudio, Vst::kInput);
	_n_bus_out = _component->getBusCount (Vst::kAudio, Vst::kOutput);

	_busbuf_in.resize (_n_bus_in);
	_busbuf_out.resize (_n_bus_out);

	/* do not re-order, _io_name is build in sequence */
	_n_inputs       = count_channels (Vst::kAudio, Vst::kInput,  Vst::kMain);
	_n_aux_inputs   = count_channels (Vst::kAudio, Vst::kInput,  Vst::kAux);
	_n_outputs      = count_channels (Vst::kAudio, Vst::kOutput, Vst::kMain);
	_n_aux_outputs  = count_channels (Vst::kAudio, Vst::kOutput, Vst::kAux);
	_n_midi_inputs  = count_channels (Vst::kEvent, Vst::kInput,  Vst::kMain);
	_n_midi_outputs = count_channels (Vst::kEvent, Vst::kOutput, Vst::kMain);

	if (!connect_components ()) {
		//_controller->terminate(); // XXX ?
		_component->terminate ();
		_component->release ();
		throw failed_constructor ();
	}

	memset (&_program_change_port, 0, sizeof (_program_change_port));
	_program_change_port.id = Vst::kNoParamId;

	FUnknownPtr<Vst::IEditControllerHostEditing> host_editing (_controller);

	FUnknownPtr<Vst::IEditController2> controller2 (_controller);
	if (controller2) {
		controller2->setKnobMode (Vst::kLinearMode);
	}

	int32 n_params = _controller->getParameterCount ();
	for (int32 i = 0; i < n_params; ++i) {
		Vst::ParameterInfo pi;
		if (_controller->getParameterInfo (i, pi) != kResultTrue) {
			continue;
		}
		if (pi.flags & Vst::ParameterInfo::kIsProgramChange) {
			_program_change_port = pi;
			continue;
		}
		/* allow non-automatable parameters IFF IEditControllerHostEditing is available */
		if (0 == (pi.flags & Vst::ParameterInfo::kCanAutomate) && !host_editing) {
			/* but allow read-only, not automatable params (ctrl outputs) */
			if (0 == (pi.flags & Vst::ParameterInfo::kIsReadOnly)) {
				continue;
			}
		}
		if (tchar_to_utf8 (pi.title).find ("MIDI CC ") != std::string::npos) {
			/* Some JUCE plugins add 16 * 128 automatable MIDI CC parameters */
			continue;
		}

		Param p;
		p.id          = pi.id;
		p.label       = tchar_to_utf8 (pi.title).c_str ();
		p.unit        = tchar_to_utf8 (pi.units).c_str ();
		p.steps       = pi.stepCount;
		p.normal      = pi.defaultNormalizedValue;
		p.is_enum     = 0 != (pi.flags & Vst::ParameterInfo::kIsList);
		p.read_only   = 0 != (pi.flags & Vst::ParameterInfo::kIsReadOnly);
		p.automatable = 0 != (pi.flags & Vst::ParameterInfo::kCanAutomate);

		if (pi.flags & /*Vst::ParameterInfo::kIsHidden*/ (1 << 4)) {
			p.label = X_("hidden");
		}

		uint32_t idx = _ctrl_params.size ();
		_ctrl_params.push_back (p);

		if (pi.flags & Vst::ParameterInfo::kIsBypass) {
			_port_id_bypass = idx;
		}
		_ctrl_id_index[pi.id] = idx;
		_ctrl_index_id[idx]   = pi.id;

		_shadow_data.push_back (p.normal);
		_update_ctrl.push_back (false);
	}

	_input_param_changes.set_n_params (n_params);
	_output_param_changes.set_n_params (n_params);

	synchronize_states ();

	/* enable all MIDI busses */
	set_event_bus_state (true);
}

VST3PI::~VST3PI ()
{
	terminate ();
}

IPtr<Vst::IUnitInfo>
VST3PI::unit_info ()
{
	IPtr<Vst::IUnitInfo> nfo = FUnknownPtr<Vst::IUnitInfo> (_component);
	if (nfo) {
		return nfo;
	}
	return FUnknownPtr<Vst::IUnitInfo> (_controller);
}

#if 0
IPtr<Vst::IUnitData>
VST3PI::unit_data ()
{
	Vst::IUnitData* iud = FUnknownPtr<Vst::IUnitData> (_component);
	if (iud) {
		return iud;
	}
	return FUnknownPtr<Vst::IUnitData> (_controller);
}
#endif

void
VST3PI::terminate ()
{
	assert (!_view);
	/* disable all MIDI busses */
	set_event_bus_state (false);

	deactivate ();

	_processor->release ();
	_processor = 0;

	disconnect_components ();

	if (_controller) {
		_controller->setComponentHandler (0);
		_controller->terminate ();
		_controller->release ();
	}

	if (_component) {
		_component->terminate ();
		_component->release ();
	}

	_controller = 0;
	_component  = 0;
}

bool
VST3PI::connect_components ()
{
	if (!_component || !_controller) {
		return false;
	}

	FUnknownPtr<Vst::IConnectionPoint> componentCP (_component);
	FUnknownPtr<Vst::IConnectionPoint> controllerCP (_controller);

	if (!componentCP || !controllerCP) {
		return true;
	}

	_component_cproxy  = boost::shared_ptr<ConnectionProxy> (new ConnectionProxy (componentCP));
	_controller_cproxy = boost::shared_ptr<ConnectionProxy> (new ConnectionProxy (controllerCP));

	tresult res = _component_cproxy->connect (controllerCP);
	if (!(res == kResultOk || res == kNotImplemented)) {
		DEBUG_TRACE (DEBUG::VST3Config, "VST3PI::connect_components Cannot connect controller to component\n");
		//return false;
	}

	res = _controller_cproxy->connect (componentCP);
	if (!(res == kResultOk || res == kNotImplemented)) {
		DEBUG_TRACE (DEBUG::VST3Config, "VST3PI::connect_components Cannot connect component to controller\n");
	}

	return true;
}

bool
VST3PI::disconnect_components ()
{
	if (!_component_cproxy || !_controller_cproxy) {
		return false;
	}

	bool rv = _component_cproxy->disconnect ();
	rv &= _controller_cproxy->disconnect ();

	_component_cproxy.reset ();
	_controller_cproxy.reset ();

	return rv;
}

tresult
VST3PI::queryInterface (const TUID _iid, void** obj)
{
	QUERY_INTERFACE (_iid, obj, FUnknown::iid, Vst::IComponentHandler)
	QUERY_INTERFACE (_iid, obj, Vst::IComponentHandler::iid, Vst::IComponentHandler)
	QUERY_INTERFACE (_iid, obj, Vst::IComponentHandler2::iid, Vst::IComponentHandler2)

	QUERY_INTERFACE (_iid, obj, Vst::IUnitHandler::iid, Vst::IUnitHandler)

	QUERY_INTERFACE (_iid, obj, Presonus::IContextInfoProvider::iid, Presonus::IContextInfoProvider)
	QUERY_INTERFACE (_iid, obj, Presonus::IContextInfoProvider2::iid, Presonus::IContextInfoProvider2)
	QUERY_INTERFACE (_iid, obj, Presonus::IContextInfoProvider3::iid, Presonus::IContextInfoProvider3)

	QUERY_INTERFACE (_iid, obj, IPlugFrame::iid, IPlugFrame)

#if SMTG_OS_LINUX
	if (_run_loop && FUnknownPrivate::iidEqual (_iid, Linux::IRunLoop::iid)) {
		*obj = _run_loop;
		return kResultOk;
	}
#endif

	if (DEBUG_ENABLED (DEBUG::VST3Config)) {
		char fuid[33];
		FUID::fromTUID (_iid).toString (fuid);
		DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::queryInterface not supported: %1\n", fuid));
	}

	*obj = nullptr;
	return kNoInterface;
}

tresult
VST3PI::restartComponent (int32 flags)
{
	DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::restartComponent %1%2\n", std::hex, flags));

	if (flags & Vst::kReloadComponent) {
		/* according to the spec, "The host has to unload completely
		 * the plug-in (controller/processor) and reload it."
		 *
		 * However other implementations, in particular JUCE, only
		 * re-activates the plugin. So let's follow their lead for
		 * the time being.
		 */
		warning << "VST3: Vst::kReloadComponent (ignored)" << endmsg;
		deactivate ();
		activate ();
	}
	if (flags & Vst::kParamValuesChanged) {
		update_shadow_data ();
	}
	if (flags & Vst::kLatencyChanged) {
		/* need to re-activate the plugin as per spec */
		deactivate ();
		activate ();
	}
	if (flags & Vst::kIoChanged) {
		warning << "VST3: Vst::kIoChanged (not implemented)" << endmsg;
#if 0
		update_processor ();
		// TODO getBusArrangement(); enable_io()
#endif
		return kNotImplemented;
	}
	return kResultOk;
}

tresult
VST3PI::notifyUnitSelection (Vst::UnitID unitId)
{
	return kResultFalse;
}

tresult
VST3PI::notifyProgramListChange (Vst::ProgramListID, int32)
{
	float        v  = 0;
	Vst::ParamID id = _program_change_port.id;
	if (id != Vst::kNoParamId) {
		v = _controller->getParamNormalized (id);
		DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::notifyProgramListChange: val: %1 (norm: %2)\n", v, _controller->normalizedParamToPlain (id, v)));
	}
	OnParameterChange (PresetChange, 0, v); /* EMIT SIGNAL */
	return kResultOk;
}

tresult
VST3PI::performEdit (Vst::ParamID id, Vst::ParamValue v)
{
	std::map<Vst::ParamID, uint32_t>::const_iterator idx = _ctrl_id_index.find (id);
	if (idx != _ctrl_id_index.end ()) {
		float value               = v;
		_shadow_data[idx->second] = value;
		_update_ctrl[idx->second] = true;
		set_parameter_internal (id, value, 0, true);
		value = _controller->normalizedParamToPlain (id, value);
		OnParameterChange (ValueChange, idx->second, v); /* EMIT SIGNAL */
	}
	return kResultOk;
}

tresult
VST3PI::beginEdit (Vst::ParamID id)
{
	std::map<Vst::ParamID, uint32_t>::const_iterator idx = _ctrl_id_index.find (id);
	if (idx != _ctrl_id_index.end ()) {
		OnParameterChange (BeginGesture, idx->second, 0); /* EMIT SIGNAL */
	}
	return kResultOk;
}

tresult
VST3PI::endEdit (Vst::ParamID id)
{
	std::map<Vst::ParamID, uint32_t>::const_iterator idx = _ctrl_id_index.find (id);
	if (idx != _ctrl_id_index.end ()) {
		OnParameterChange (EndGesture, idx->second, 0); /* EMIT SIGNAL */
	}
	return kResultOk;
}

tresult
VST3PI::setDirty (TBool state)
{
	if (state) {
		OnParameterChange (InternalChange, 0, 0); /* EMIT SIGNAL */
	}
	return kResultOk;
}

tresult
VST3PI::requestOpenEditor (FIDString name)
{
	if (name == Vst::ViewType::kEditor) {
		/* TODO get plugin-insert (first plugin only, not replicated ones)
		 * call pi->ShowUI ();
		 */
	}
	return kNotImplemented;
}

tresult
VST3PI::startGroupEdit ()
{
	/* TODO:
	 * remember current time, update StartTouch API
	 * to allow passing a timestamp to PluginInsert::start_touch
	 * replacing  .audible_sample()
	 */
	return kNotImplemented;
}

tresult
VST3PI::finishGroupEdit ()
{
	return kNotImplemented;
}

bool
VST3PI::deactivate ()
{
	if (!_is_processing) {
		return true;
	}

	tresult res = _processor->setProcessing (false);
	if (!(res == kResultOk || res == kNotImplemented)) {
		return false;
	}

	res = _component->setActive (false);
	if (!(res == kResultOk || res == kNotImplemented)) {
		return false;
	}

	_is_processing = false;
	return true;
}

bool
VST3PI::activate ()
{
	if (_is_processing) {
		return true;
	}

	tresult res = _component->setActive (true);
	if (!(res == kResultOk || res == kNotImplemented)) {
		return false;
	}

	res = _processor->setProcessing (true);
	if (!(res == kResultOk || res == kNotImplemented)) {
		return false;
	}

	_plugin_latency.reset ();
	_is_processing = true;
	return true;
}

bool
VST3PI::update_processor ()
{
	bool was_active = _is_processing;

	if (!deactivate ()) {
		return false;
	}

	Vst::ProcessSetup setup;
	setup.processMode        = AudioEngine::instance ()->freewheeling () ? Vst::kOffline : Vst::kRealtime;
	setup.symbolicSampleSize = Vst::kSample32;
	setup.maxSamplesPerBlock = _block_size;
	setup.sampleRate         = _context.sampleRate;

	if (_processor->setupProcessing (setup) != kResultOk) {
		return false;
	}

	if (was_active) {
		return activate ();
	}
	return true;
}

uint32_t
VST3PI::plugin_latency ()
{
	if (!_plugin_latency) {
		_plugin_latency = _processor->getLatencySamples ();
	}
	return _plugin_latency.value ();
}

void
VST3PI::set_owner (SessionObject* o)
{
	_owner = o;
	if (!o) {
		_strip_connections.drop_connections ();
		_ac_connection_list.drop_connections ();
		_ac_subscriptions.clear ();
		return;
	}

	if (!setup_psl_info_handler ()) {
		setup_info_listener ();
	}
}

int32
VST3PI::count_channels (Vst::MediaType media, Vst::BusDirection dir, Vst::BusType type)
{
	/* see also libs/ardour/vst3_scan.cc count_channels */
	int32 n_busses   = _component->getBusCount (media, dir);
	int32 n_channels = 0;
	for (int32 i = 0; i < n_busses; ++i) {
		Vst::BusInfo bus;
		if (_component->getBusInfo (media, dir, i, bus) == kResultTrue && bus.busType == type) {
#if 1
			if ((type == Vst::kMain && i != 0) || (type == Vst::kAux && i != 1)) {
				/* For now allow we only support one main bus, and one aux-bus.
				 * Also an aux-bus by itself is currently N/A.
				 */
				continue;
			}
#endif

			std::string bus_name     = tchar_to_utf8 (bus.name);
			bool        is_sidechain = (type == Vst::kAux) && (dir == Vst::kInput);

			if (media == Vst::kEvent) {
#if 0
				/* Supported MIDI Channel count (for a single MIDI input) */
				if (bus.channelCount > 0) {
					_io_name[media][dir].push_back (Plugin::IOPortDescription (bus_name, is_sidechain));
				}
				return std::min<int32> (1, bus.channelCount);
#else
				/* Some plugin leave it at zero, even though they accept events */
				_io_name[media][dir].push_back (Plugin::IOPortDescription (bus_name, is_sidechain));
				return 1;
#endif
			} else {
				for (int32_t j = 0; j < bus.channelCount; ++j) {
					std::string channel_name;
					if (bus.channelCount > 1) {
						channel_name = string_compose ("%1 %2", bus_name, j + 1);
					} else {
						channel_name = bus_name;
					}
					_io_name[media][dir].push_back (Plugin::IOPortDescription (channel_name, is_sidechain, bus_name, j));
				}
				n_channels += bus.channelCount;
			}
		}
	}
	return n_channels;
}

Vst::ParamID
VST3PI::index_to_id (uint32_t p) const
{
	assert (_ctrl_index_id.find (p) != _ctrl_index_id.end ());
	return (_ctrl_index_id.find (p))->second;
}

bool
VST3PI::set_block_size (int32_t n_samples)
{
	if (_block_size == n_samples) {
		return true;
	}
	_block_size = n_samples;
	return update_processor ();
}

float
VST3PI::default_value (uint32_t port) const
{
	Vst::ParamID id (index_to_id (port));
	return _controller->normalizedParamToPlain (id, _ctrl_params[port].normal);
}

void
VST3PI::get_parameter_descriptor (uint32_t port, ParameterDescriptor& desc) const
{
	Param const& p (_ctrl_params[port]);
	Vst::ParamID id (index_to_id (port));

	desc.lower        = _controller->normalizedParamToPlain (id, 0.f);
	desc.upper        = _controller->normalizedParamToPlain (id, 1.f);
	desc.normal       = _controller->normalizedParamToPlain (id, p.normal);
	desc.toggled      = 1 == p.steps;
	desc.logarithmic  = false;
	desc.integer_step = p.steps > 1 && (desc.upper - desc.lower) == p.steps;
	desc.sr_dependent = false;
	desc.enumeration  = p.is_enum;
	desc.label        = p.label;
	if (p.unit == "dB") {
		desc.unit = ARDOUR::ParameterDescriptor::DB;
	} else if (p.unit == "Hz") {
		desc.unit = ARDOUR::ParameterDescriptor::HZ;
	}
	if (p.steps > 1) {
		desc.rangesteps = 1 + p.steps;
	}

	FUnknownPtr<IEditControllerExtra> extra_ctrl (_controller);
	if (extra_ctrl && port != designated_bypass_port ()) {
		int32 flags      = extra_ctrl->getParamExtraFlags (id);
		if (Config->get_show_vst3_micro_edit_inline ()) {
			desc.inline_ctrl = (flags & kParamFlagMicroEdit) ? true : false;
		}
	}
}

std::string
VST3PI::print_parameter (uint32_t port) const
{
	Vst::ParamID id (index_to_id (port));
	return print_parameter (id, _shadow_data[port]);
}

std::string
VST3PI::print_parameter (Vst::ParamID id, Vst::ParamValue value) const
{
	Vst::String128 rv;
	if (_controller->getParamStringByValue (id, value, rv) == kResultOk) {
		return tchar_to_utf8 (rv);
	}
	return "";
}

uint32_t
VST3PI::n_audio_inputs () const
{
	return _n_inputs + _n_aux_inputs;
}

uint32_t
VST3PI::n_audio_outputs () const
{
	return _n_outputs + _n_aux_outputs;
}

uint32_t
VST3PI::n_midi_inputs () const
{
	return _n_midi_inputs;
}

uint32_t
VST3PI::n_midi_outputs () const
{
	return _n_midi_outputs;
}

Plugin::IOPortDescription
VST3PI::describe_io_port (ARDOUR::DataType dt, bool input, uint32_t id) const
{
	switch (dt) {
		case DataType::AUDIO:
			return _io_name[Vst::kAudio][input ? 0 : 1][id];
			break;
		case DataType::MIDI:
			return _io_name[Vst::kEvent][input ? 0 : 1][id];
			break;
		default:
			return Plugin::IOPortDescription ("?");
			break;
	}
}

bool
VST3PI::try_set_parameter_by_id (Vst::ParamID id, float value)
{
	std::map<Vst::ParamID, uint32_t>::const_iterator idx = _ctrl_id_index.find (id);
	if (idx == _ctrl_id_index.end ()) {
		return false;
	}
	set_parameter (idx->second, value, 0);
	return true;
}

void
VST3PI::set_parameter (uint32_t p, float value, int32 sample_off)
{
	set_parameter_internal (index_to_id (p), value, sample_off, false);
	_shadow_data[p] = value;
	_update_ctrl[p] = true;
}

bool
VST3PI::set_program (int pgm, int32 sample_off)
{
	if (_program_change_port.id == Vst::kNoParamId) {
		return false;
	}
	if (_n_factory_presets < 1) {
		return false;
	}

	if (pgm < 0 || pgm >= _n_factory_presets) {
		return false;
	}

	Vst::ParamID id = _program_change_port.id;
#if 0
	/* This fails with some plugins (e.g. waves.vst3),
	 * that do not use integer indexed presets.
	 */
	float value = _controller->plainParamToNormalized (id, pgm);
#else
	float value = pgm;
	if (_n_factory_presets > 1) {
		value /= (_n_factory_presets - 1.f);
	}
#endif
	DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::set_program pgm: %1 val: %2 (norm: %3)\n", pgm, value, _controller->plainParamToNormalized (id, pgm)));

	int32 index;
	_input_param_changes.addParameterData (id, index)->addPoint (sample_off, value, index);
	_controller->setParamNormalized (id, value);

#if 0
	update_shadow_data ();
	synchronize_states ();
#endif
	return true;
}

bool
VST3PI::synchronize_states ()
{
	RAMStream stream;
	if (_component->getState (&stream) == kResultTrue) {
		stream.rewind ();
		tresult res = _controller->setComponentState (&stream);
		if (!(res == kResultOk || res == kNotImplemented)) {
#ifndef NDEBUG
			std::cerr << "Failed to synchronize VST3 component <> controller state\n";
			stream.hexdump (0);
#endif
		}
		return res == kResultOk;
	}
	return false;
}

void
VST3PI::update_shadow_data ()
{
	std::map<uint32_t, Vst::ParamID>::const_iterator i;
	for (i = _ctrl_index_id.begin (); i != _ctrl_index_id.end (); ++i) {
		Vst::ParamValue v = _controller->getParamNormalized (i->second);
		if (_shadow_data[i->first] != v) {
#if 0 // DEBUG
			printf ("VST3PI::update_shadow_data %d: %f -> %f\n", i->first,
					_shadow_data[i->first], _controller->getParamNormalized (i->second));
#endif
#if 1 // needed for set_program() changes to take effect, after kParamValuesChanged
			int32 index;
			_input_param_changes.addParameterData (i->second, index)->addPoint (0, v, index);
#endif
			_shadow_data[i->first] = v;
		}
	}
}

void
VST3PI::update_contoller_param ()
{
	/* GUI thread */
	FUnknownPtr<Vst::IEditControllerHostEditing> host_editing (_controller);

	std::map<uint32_t, Vst::ParamID>::const_iterator i;
	for (i = _ctrl_index_id.begin (); i != _ctrl_index_id.end (); ++i) {
		if (!_update_ctrl[i->first]) {
			continue;
		}
		_update_ctrl[i->first] = false;
		if (!parameter_is_automatable (i->first) && !parameter_is_readonly (i->first)) {
			assert (host_editing);
			host_editing->beginEditFromHost (i->second);
		}
		_controller->setParamNormalized (i->second, _shadow_data[i->first]);
		if (!parameter_is_automatable (i->first) && !parameter_is_readonly (i->first)) {
			host_editing->endEditFromHost (i->second);
		}
	}
}

void
VST3PI::set_parameter_by_id (Vst::ParamID id, float value, int32 sample_off)
{
	set_parameter_internal (id, value, sample_off, true);
	std::map<Vst::ParamID, uint32_t>::const_iterator idx = _ctrl_id_index.find (id);
	if (idx != _ctrl_id_index.end ()) {
		_shadow_data[idx->second] = value;
		_update_ctrl[idx->second] = true;
	}
}

void
VST3PI::set_parameter_internal (Vst::ParamID id, float& value, int32 sample_off, bool normalized)
{
	int32 index;
	if (!normalized) {
		value = _controller->plainParamToNormalized (id, value);
	}
	_input_param_changes.addParameterData (id, index)->addPoint (sample_off, value, index);
}

float
VST3PI::get_parameter (uint32_t p) const
{
	Vst::ParamID id = index_to_id (p);
	if (_update_ctrl[p]) {
		_update_ctrl[p] = false;

		FUnknownPtr<Vst::IEditControllerHostEditing> host_editing (_controller);
		if (!parameter_is_automatable (p) && !parameter_is_readonly (p)) {
			assert (host_editing);
			host_editing->beginEditFromHost (id);
		}
		_controller->setParamNormalized (id, _shadow_data[p]); // GUI thread only
		if (!parameter_is_automatable (p) && !parameter_is_readonly (p)) {
			host_editing->endEditFromHost (id);
		}
	}
	return _controller->normalizedParamToPlain (id, _shadow_data[p]);
}

bool
VST3PI::live_midi_cc (int32_t bus, int16_t channel, Vst::CtrlNumber ctrl)
{
	FUnknownPtr<Vst::IMidiLearn> midiLearn (_controller);
	if (!midiLearn) {
		return false;
	}
	return kResultOk == midiLearn->onLiveMIDIControllerInput (bus, channel, ctrl);
}

bool
VST3PI::midi_controller (int32_t bus, int16_t channel, Vst::CtrlNumber ctrl, Vst::ParamID& id)
{
	FUnknownPtr<Vst::IMidiMapping> midiMapping (_controller);
	if (!midiMapping) {
		return false;
	}
	return kResultOk == midiMapping->getMidiControllerAssignment (bus, channel, ctrl, id);
}

void
VST3PI::cycle_start ()
{
	_input_events.clear ();
	_output_events.clear ();
}

void
VST3PI::add_event (Evoral::Event<samplepos_t> const& ev, int32_t bus)
{
	Vst::Event e;
	e.busIndex     = bus;
	e.flags        = ev.is_live_midi () ? Vst::Event::kIsLive : 0;
	e.sampleOffset = ev.time ();
	e.ppqPosition  = _context.projectTimeMusic;
	if (evoral_to_vst3 (e, ev, bus)) {
		_input_events.addEvent (e);
	}
}

void
VST3PI::set_event_bus_state (bool enable)
{
	int32 n_bus_in  = _component->getBusCount (Vst::kEvent, Vst::kInput);
	int32 n_bus_out = _component->getBusCount (Vst::kEvent, Vst::kOutput);

	DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::set_event_bus_state: n_bus_in = %1 n_bus_in = %2 enable = %3\n", n_bus_in, n_bus_out, enable));

	for (int32 i = 0; i < n_bus_in; ++i) {
		_component->activateBus (Vst::kEvent, Vst::kInput, i, enable);
	}
	for (int32 i = 0; i < n_bus_out; ++i) {
		_component->activateBus (Vst::kEvent, Vst::kOutput, i, enable);
	}
}

void
VST3PI::enable_io (std::vector<bool> const& ins, std::vector<bool> const& outs)
{
	if (_enabled_audio_in == ins && _enabled_audio_out == outs) {
		return;
	}

	DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::enable_io: ins = %1 == %3 outs = %2 == %4\n", ins.size (), outs.size (), n_audio_inputs (), n_audio_outputs ()));

	_enabled_audio_in  = ins;
	_enabled_audio_out = outs;

	assert (_enabled_audio_in.size () == n_audio_inputs ());
	assert (_enabled_audio_out.size () == n_audio_outputs ());
	/* check that settings have not changed */
	assert (_n_bus_in == _component->getBusCount (Vst::kAudio, Vst::kInput));
	assert (_n_bus_out == _component->getBusCount (Vst::kAudio, Vst::kOutput));

	DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::enable_io: n_bus_in = %1 n_bus_in = %2\n", _n_bus_in, _n_bus_out));

	std::vector<Vst::SpeakerArrangement> sa_in;
	std::vector<Vst::SpeakerArrangement> sa_out;

	bool                    enable = false;
	Vst::SpeakerArrangement sa     = 0;

	for (int i = 0; i < _n_inputs; ++i) {
		if (ins[i]) {
			enable = true;
		}
		sa |= (uint64_t)1 << i;
	}
	if (_n_inputs > 0) {
		DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::enable_io: activateBus (kAudio, kInput, 0, %1)\n", enable));
		_component->activateBus (Vst::kAudio, Vst::kInput, 0, enable);
		sa_in.push_back (sa);
	}

	enable = false;
	sa     = 0;
	for (int i = 0; i < _n_aux_inputs; ++i) {
		if (ins[i + _n_inputs]) {
			enable = true;
		}
		sa |= (uint64_t)1 << i;
	}
	if (_n_aux_inputs > 0) {
		DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::enable_io: activateBus (kAudio, kInput, 1, %1)\n", enable));
		_component->activateBus (Vst::kAudio, Vst::kInput, 1, enable);
		sa_in.push_back (sa);
	}

	/* disable remaining input busses and set their speaker-count to zero */
	while (sa_in.size () < _n_bus_in) {
		DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::enable_io: activateBus (kAudio, kInput, %1, false)\n", sa_in.size ()));
		_component->activateBus (Vst::kAudio, Vst::kInput, sa_in.size (), false);
		sa_in.push_back (0);
	}

	enable = false;
	sa     = 0;
	for (int i = 0; i < _n_outputs; ++i) {
		if (outs[i]) {
			enable = true;
		}
		sa |= (uint64_t)1 << i;
	}

	if (_n_outputs > 0) {
		DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::enable_io: activateBus (kAudio, kOutput, 0, %1)\n", enable));
		_component->activateBus (Vst::kAudio, Vst::kOutput, 0, enable);
		sa_out.push_back (sa);
	}

	enable = false;
	sa     = 0;
	for (int i = 0; i < _n_aux_outputs; ++i) {
		if (outs[i + _n_outputs]) {
			enable = true;
		}
		sa |= (uint64_t)1 << i;
	}
	if (_n_aux_outputs > 0) {
		DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::enable_io: activateBus (kAudio, kOutput, 1, %1)\n", enable));
		_component->activateBus (Vst::kAudio, Vst::kOutput, 1, enable);
		sa_out.push_back (sa);
	}

	while (sa_out.size () < _n_bus_out) {
		DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::enable_io: activateBus (kAudio, kOutput, %1, false)\n", sa_out.size ()));
		_component->activateBus (Vst::kAudio, Vst::kOutput, sa_out.size (), false);
		sa_out.push_back (0);
	}

	DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::enable_io: setBusArrangements ins = %1 outs = %2\n", sa_in.size (), sa_out.size ()));
	_processor->setBusArrangements (sa_in.size () > 0 ? &sa_in[0] : NULL, sa_in.size (),
	                                sa_out.size () > 0 ? &sa_out[0] : NULL, sa_out.size ());

#if 0
	for (int32 i = 0; i < _n_bus_in; ++i) {
		Vst::SpeakerArrangement arr;
		if (_processor->getBusArrangement (Vst::kInput, i, arr) == kResultOk) {
			int cc = Vst::SpeakerArr::getChannelCount (arr);
			std::cerr << "VST3: Input BusArrangements: " << i << " chan: " << cc << " bits: " << arr << "\n";
		}
	}
	for (int32 i = 0; i < _n_bus_out; ++i) {
		Vst::SpeakerArrangement arr;
		if (_processor->getBusArrangement (Vst::kOutput, i, arr) == kResultOk) {
			int cc = Vst::SpeakerArr::getChannelCount (arr);
			std::cerr << "VST3: Output BusArrangements: " << i << " chan: " << cc << " bits: " << arr << "\n";
		}
	}
#endif
}

static int32
used_bus_count (int auxes, int inputs)
{
	if (auxes > 0 && inputs > 0) {
		return 2;
	}
	if (auxes == 0 && inputs == 0) {
		return 0;
	}
	return 1;
}

void
VST3PI::process (float** ins, float** outs, uint32_t n_samples)
{
	Vst::AudioBusBuffers* inputs  = _n_bus_in > 0 ? &_busbuf_in[0] : NULL;
	Vst::AudioBusBuffers* outputs = _n_bus_out > 0 ? &_busbuf_out[0] : NULL;

	Vst::ProcessData data;
	data.numSamples         = n_samples;
	data.processMode        = AudioEngine::instance ()->freewheeling () ? Vst::kOffline : Vst::kRealtime;
	data.symbolicSampleSize = Vst::kSample32;
	data.numInputs          = used_bus_count (_n_aux_inputs, _n_inputs); // _n_bus_in;
	data.numOutputs         = used_bus_count (_n_aux_outputs, _n_outputs); // _n_bus_out;
	data.inputs             = inputs;
	data.outputs            = outputs;

	data.processContext = &_context;
	data.inputEvents    = &_input_events;
	data.outputEvents   = &_output_events;

	data.inputParameterChanges  = &_input_param_changes;
	data.outputParameterChanges = &_output_param_changes;

	int used_ins = 0;
	int used_outs = 0;

	if (_n_bus_in > 0) {
		inputs[0].silenceFlags     = 0;
		inputs[0].numChannels      = _n_inputs;
		inputs[0].channelBuffers32 = ins;
		++used_ins;
	}

	if (_n_bus_in > 1 && _n_aux_inputs > 0) {
		inputs[1].silenceFlags     = 0;
		inputs[1].numChannels      = _n_aux_inputs;
		inputs[1].channelBuffers32 = &ins[_n_inputs];
		++used_ins;
	}

	if (_n_bus_out > 0) {
		outputs[0].silenceFlags     = 0;
		outputs[0].numChannels      = _n_outputs;
		outputs[0].channelBuffers32 = outs;
		++used_outs;
	}

	if (_n_bus_out > 1 && _n_aux_outputs > 0) {
		outputs[1].silenceFlags     = 0;
		outputs[1].numChannels      = _n_outputs;
		outputs[1].channelBuffers32 = &outs[_n_outputs];
		++used_outs;
	}

	for (int i = used_ins; i < _n_bus_in; ++i) {
		inputs[i].silenceFlags     = 0;
		inputs[i].numChannels      = 0;
		inputs[i].channelBuffers32 = 0;
	}

	for (int i = used_outs; i < _n_bus_out; ++i) {
		outputs[i].silenceFlags     = 0;
		outputs[i].numChannels      = 0;
		outputs[i].channelBuffers32 = 0;
	}

	/* and go */
	if (_processor->process (data) != kResultOk) {
		DEBUG_TRACE (DEBUG::VST3Process, "VST3 process error\n");
	}

	/* handle output parameter changes */
	int n_changes = _output_param_changes.getParameterCount ();
	for (int i = 0; i < n_changes; ++i) {
		Vst::IParamValueQueue* data = _output_param_changes.getParameterData (i);
		if (!data) {
			continue;
		}
		Vst::ParamID id       = data->getParameterId ();
		int          n_points = data->getPointCount ();

		if (n_points == 0) {
			continue;
		}

		std::map<Vst::ParamID, uint32_t>::const_iterator idx = _ctrl_id_index.find (id);
		if (idx != _ctrl_id_index.end ()) {
			/* automatable parameter, or read-only output */
			int32           offset = 0;
			Vst::ParamValue value  = 0;
			/* only get most recent point */
			if (data->getPoint (n_points - 1, offset, value) == kResultOk) {
				if (_shadow_data[idx->second] != value) {
					_update_ctrl[idx->second] = true;
					_shadow_data[idx->second] = (float)value;
				}
			}
		} else {
#ifndef NDEBUG
			/* non-automatable parameter */
			std::cerr << "VST3: TODO non-automatable output param..\n"; // TODO inform UI
#endif
		}
	}

	_input_param_changes.clear ();
	_output_param_changes.clear ();
}

/* ****************************************************************************
 * State
 * compare to public.sdk/source/vst/vstpresetfile.cpp
 */

namespace Steinberg {
namespace Vst {

enum ChunkType {
	kHeader,
	kComponentState,
	kControllerState,
	kProgramData,
	kMetaInfo,
	kChunkList,
	kNumPresetChunks
};

static const ChunkID commonChunks[kNumPresetChunks] = {
	{ 'V', 'S', 'T', '3' }, // kHeader
	{ 'C', 'o', 'm', 'p' }, // kComponentState
	{ 'C', 'o', 'n', 't' }, // kControllerState
	{ 'P', 'r', 'o', 'g' }, // kProgramData
	{ 'I', 'n', 'f', 'o' }, // kMetaInfo
	{ 'L', 'i', 's', 't' }  // kChunkList
};

static const int32 kFormatVersion = 1;

static const ChunkID&
getChunkID (ChunkType type)
{
	return commonChunks[type];
}

struct ChunkEntry {
	void start_chunk (const ChunkID& id, RAMStream& stream)
	{
		memcpy (_id, &id, sizeof (ChunkID));
		stream.tell (&_offset);
		_size = 0;
	}
	void end_chunk (RAMStream& stream)
	{
		int64 pos = 0;
		stream.tell (&pos);
		_size = pos - _offset;
	}

	ChunkID _id;
	int64   _offset;
	int64   _size;
};

} // namespace Vst

typedef std::vector<Vst::ChunkEntry> ChunkEntryVector;

} // namespace Steinberg

static bool
is_equal_ID (const Vst::ChunkID id1, const Vst::ChunkID id2)
{
	return 0 == memcmp (id1, id2, sizeof (Vst::ChunkID));
}

static bool
read_equal_ID (RAMStream& stream, const Vst::ChunkID id)
{
	Vst::ChunkID tmp;
	return stream.read_ChunkID (tmp) && is_equal_ID (tmp, id);
}

bool
VST3PI::load_state (RAMStream& stream)
{
	assert (stream.readonly ());
	if (stream.size () < Vst::kHeaderSize) {
		return false;
	}

	int32 version     = 0;
	int64 list_offset = 0;
	TUID  class_id;

	if (!(read_equal_ID (stream, Vst::getChunkID (Vst::kHeader))
	      && stream.read_int32 (version)
	      && stream.read_TUID (class_id)
	      && stream.read_int64 (list_offset)
	      && list_offset > 0
	     )
	   ) {
		DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::load_state: invalid header vers: %1 off: %2\n", version, list_offset));
		return false;
	}

	if (_fuid != FUID::fromTUID (class_id)) {
		DEBUG_TRACE (DEBUG::VST3Config, "VST3PI::load_state: class ID mismatch\n");
		return false;
	}

	/* read chunklist */
	ChunkEntryVector entries;
	int64            seek_result = 0;
	stream.seek (list_offset, IBStream::kIBSeekSet, &seek_result);
	if (seek_result != list_offset) {
		return false;
	}
	if (!read_equal_ID (stream, Vst::getChunkID (Vst::kChunkList))) {
		return false;
	}
	int32 count;
	stream.read_int32 (count);
	for (int32 i = 0; i < count; ++i) {
		Vst::ChunkEntry c;
		stream.read_ChunkID (c._id);
		stream.read_int64 (c._offset);
		stream.read_int64 (c._size);
		entries.push_back (c);
		DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::load_state: chunk: %1 off: %2 size: %3 type: %4\n", i, c._offset, c._size, c._id));
	}

	bool rv = true;
	bool synced = false;

	/* parse chunks */
	for (ChunkEntryVector::const_iterator i = entries.begin (); i != entries.end (); ++i) {
		stream.seek (i->_offset, IBStream::kIBSeekSet, &seek_result);
		if (seek_result != i->_offset) {
			rv = false;
			continue;
		}
		if (is_equal_ID (i->_id, Vst::getChunkID (Vst::kComponentState))) {
			ROMStream s (stream, i->_offset, i->_size);
			tresult   res = _component->setState (&s);

			s.rewind ();
			tresult re2 = _controller->setComponentState (&s);

			if (re2 == kResultOk) {
				synced = true;
			}

			if (!(re2 == kResultOk || re2 == kNotImplemented || res == kResultOk || res == kNotImplemented)) {
				DEBUG_TRACE (DEBUG::VST3Config, "VST3PI::load_state: failed to restore component state\n");
				rv = false;
			}
		} else if (is_equal_ID (i->_id, Vst::getChunkID (Vst::kControllerState))) {
			ROMStream s (stream, i->_offset, i->_size);
			tresult res = _controller->setState (&s);
			if (res == kResultOk) {
				synced = true;
			}

			if (!(res == kResultOk || res == kNotImplemented)) {
				DEBUG_TRACE (DEBUG::VST3Config, "VST3PI::load_state: failed to restore controller state\n");
				rv = false;
			}
		}
#if 0
		else if (is_equal_ID (i->_id, Vst::getChunkID (Vst::kProgramData))) {
			Vst::IUnitInfo* unitInfo = unit_info ();
			stream.seek (i->_offset, IBStream::kIBSeekSet, &seek_result);
			int32 id = -1;
			if (stream.read_int32 (id)) {
				ROMStream s (stream, i->_offset + sizeof (int32), i->_size - sizeof (int32));
				unit_info->setUnitProgramData (id, programIndex, s);
				//unit_data->setUnitData (id, programIndex, s)
			}
		}
#endif
		else {
			DEBUG_TRACE (DEBUG::VST3Config, "VST3PI::load_state: ignored unsupported state chunk.\n");
		}
	}
	if (rv && !synced) {
		synced = synchronize_states ();
	}

	if (rv && synced) {
		update_shadow_data ();
	}
	return rv;
}

bool
VST3PI::save_state (RAMStream& stream)
{
	assert (!stream.readonly ());
	Vst::ChunkEntry  c;
	ChunkEntryVector entries;

	/* header */
	stream.write_ChunkID (Vst::getChunkID (Vst::kHeader));
	stream.write_int32 (Vst::kFormatVersion);
	stream.write_TUID (_fuid.toTUID ()); // class ID
	stream.write_int64 (0);              // skip offset

	/* state chunks */
	c.start_chunk (getChunkID (Vst::kComponentState), stream);
	if (_component->getState (&stream) == kResultTrue) {
		c.end_chunk (stream);
		entries.push_back (c);
	}

	c.start_chunk (getChunkID (Vst::kControllerState), stream);
	if (_controller->getState (&stream) == kResultTrue) {
		c.end_chunk (stream);
		entries.push_back (c);
	}

	/* update header */
	int64 pos;
	stream.tell (&pos);
	stream.seek (Vst::kListOffsetPos, IBStream::kIBSeekSet, NULL);
	stream.write_int64 (pos);
	stream.seek (pos, IBStream::kIBSeekSet, NULL);

	/* write list */
	stream.write_ChunkID (Vst::getChunkID (Vst::kChunkList));
	stream.write_int32 (entries.size ());

	for (ChunkEntryVector::const_iterator i = entries.begin (); i != entries.end (); ++i) {
		stream.write_ChunkID (i->_id);
		stream.write_int64 (i->_offset);
		stream.write_int64 (i->_size);
	}

	return entries.size () > 0;
}

/* ****************************************************************************/

void
VST3PI::stripable_property_changed (PBD::PropertyChange const&)
{
	FUnknownPtr<Vst::ChannelContext::IInfoListener> il (_controller);
	Stripable*                                      s = dynamic_cast<Stripable*> (_owner);
	assert (il && s);

	DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::stripable_property_changed\n");

	IPtr<HostAttributeList> al (new HostAttributeList ());

	Vst::String128 tmp;
	utf8_to_tchar (tmp, _owner->name (), 128);
	al->setInt (Vst::ChannelContext::kChannelNameLengthKey, _owner->name ().size ());
	al->setString (Vst::ChannelContext::kChannelNameKey, tmp);

	utf8_to_tchar (tmp, _owner->id ().to_s (), 128);
	al->setInt (Vst::ChannelContext::kChannelNameLengthKey, _owner->id ().to_s ().size ());
	al->setString (Vst::ChannelContext::kChannelUIDKey, tmp);

	std::string ns;
	int order_key;
	if (s->is_master ()) {
		ns = _("Master");
		order_key = 2;
	} else if (s->is_monitor ()) {
		ns = _("Monitor");
		order_key = 3;
	} else {
		ns = _("Track");
		order_key = 1;
	}

	al->setInt (Vst::ChannelContext::kChannelIndexNamespaceOrderKey, order_key);
	al->setInt (Vst::ChannelContext::kChannelIndexKey, 1 + s->presentation_info ().order ());

	utf8_to_tchar (tmp, ns, 128);
	al->setInt (Vst::ChannelContext::kChannelIndexNamespaceLengthKey, ns.size ());
	al->setString (Vst::ChannelContext::kChannelIndexNamespaceKey, tmp);

	uint32_t rgba = s->presentation_info ().color ();
	Vst::ChannelContext::ColorSpec argb = ((rgba >> 8) & 0xffffff) | ((rgba & 0xff) << 24);
	al->setInt (Vst::ChannelContext::kChannelColorKey, argb);

	al->setInt (Vst::ChannelContext::kChannelPluginLocationKey, Vst::ChannelContext::kPreVolumeFader); // XXX

	il->setChannelContextInfos (al);
}

bool
VST3PI::setup_info_listener ()
{
	FUnknownPtr<Vst::ChannelContext::IInfoListener> il (_controller);
	if (!il) {
		return false;
	}
	DEBUG_TRACE (DEBUG::VST3Config, "VST3PI::setup_info_listener\n");
	Stripable* s = dynamic_cast<Stripable*> (_owner);

	s->PropertyChanged.connect_same_thread (_strip_connections, boost::bind (&VST3PI::stripable_property_changed, this, _1));
	s->presentation_info ().PropertyChanged.connect_same_thread (_strip_connections, boost::bind (&VST3PI::stripable_property_changed, this, _1));

	/* send initial change */
	stripable_property_changed (PropertyChange ());
	return true;
}

/* ****************************************************************************
 * PSL Extensions
 */

bool
VST3PI::add_slave (Vst::IEditController* c, bool rt)
{
	FUnknownPtr<ISlaveControllerHandler> slave_ctrl (_controller);
	if (slave_ctrl) {
		return slave_ctrl->addSlave (c, rt ? kSlaveModeLowLatencyClone : kSlaveModeNormal) == kResultOk;
	}
	return false;
}

bool
VST3PI::remove_slave (Vst::IEditController* c)
{
	FUnknownPtr<ISlaveControllerHandler> slave_ctrl (_controller);
	if (slave_ctrl) {
		return slave_ctrl->removeSlave (c) == kResultOk;
	}
	return false;
}

bool
VST3PI::subscribe_to_automation_changes () const
{
	FUnknownPtr<IEditControllerExtra> extra_ctrl (_controller);
	return 0 != extra_ctrl ? true : false;
}

void
VST3PI::automation_state_changed (uint32_t port, AutoState s, boost::weak_ptr<AutomationList> wal)
{
	Vst::ParamID                      id (index_to_id (port));
	boost::shared_ptr<AutomationList> al = wal.lock ();
	FUnknownPtr<IEditControllerExtra> extra_ctrl (_controller);
	assert (extra_ctrl);

	AutomationMode am;
	switch (s) {
		case ARDOUR::Off:
			if (!al || al->empty ()) {
				am = kAutomationNone;
			} else {
				am = kAutomationOff;
			}
			break;
		case Write:
			am = kAutomationWrite;
			break;
		case Touch:
			am = kAutomationTouch;
			break;
		case Play:
			am = kAutomationRead;
			break;
		case Latch:
			am = kAutomationLatch;
			break;
		default:
			assert (0);
	}
	extra_ctrl->setParamAutomationMode (id, am);
}

/* ****************************************************************************/

static boost::shared_ptr<AutomationControl>
lookup_ac (SessionObject* o, FIDString id)
{
	Stripable* s = dynamic_cast<Stripable*> (o);
	if (!s) {
		return boost::shared_ptr<AutomationControl> ();
	}

	if (0 == strcmp (id, ContextInfo::kMute)) {
		return s->mute_control ();
	} else if (0 == strcmp (id, ContextInfo::kSolo)) {
		return s->solo_control ();
	} else if (0 == strcmp (id, ContextInfo::kPan)) {
		return s->pan_azimuth_control ();
	} else if (0 == strcmp (id, ContextInfo::kVolume)) {
		return s->gain_control ();
	} else if (0 == strncmp (id, ContextInfo::kSendLevel, strlen (ContextInfo::kSendLevel))) {
#ifdef MIXBUS
		/* Only use mixbus sends, which are identified by providing a
		 * send_enable_controllable().
		 *
		 * The main reason is that the number of Mixbus sends
		 * per route is fixed, but this also works around a crash:
		 *
		 * For Ardour sends, send_level_controllable() calls
		 * Route::nth_send() which takes the _processor_lock.
		 *
		 * However this callback can be triggered initially
		 *   Route::add_processors () -> set_owner() ->
		 *   setup_psl_info_handler() -> ..notify..
		 * with process and processor locks held, leading to
		 * recurive locks (deadlock, or double unlock crash).
		 */
		int send_id = atoi (id + strlen (ContextInfo::kSendLevel));
		if (s->send_enable_controllable (send_id)) {
			return s->send_level_controllable (send_id);
		}
#endif
	}
	return boost::shared_ptr<AutomationControl> ();
}

tresult
VST3PI::getContextInfoValue (int32& value, FIDString id)
{
	Stripable* s = dynamic_cast<Stripable*> (_owner);
	if (!s) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::getContextInfoValue<int>: not initialized");
		return kNotInitialized;
	}
	if (0 == strcmp (id, ContextInfo::kIndexMode)) {
		value = ContextInfo::kFlatIndex;
	} else if (0 == strcmp (id, ContextInfo::kType)) {
		if (s->is_master ()) {
			value = ContextInfo::kOut;
		} else if (s->presentation_info ().flags () & PresentationInfo::AudioTrack) {
			value = ContextInfo::kTrack;
		} else if (s->presentation_info ().flags () & PresentationInfo::MidiTrack) {
			value = ContextInfo::kSynth;
		} else {
			value = ContextInfo::kBus;
		}
	} else if (0 == strcmp (id, ContextInfo::kMain)) {
		value = s->is_master () ? 1 : 0;
	} else if (0 == strcmp (id, ContextInfo::kIndex)) {
		value = s->presentation_info ().order ();
	} else if (0 == strcmp (id, ContextInfo::kColor)) {
		value = s->presentation_info ().color ();
#if BYTEORDER == kBigEndian
		SWAP_32 (value) // RGBA32 -> ABGR32
#endif
	} else if (0 == strcmp (id, ContextInfo::kVisibility)) {
		value = s->is_hidden () ? 0 : 1;
	} else if (0 == strcmp (id, ContextInfo::kSelected)) {
		value = s->is_selected () ? 1 : 0;
	} else if (0 == strcmp (id, ContextInfo::kFocused)) {
		boost::shared_ptr<Stripable> stripable = s->session ().selection ().first_selected_stripable ();
		value                                  = stripable && stripable.get () == s ? 1 : 0;
	} else if (0 == strcmp (id, ContextInfo::kSendCount)) {
		value = 0;
		while (s->send_enable_controllable (value)) {
			++value;
		}
	} else if (0 == strcmp (id, ContextInfo::kMute)) {
		boost::shared_ptr<MuteControl> ac = s->mute_control ();
		if (ac) {
			psl_subscribe_to (ac, id);
			value = ac->muted_by_self ();
		} else {
			value = 0;
		}
	} else if (0 == strcmp (id, ContextInfo::kSolo)) {
		boost::shared_ptr<SoloControl> ac = s->solo_control ();
		if (ac) {
			psl_subscribe_to (ac, id);
			value = ac->self_soloed ();
		} else {
			value = 0;
		}
	} else {
		DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::getContextInfoValue<int> unsupported ID %1\n", id));
		return kNotImplemented;
	}
	DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::getContextInfoValue<int> %1 = %2\n", id, value));
	return kResultOk;
}

tresult
VST3PI::getContextInfoString (Vst::TChar* string, int32 max_len, FIDString id)
{
	if (!_owner) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::getContextInfoString: not initialized");
		return kNotInitialized;
	}
	if (0 == strcmp (id, ContextInfo::kID)) {
		utf8_to_tchar (string, _owner->id ().to_s (), max_len);
	} else if (0 == strcmp (id, ContextInfo::kName)) {
		utf8_to_tchar (string, _owner->name (), max_len);
	} else if (0 == strcmp (id, ContextInfo::kActiveDocumentID)) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::setContextInfoString: NOT IMPLEMENTED (%1)\n", id));
		return kNotImplemented; // XXX TODO
	} else if (0 == strcmp (id, ContextInfo::kDocumentID)) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::setContextInfoString: NOT IMPLEMENTED (%1)\n", id));
		return kNotImplemented; // XXX TODO
	} else if (0 == strcmp (id, ContextInfo::kDocumentName)) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::setContextInfoString: NOT IMPLEMENTED (%1)\n", id));
		return kNotImplemented; // XXX TODO
	} else if (0 == strcmp (id, ContextInfo::kDocumentFolder)) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::setContextInfoString: NOT IMPLEMENTED (%1)\n", id));
		return kNotImplemented; // XXX TODO
	} else if (0 == strcmp (id, ContextInfo::kAudioFolder)) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::setContextInfoString: NOT IMPLEMENTED (%1)\n", id));
		return kNotImplemented; // XXX TODO
	} else {
		boost::shared_ptr<AutomationControl> ac = lookup_ac (_owner, id);
		if (!ac) {
			DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::getContextInfoString unsupported ID %1\n", id));
			return kInvalidArgument;
		}
		utf8_to_tchar (string, ac->get_user_string (), max_len);
	}
	DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::getContextInfoValue<string> %1 = %2\n", id, tchar_to_utf8 (string)));
	return kResultOk;
}

tresult
VST3PI::getContextInfoValue (double& value, FIDString id)
{
	Stripable* s = dynamic_cast<Stripable*> (_owner);
	if (!s) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::getContextInfoValue<double>: not initialized");
		return kNotInitialized;
	}
	if (0 == strcmp (id, ContextInfo::kMaxVolume)) {
		value = s->gain_control ()->upper ();
	} else if (0 == strcmp (id, ContextInfo::kMaxSendLevel)) {
#ifdef MIXBUS
		if (s->send_level_controllable (0)) {
			value = s->send_level_controllable (0)->upper (); // pow (10.0, .05 *  15.0);
		}
#endif
		value = 2.0; // Config->get_max_gain();
	} else if (0 == strcmp (id, ContextInfo::kVolume)) {
		boost::shared_ptr<AutomationControl> ac = s->gain_control ();
		value                                   = ac->get_value (); // gain coefficient  0..2 (1.0 = 0dB)
		psl_subscribe_to (ac, id);
	} else if (0 == strcmp (id, ContextInfo::kPan)) {
		boost::shared_ptr<AutomationControl> ac = s->pan_azimuth_control ();
		if (ac) {
			value = ac->internal_to_interface (ac->get_value (), true);
			psl_subscribe_to (ac, id);
		} else {
			value = 0.5; // center
		}
	} else if (0 == strncmp (id, ContextInfo::kSendLevel, strlen (ContextInfo::kSendLevel))) {
		boost::shared_ptr<AutomationControl> ac = lookup_ac (_owner, id);
		if (ac) {
			value = ac->get_value (); // gain cofficient
			psl_subscribe_to (ac, id);
		} else {
			DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::getContextInfoValue<double> invalid AC %1\n", id));
			return kInvalidArgument; // send index out of bounds
		}
	} else {
		DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::getContextInfoValue<double> unsupported ID %1\n", id));
		return kInvalidArgument;
	}
	DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::getContextInfoValue<double> %1 = %2\n", id, value));
	return kResultOk;
}

tresult
VST3PI::setContextInfoValue (FIDString id, double value)
{
	if (!_owner) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::setContextInfoValue<double>: not initialized");
		return kNotInitialized;
	}
	DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::setContextInfoValue<double> %1 to %2\n", id, value));
	if (0 == strcmp (id, ContextInfo::kVolume)) {
		boost::shared_ptr<AutomationControl> ac = lookup_ac (_owner, id);
		ac->set_value (value, Controllable::NoGroup);
	} else if (0 == strcmp (id, ContextInfo::kPan)) {
		boost::shared_ptr<AutomationControl> ac = lookup_ac (_owner, id);
		if (ac) {
			ac->set_value (ac->interface_to_internal (value, true), PBD::Controllable::NoGroup);
		}
	} else if (0 == strncmp (id, ContextInfo::kSendLevel, strlen (ContextInfo::kSendLevel))) {
		boost::shared_ptr<AutomationControl> ac = lookup_ac (_owner, id);
		if (ac) {
			ac->set_value (value, Controllable::NoGroup);
		} else {
			return kInvalidArgument; // send index out of bounds
		}
	} else {
		DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::setContextInfoValue<double>: unsupported ID\n");
		return kInvalidArgument;
	}
	return kResultOk;
}

tresult
VST3PI::setContextInfoValue (FIDString id, int32 value)
{
	Stripable* s = dynamic_cast<Stripable*> (_owner);
	if (!s) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::setContextInfoValue<int>: not initialized");
		return kNotInitialized;
	}
	DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::setContextInfoValue<int> %1 to %2\n", id, value));
	if (0 == strcmp (id, ContextInfo::kColor)) {
#if BYTEORDER == kBigEndian
		SWAP_32 (value) // ABGR32 -> RGBA32
#endif
		s->presentation_info ().set_color (value);
	} else if (0 == strcmp (id, ContextInfo::kSelected)) {
		boost::shared_ptr<Stripable> stripable = s->session ().stripable_by_id (s->id ());
		assert (stripable);
		if (value == 0) {
			s->session ().selection ().remove (stripable, boost::shared_ptr<AutomationControl> ());
		} else if (_add_to_selection) {
			s->session ().selection ().add (stripable, boost::shared_ptr<AutomationControl> ());
		} else {
			s->session ().selection ().set (stripable, boost::shared_ptr<AutomationControl> ());
		}
	} else if (0 == strcmp (id, ContextInfo::kMultiSelect)) {
		_add_to_selection = value != 0;
	} else if (0 == strcmp (id, ContextInfo::kMute)) {
		s->session ().set_control (lookup_ac (_owner, id), value != 0 ? 1 : 0, Controllable::NoGroup);
	} else if (0 == strcmp (id, ContextInfo::kSolo)) {
		s->session ().set_control (lookup_ac (_owner, id), value != 0 ? 1 : 0, Controllable::NoGroup);
	} else {
		DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::setContextInfoValue<int>: unsupported ID\n");
		return kNotImplemented;
	}
	return kResultOk;
}

tresult
VST3PI::setContextInfoString (FIDString id, Vst::TChar* string)
{
	if (!_owner) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::setContextInfoString: not initialized");
		return kNotInitialized;
	}
	DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::setContextInfoString %1 to %2\n", id, tchar_to_utf8 (string)));
	if (0 == strcmp (id, ContextInfo::kName)) {
		return _owner->set_name (tchar_to_utf8 (string)) ? kResultOk : kResultFalse;
	}
	DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::setContextInfoString: unsupported ID\n");
	return kInvalidArgument;
}

tresult
VST3PI::beginEditContextInfoValue (FIDString id)
{
	if (!_owner) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::beginEditContextInfoValue: not initialized");
		return kNotInitialized;
	}
	boost::shared_ptr<AutomationControl> ac = lookup_ac (_owner, id);
	if (!ac) {
		return kInvalidArgument;
	}
	DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::beginEditContextInfoValue %1\n", id));
	ac->start_touch (timepos_t (ac->session ().transport_sample ()));
	return kResultOk;
}

tresult
VST3PI::endEditContextInfoValue (FIDString id)
{
	if (!_owner) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::endEditContextInfoValue: not initialized");
		return kNotInitialized;
	}
	boost::shared_ptr<AutomationControl> ac = lookup_ac (_owner, id);
	if (!ac) {
		return kInvalidArgument;
	}
	DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::endEditContextInfoValue %1\n", id));
	ac->stop_touch (timepos_t (ac->session ().transport_sample ()));
	return kResultOk;
}

void
VST3PI::psl_subscribe_to (boost::shared_ptr<ARDOUR::AutomationControl> ac, FIDString id)
{
	FUnknownPtr<IContextInfoHandler2> nfo2 (_controller);
	if (!nfo2) {
		return;
	}

	std::pair<std::set<Evoral::Parameter>::iterator, bool> r = _ac_subscriptions.insert (ac->parameter ());

	if (!r.second) {
		return;
	}

	DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::psl_subscribe_to: %1\n", ac->name ()));
	ac->Changed.connect_same_thread (_ac_connection_list, boost::bind (&VST3PI::foward_signal, this, nfo2.get (), id));
}

void
VST3PI::foward_signal (IContextInfoHandler2* handler, FIDString id) const
{
	assert (handler);
	DEBUG_TRACE (DEBUG::VST3Callbacks, string_compose ("VST3PI::psl_subscribtion AC changed %1\n", id));
	handler->notifyContextInfoChange (id);
}

void
VST3PI::psl_stripable_property_changed (PBD::PropertyChange const& what_changed)
{
	FUnknownPtr<IContextInfoHandler>  nfo (_controller);
	FUnknownPtr<IContextInfoHandler2> nfo2 (_controller);
	if (nfo && !nfo2) {
		DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::psl_stripable_property_changed v1\n");
		nfo->notifyContextInfoChange ();
	}
	if (!nfo2) {
		return;
	}

	DEBUG_TRACE (DEBUG::VST3Callbacks, "VST3PI::psl_stripable_property_changed v2\n");

	if (what_changed.contains (Properties::selected)) {
		nfo2->notifyContextInfoChange (ContextInfo::kSelected);
		nfo2->notifyContextInfoChange (ContextInfo::kFocused); // XXX
	}
	if (what_changed.contains (Properties::hidden)) {
		nfo2->notifyContextInfoChange (ContextInfo::kVisibility);
	}
	if (what_changed.contains (Properties::name)) {
		nfo2->notifyContextInfoChange (ContextInfo::kName);
	}
	if (what_changed.contains (Properties::color)) {
		nfo2->notifyContextInfoChange (ContextInfo::kColor);
	}
}

bool
VST3PI::setup_psl_info_handler ()
{
	/* initial update */
	FUnknownPtr<IContextInfoHandler>  nfo (_controller);
	FUnknownPtr<IContextInfoHandler2> nfo2 (_controller);
	DEBUG_TRACE (DEBUG::VST3Config, string_compose ("VST3PI::setup_psl_info_handler: (%1) (%2)\n", nfo != 0, nfo2 != 0));

	if (nfo2) {
		nfo2->notifyContextInfoChange ("");
	} else if (nfo) {
		nfo->notifyContextInfoChange ();
	}

	if (!nfo && !nfo2) {
		return false;
	}

	Stripable* s = dynamic_cast<Stripable*> (_owner);
	s->PropertyChanged.connect_same_thread (_strip_connections, boost::bind (&VST3PI::psl_stripable_property_changed, this, _1));
	s->presentation_info ().PropertyChanged.connect_same_thread (_strip_connections, boost::bind (&VST3PI::psl_stripable_property_changed, this, _1));

	return true;
}

/* ****************************************************************************
 * GUI
 */

IPlugView*
VST3PI::try_create_view () const
{
	IPlugView* view = _controller->createView (Vst::ViewType::kEditor);
	if (!view) {
		view = _controller->createView (0);
	}
	if (!view) {
		view = FUnknownPtr<IPlugView> (_controller).take ();
		if (view) {
			view->addRef ();
		}
	}
	return view;
}

IPlugView*
VST3PI::view ()
{
	if (!_view) {
		_view = try_create_view ();
		if (_view) {
			_view->setFrame (this);
		}
	}
	return _view;
}

void
VST3PI::close_view ()
{
	if (!_view) {
		return;
	}
	_view->removed ();
	_view->setFrame (0);
	_view->release ();
	_view = 0;
}

bool
VST3PI::has_editor () const
{
	IPlugView* view = _view;
	if (!view) {
		view = try_create_view ();
	}

	bool rv = false;
	if (view) {
#ifdef PLATFORM_WINDOWS
		rv = kResultOk == view->isPlatformTypeSupported (kPlatformTypeHWND);
#elif defined(__APPLE__)
		rv = kResultOk == view->isPlatformTypeSupported (kPlatformTypeNSView);
#else
		rv = kResultOk == view->isPlatformTypeSupported (kPlatformTypeX11EmbedWindowID);
#endif
		if (!_view) {
			view->release ();
		}
	}
	return rv;
}

#if SMTG_OS_LINUX
void
VST3PI::set_runloop (Linux::IRunLoop* run_loop)
{
	_run_loop = run_loop;
}
#endif

tresult
VST3PI::resizeView (IPlugView* view, ViewRect* new_size)
{
	OnResizeView (new_size->getWidth (), new_size->getHeight ()); /* EMIT SIGNAL */
	return view->onSize (new_size);
}
