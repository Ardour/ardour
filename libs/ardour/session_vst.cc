/*
 * Copyright (C) 2007-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2015-2018 John Emmas <john@creativepost.co.uk>
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

#ifndef COMPILER_MSVC
#include <stdbool.h>
#endif
#include <cstdio>

#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/plugin_insert.h"
#include "ardour/windows_vst_plugin.h"
#include "ardour/vestige/vestige.h"
#include "ardour/vst_types.h"
#ifdef WINDOWS_VST_SUPPORT
#include <fst.h>
#endif

#include "pbd/i18n.h"

using namespace ARDOUR;

#define SHOW_CALLBACK(MSG) DEBUG_TRACE (PBD::DEBUG::VSTCallbacks, string_compose (MSG " val = %1 idx = %2\n", index, value))

int Session::vst_current_loading_id = 0;
const char* Session::vst_can_do_strings[] = {
	X_("supplyIdle"),
	X_("sendVstTimeInfo"),
	X_("sendVstEvents"),
	X_("sendVstMidiEvent"),
	X_("receiveVstEvents"),
	X_("receiveVstMidiEvent"),
	X_("supportShell"),
	X_("shellCategory"),
	X_("shellCategorycurID"),
	X_("sizeWindow")
};
const int Session::vst_can_do_string_count = sizeof (vst_can_do_strings) / sizeof (char*);

intptr_t Session::vst_callback (
	AEffect* effect,
	int32_t opcode,
	int32_t index,
	intptr_t value,
	void* ptr,
	float opt
	)
{
	VSTPlugin* plug;
	Session* session;
	static VstTimeInfo _timeinfo; // only uses as fallback
	VstTimeInfo* timeinfo;
	int32_t newflags = 0;

	if (effect && effect->ptr1) {
		plug = (VSTPlugin *) (effect->ptr1);
		session = &plug->session();
		timeinfo = plug->timeinfo ();
		DEBUG_TRACE (PBD::DEBUG::VSTCallbacks, string_compose ("am callback 0x%1%2, opcode = %3%4, plugin = \"%5\"\n",
					std::hex, (void*) DEBUG_THREAD_SELF,
					std::dec, opcode, plug->name()));
		if (plug->_for_impulse_analysis) {
			plug = 0;
		}
	} else {
		plug = 0;
		session = 0;
		timeinfo = &_timeinfo;
		DEBUG_TRACE (PBD::DEBUG::VSTCallbacks, string_compose ("am callback 0x%1%2, opcode = %3%4\n",
					std::hex, (void*) DEBUG_THREAD_SELF,
					std::dec, opcode));
	}

	switch(opcode){

	case audioMasterAutomate:
		SHOW_CALLBACK ("audioMasterAutomate");
		// index, value, returns 0
		if (plug) {
			plug->parameter_changed_externally (index, opt);
		}
		return 0;

	case audioMasterVersion:
		SHOW_CALLBACK ("audioMasterVersion");
		// vst version, currently 2 (0 for older)
		return 2400;

	case audioMasterCurrentId:
		SHOW_CALLBACK ("audioMasterCurrentId");
		// returns the unique id of a plug that's currently loading
		return vst_current_loading_id;

	case audioMasterIdle:
		SHOW_CALLBACK ("audioMasterIdle");
#ifdef WINDOWS_VST_SUPPORT
		fst_audio_master_idle();
#endif
		if (effect) {
			effect->dispatcher(effect, effEditIdle, 0, 0, NULL, 0.0f);
		}
		return 0;

	case audioMasterPinConnected:
		SHOW_CALLBACK ("audioMasterPinConnected");
		// inquire if an input or output is beeing connected;
		// index enumerates input or output counting from zero:
		// value is 0 for input and != 0 otherwise. note: the
		// return value is 0 for <true> such that older versions
		// will always return true.
		if (!plug) {
			// we don't know.
			// but ardour always connects all buffers, so we're good
			return 0;
		}
		switch (value) {
			case 0:
				if (plug->plugin_insert ()) {
					bool valid;
					const ChanMapping& map (plug->plugin_insert ()->input_map (plug->plugin_number ()));
					map.get (DataType::AUDIO, index, &valid);
					return valid ? 0 : 1;
				}
				if (index < plug->plugin()->numInputs) {
					return 0;
				}
				break;
			case 1:
#if 0 // investigate, the outputs *are* connected to scratch buffers
				if (plug->plugin_insert ()) {
					bool valid;
					const ChanMapping& map (plug->plugin_insert ()->output_map (plug->plugin_number ()));
					map.get (DataType::AUDIO, index, &valid);
					return valid ? 0 : 1;
				}
#endif
				if (index < plug->plugin()->numOutputs) {
					return 0;
				}
				break;
			default:
				break;
		}
		return 1;

	case audioMasterWantMidi:
		SHOW_CALLBACK ("audioMasterWantMidi");
		// <value> is a filter which is currently ignored
		if (plug && plug->get_info() != NULL) {
			plug->get_info()->n_inputs.set_midi (1);
		}
		return 0;

	case audioMasterGetTime:
		SHOW_CALLBACK ("audioMasterGetTime");
		newflags = kVstNanosValid | kVstAutomationWriting | kVstAutomationReading;

		timeinfo->nanoSeconds = g_get_monotonic_time () * 1000;

		if (plug && session) {
			samplepos_t now = plug->transport_sample();

			timeinfo->samplePos = now;
			timeinfo->sampleRate = session->sample_rate();

			if (value & (kVstTempoValid)) {
				const Tempo& t (session->tempo_map().tempo_at_sample (now));
				timeinfo->tempo = t.quarter_notes_per_minute ();
				newflags |= (kVstTempoValid);
			}
			if (value & (kVstTimeSigValid)) {
				const MeterSection& ms (session->tempo_map().meter_section_at_sample (now));
				timeinfo->timeSigNumerator = ms.divisions_per_bar ();
				timeinfo->timeSigDenominator = ms.note_divisor ();
				newflags |= (kVstTimeSigValid);
			}
			if ((value & (kVstPpqPosValid)) || (value & (kVstBarsValid))) {
				Temporal::BBT_Time bbt;

				try {
					bbt = session->tempo_map().bbt_at_sample_rt (now);
					bbt.beats = 1;
					bbt.ticks = 0;
					/* exact quarter note */
					double ppqBar = session->tempo_map().quarter_note_at_bbt_rt (bbt);
					/* quarter note at sample position (not rounded to note subdivision) */
					double ppqPos = session->tempo_map().quarter_note_at_sample_rt (now);
					if (value & (kVstPpqPosValid)) {
						timeinfo->ppqPos = ppqPos;
						newflags |= kVstPpqPosValid;
					}

					if (value & (kVstBarsValid)) {
						timeinfo->barStartPos = ppqBar;
						newflags |= kVstBarsValid;
					}

				} catch (...) {
					/* relax */
				}
			}

			if (value & (kVstSmpteValid)) {
				Timecode::Time t;

				session->timecode_time (now, t);

				timeinfo->smpteOffset = (t.hours * t.rate * 60.0 * 60.0) +
					(t.minutes * t.rate * 60.0) +
					(t.seconds * t.rate) +
					(t.frames) +
					(t.subframes);

				timeinfo->smpteOffset *= 80.0; /* VST spec is 1/80th samples */

				if (session->timecode_drop_frames()) {
					if (session->timecode_frames_per_second() == 30.0) {
						timeinfo->smpteFrameRate = 5;
					} else {
						timeinfo->smpteFrameRate = 4; /* 29.97 assumed, thanks VST */
					}
				} else {
					if (session->timecode_frames_per_second() == 24.0) {
						timeinfo->smpteFrameRate = 0;
					} else if (session->timecode_frames_per_second() == 24.975) {
						timeinfo->smpteFrameRate = 2;
					} else if (session->timecode_frames_per_second() == 25.0) {
						timeinfo->smpteFrameRate = 1;
					} else {
						timeinfo->smpteFrameRate = 3; /* 30 fps */
					}
				}
				newflags |= (kVstSmpteValid);
			}

			if (session->actively_recording ()) {
				newflags |= kVstTransportRecording;
			}

			if (plug->transport_speed () != 0.0f) {
				newflags |= kVstTransportPlaying;
			}

			if (session->get_play_loop ()) {
				newflags |= kVstTransportCycleActive;
				Location * looploc = session->locations ()->auto_loop_location ();
				if (looploc) try {
#warning NUTEMPO FIXME needs new session tempo map
						//timeinfo->cycleStartPos = session->tempo_map ().quarter_note_at_sample_rt (looploc->start ());
						//timeinfo->cycleEndPos = session->tempo_map ().quarter_note_at_sample_rt (looploc->end ());
						// newflags |= kVstCyclePosValid;
				} catch (...) { }
			}

		} else {
			timeinfo->samplePos = 0;
			timeinfo->sampleRate = AudioEngine::instance()->sample_rate();
		}

		if ((timeinfo->flags & (kVstTransportPlaying | kVstTransportRecording | kVstTransportCycleActive))
		    !=
		    (newflags        & (kVstTransportPlaying | kVstTransportRecording | kVstTransportCycleActive)))
		{
			newflags |= kVstTransportChanged;
		}

		timeinfo->flags = newflags;
		return (intptr_t) timeinfo;

	case audioMasterProcessEvents:
		SHOW_CALLBACK ("audioMasterProcessEvents");
		// VstEvents* in <ptr>
		if (plug && plug->midi_buffer()) {
			VstEvents* v = (VstEvents*)ptr;
			for (int n = 0 ; n < v->numEvents; ++n) {
				VstMidiEvent *vme = (VstMidiEvent*) (v->events[n]->dump);
				if (vme->type == kVstMidiType) {
					plug->midi_buffer()->push_back(vme->deltaSamples, Evoral::MIDI_EVENT, 3, (uint8_t*)vme->midiData);
				}
			}
		}
		return 0;

	case audioMasterSetTime:
		SHOW_CALLBACK ("audioMasterSetTime");
		// VstTimenfo* in <ptr>, filter in <value>, not supported
		return 0;

	case audioMasterTempoAt:
		SHOW_CALLBACK ("audioMasterTempoAt");
		// returns tempo (in bpm * 10000) at sample sample location passed in <value>
		if (session) {
			const Tempo& t (session->tempo_map().tempo_at_sample (value));
			return t.quarter_notes_per_minute() * 1000;
		} else {
			return 0;
		}
		break;

	case audioMasterGetNumAutomatableParameters:
		SHOW_CALLBACK ("audioMasterGetNumAutomatableParameters");
		return 0;

	case audioMasterGetParameterQuantization:
		SHOW_CALLBACK ("audioMasterGetParameterQuantization");
		// returns the integer value for +1.0 representation,
		// or 1 if full single float precision is maintained
		// in automation. parameter index in <value> (-1: all, any)
		return 0;

	case audioMasterIOChanged:
		SHOW_CALLBACK ("audioMasterIOChanged");
		// numInputs and/or numOutputs has changed
		return 0;

	case audioMasterNeedIdle:
		SHOW_CALLBACK ("audioMasterNeedIdle");
		// plug needs idle calls (outside its editor window)
		if (plug) {
			plug->state()->wantIdle = 1;
		}
		return 0;

	case audioMasterSizeWindow:
		SHOW_CALLBACK ("audioMasterSizeWindow");
		if (plug && plug->state()) {
			if (plug->state()->width != index || plug->state()->height != value) {
				plug->state()->width = index;
				plug->state()->height = value;
#ifndef NDEBUG
				printf ("audioMasterSizeWindow %d %d\n", plug->state()->width, plug->state()->height);
#endif
				plug->VSTSizeWindow (); /* EMIT SIGNAL */
			}
		}
		return 1;

	case audioMasterGetSampleRate:
		SHOW_CALLBACK ("audioMasterGetSampleRate");
		if (session) {
			return session->sample_rate();
		}
		return 0;

	case audioMasterGetBlockSize:
		SHOW_CALLBACK ("audioMasterGetBlockSize");
		if (session) {
			return session->get_block_size();
		}
		return 0;

	case audioMasterGetInputLatency:
		SHOW_CALLBACK ("audioMasterGetInputLatency");
		return 0;

	case audioMasterGetOutputLatency:
		SHOW_CALLBACK ("audioMasterGetOutputLatency");
		return 0;

	case audioMasterGetPreviousPlug:
		SHOW_CALLBACK ("audioMasterGetPreviousPlug");
		// input pin in <value> (-1: first to come), returns cEffect*
		return 0;

	case audioMasterGetNextPlug:
		SHOW_CALLBACK ("audioMasterGetNextPlug");
		// output pin in <value> (-1: first to come), returns cEffect*
		return 0;

	case audioMasterWillReplaceOrAccumulate:
		SHOW_CALLBACK ("audioMasterWillReplaceOrAccumulate");
		// returns: 0: not supported, 1: replace, 2: accumulate
		return 0;

	case audioMasterGetCurrentProcessLevel:
		SHOW_CALLBACK ("audioMasterGetCurrentProcessLevel");
		// returns: 0: not supported,
		// 1: currently in user thread (gui)
		// 2: currently in audio thread (where process is called)
		// 3: currently in 'sequencer' thread (midi, timer etc)
		// 4: currently offline processing and thus in user thread
		// other: not defined, but probably pre-empting user thread.
		return 0;

	case audioMasterGetAutomationState:
		SHOW_CALLBACK ("audioMasterGetAutomationState");
		// returns 0: not supported, 1: off, 2:read, 3:write, 4:read/write
		// offline
		return 0;

	case audioMasterOfflineStart:
		SHOW_CALLBACK ("audioMasterOfflineStart");
		return 0;

	case audioMasterOfflineRead:
		SHOW_CALLBACK ("audioMasterOfflineRead");
		// ptr points to offline structure, see below. return 0: error, 1 ok
		return 0;

	case audioMasterOfflineWrite:
		SHOW_CALLBACK ("audioMasterOfflineWrite");
		// same as read
		return 0;

	case audioMasterOfflineGetCurrentPass:
		SHOW_CALLBACK ("audioMasterOfflineGetCurrentPass");
		return 0;

	case audioMasterOfflineGetCurrentMetaPass:
		SHOW_CALLBACK ("audioMasterOfflineGetCurrentMetaPass");
		return 0;

	case audioMasterSetOutputSampleRate:
		SHOW_CALLBACK ("audioMasterSetOutputSampleRate");
		// for variable i/o, sample rate in <opt>
		return 0;

	case audioMasterGetSpeakerArrangement:
		SHOW_CALLBACK ("audioMasterGetSpeakerArrangement");
		// (long)input in <value>, output in <ptr>
		return 0;

	case audioMasterGetVendorString:
		SHOW_CALLBACK ("audioMasterGetVendorString");
		// fills <ptr> with a string identifying the vendor (max 64 char)
		strcpy ((char*) ptr, "Linux Audio Systems");
		return 1;

	case audioMasterGetProductString:
		SHOW_CALLBACK ("audioMasterGetProductString");
		// fills <ptr> with a string with product name (max 64 char)
		strcpy ((char*) ptr, PROGRAM_NAME);
		return 1;

	case audioMasterGetVendorVersion:
		SHOW_CALLBACK ("audioMasterGetVendorVersion");
		// returns vendor-specific version
		return 900;

	case audioMasterVendorSpecific:
		SHOW_CALLBACK ("audioMasterVendorSpecific");
		// no definition, vendor specific handling
		return 0;

	case audioMasterSetIcon:
		SHOW_CALLBACK ("audioMasterSetIcon");
		// void* in <ptr>, format not defined yet
		return 0;

	case audioMasterCanDo:
		SHOW_CALLBACK ("audioMasterCanDo");
		// string in ptr,  (const char*)ptr
		for (int i = 0; i < vst_can_do_string_count; i++) {
			if (! strcmp(vst_can_do_strings[i], (const char*)ptr)) {
				return 1;
			}
		}
		return 0;

	case audioMasterGetLanguage:
		SHOW_CALLBACK ("audioMasterGetLanguage");
		// see enum
		return 0;

	case audioMasterOpenWindow:
		SHOW_CALLBACK ("audioMasterOpenWindow");
		// returns platform specific ptr
		return 0;

	case audioMasterCloseWindow:
		SHOW_CALLBACK ("audioMasterCloseWindow");
		// close window, platform specific handle in <ptr>
		return 0;

	case audioMasterGetDirectory:
		SHOW_CALLBACK ("audioMasterGetDirectory");
		// get plug directory, FSSpec on MAC, else char*
		return 0;

	case audioMasterUpdateDisplay:
		SHOW_CALLBACK ("audioMasterUpdateDisplay");
		/* Something has changed, update 'multi-fx' display.
		 * (Ardour watches output ports already, and redraws when idle.)
		 *
		 * We assume that the internal state of the plugin has changed,
		 * and session as well as preset is marked as modified.
		 */
		if (plug) {
			plug->state_changed ();
		}
		return 0;

	case audioMasterBeginEdit:
		SHOW_CALLBACK ("audioMasterBeginEdit");
		// begin of automation session (when mouse down), parameter index in <index>
		if (plug && plug->plugin_insert ()) {
			boost::shared_ptr<AutomationControl> ac = plug->plugin_insert ()->automation_control (Evoral::Parameter (PluginAutomation, 0, index));
			if (ac) {
				ac->start_touch (timepos_t (ac->session().transport_sample()));
			}
		}
		return 0;

	case audioMasterEndEdit:
		SHOW_CALLBACK ("audioMasterEndEdit");
		// end of automation session (when mouse up),     parameter index in <index>
		if (plug && plug->plugin_insert ()) {
			boost::shared_ptr<AutomationControl> ac = plug->plugin_insert ()->automation_control (Evoral::Parameter (PluginAutomation, 0, index));
			if (ac) {
				ac->stop_touch (timepos_t (ac->session().transport_sample()));
			}
		}
		return 0;

	case audioMasterOpenFileSelector:
		SHOW_CALLBACK ("audioMasterOpenFileSelector");
		// open a fileselector window with VstFileSelect* in <ptr>
		return 0;

	default:
		DEBUG_TRACE (PBD::DEBUG::VSTCallbacks, string_compose ("VST master dispatcher: undefed: %1\n", opcode));
		break;
	}

	return 0;
}
