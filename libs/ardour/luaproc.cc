/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Julien "_FrnchFrgg_" RIVAUD <frnchfrgg@free.fr>
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

#include <glib.h>
#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include "pbd/gstdio_compat.h"
#include "pbd/pthread_utils.h"

#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/filesystem_paths.h"
#include "ardour/luabindings.h"
#include "ardour/luaproc.h"
#include "ardour/luascripting.h"
#include "ardour/midi_buffer.h"
#include "ardour/plugin.h"
#include "ardour/session.h"

#include "LuaBridge/LuaBridge.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;

LuaProc::LuaProc (AudioEngine& engine,
                  Session& session,
                  const std::string &script)
	: Plugin (engine, session)
	, _mempool ("LuaProc", 3145728)
#ifdef USE_TLSF
	, lua (lua_newstate (&PBD::TLSF::lalloc, &_mempool))
#elif defined USE_MALLOC
	, lua ()
#else
	, lua (lua_newstate (&PBD::ReallocPool::lalloc, &_mempool))
#endif
	, _lua_dsp (0)
	, _lua_latency (0)
	, _script (script)
	, _lua_does_channelmapping (false)
	, _lua_has_inline_display (false)
	, _designated_bypass_port (UINT32_MAX)
	, _signal_latency (0)
	, _control_data (0)
	, _shadow_data (0)
	, _configured (false)
	, _has_midi_input (false)
	, _has_midi_output (false)
{
	init ();

	/* when loading a session, or pasing a processor,
	 * the script is set during set_state();
	 */
	if (!_script.empty () && load_script ()) {
		throw failed_constructor ();
	}
}

LuaProc::LuaProc (const LuaProc &other)
	: Plugin (other)
	, _mempool ("LuaProc", 3145728)
#ifdef USE_TLSF
	, lua (lua_newstate (&PBD::TLSF::lalloc, &_mempool))
#elif defined USE_MALLOC
	, lua ()
#else
	, lua (lua_newstate (&PBD::ReallocPool::lalloc, &_mempool))
#endif
	, _lua_dsp (0)
	, _lua_latency (0)
	, _script (other.script ())
	, _origin (other._origin)
	, _lua_does_channelmapping (false)
	, _lua_has_inline_display (false)
	, _designated_bypass_port (UINT32_MAX)
	, _signal_latency (0)
	, _control_data (0)
	, _shadow_data (0)
	, _configured (false)
	, _has_midi_input (false)
	, _has_midi_output (false)
{
	init ();

	if (load_script ()) {
		throw failed_constructor ();
	}

	for (uint32_t i = 0; i < parameter_count (); ++i) {
		_control_data[i] = other._shadow_data[i];
		_shadow_data[i]  = other._shadow_data[i];
	}
}

LuaProc::~LuaProc () {
#ifdef WITH_LUAPROC_STATS
	if (_info && _stats_cnt > 0) {
		printf ("LuaProc: '%s' run()  avg: %.3f  max: %.3f [ms] p: %.1f\n",
				_info->name.c_str (),
				0.0001f * _stats_avg[0] / (float) _stats_cnt,
				0.0001f * _stats_max[0],
				_stats_max[0] * (float)_stats_cnt / _stats_avg[0]);
		printf ("LuaProc: '%s' gc()   avg: %.3f  max: %.3f [ms] p: %.1f\n",
				_info->name.c_str (),
				0.0001f * _stats_avg[1] / (float) _stats_cnt,
				0.0001f * _stats_max[1],
				_stats_max[1] * (float)_stats_cnt / _stats_avg[1]);
	}
#endif
	lua.do_command ("collectgarbage();");
	delete (_lua_dsp);
	delete (_lua_latency);
	delete [] _control_data;
	delete [] _shadow_data;
}

void
LuaProc::init ()
{
#ifdef WITH_LUAPROC_STATS
	_stats_avg[0] = _stats_avg[1] = _stats_max[0] = _stats_max[1] = 0;
	_stats_cnt = -25;
#endif

	lua.Print.connect (sigc::mem_fun (*this, &LuaProc::lua_print));
	// register session object
	lua_State* L = lua.getState ();
	lua_mlock (L, 1);
	LuaBindings::stddef (L);
	LuaBindings::common (L);
	LuaBindings::dsp (L);

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Ardour")
		.deriveClass <LuaProc, PBD::StatefulDestructible> ("LuaProc")
		.addFunction ("queue_draw", &LuaProc::queue_draw)
		.addFunction ("shmem", &LuaProc::instance_shm)
		.addFunction ("table", &LuaProc::instance_ref)
		.addFunction ("route", &LuaProc::route)
		.addFunction ("unique_id", &LuaProc::unique_id)
		.addFunction ("name", &LuaProc::name)
		.endClass ()
		.endNamespace ();
	lua_mlock (L, 0);

	// add session to global lua namespace
	luabridge::push <Session *> (L, &_session);
	lua_setglobal (L, "Session");

	// instance
	luabridge::push <LuaProc *> (L, this);
	lua_setglobal (L, "self");

	// sandbox
	lua.sandbox (true);
#if 0
	lua.do_command ("for n in pairs(_G) do print(n) end print ('----')"); // print global env
#endif
	lua.do_command ("function ardour () end");
}

boost::weak_ptr<Route>
LuaProc::route () const
{
	if (!_owner) {
		return boost::weak_ptr<Route>();
	}
	return static_cast<Route*>(_owner)->weakroute ();
}

void
LuaProc::lua_print (std::string s) {
	std::cout <<"LuaProc: " << s << "\n";
	PBD::error << "LuaProc: " << s << "\n";
}

bool
LuaProc::load_script ()
{
	if (_script.empty ()) {
		return true;
	}
	assert (!_lua_dsp); // don't allow to re-initialize
	LuaPluginInfoPtr lpi;

	// TODO: refine APIs; function arguments..
	// - perform channel-map in ardour (silent/scratch buffers) ?
	// - control-port API (explicit get/set functions ??)
	// - latency reporting (global var? ctrl-port? set-function ?)
	// - MIDI -> sparse table of events
	//     { [sample] => { Event }, .. }
	//   or  { { sample, Event }, .. }

	try {
		LuaScriptInfoPtr lsi = LuaScripting::script_info (_script);
		lpi = LuaPluginInfoPtr (new LuaPluginInfo (lsi));
		assert (lpi);
		set_info (lpi);
		_mempool.set_name ("LuaProc: " + lsi->name);
		_docs = lsi->description;
	} catch (failed_constructor& err) {
		return true;
	}

	lua_State* L = lua.getState ();
	lua.do_command (_script);

	// check if script has a DSP callback
	luabridge::LuaRef lua_dsp_run = luabridge::getGlobal (L, "dsp_run");
	luabridge::LuaRef lua_dsp_map = luabridge::getGlobal (L, "dsp_runmap");

	if ((lua_dsp_run.type () != LUA_TFUNCTION) == (lua_dsp_map.type () != LUA_TFUNCTION)) {
		return true;
	}

	if (lua_dsp_run.type () == LUA_TFUNCTION) {
		_lua_dsp = new luabridge::LuaRef (lua_dsp_run);
	}
	else if (lua_dsp_map.type () == LUA_TFUNCTION) {
		_lua_dsp = new luabridge::LuaRef (lua_dsp_map);
		_lua_does_channelmapping = true;
	}
	else {
		assert (0);
		return true;
	}

	luabridge::LuaRef lua_dsp_latency = luabridge::getGlobal (L, "dsp_latency");
	if (lua_dsp_latency.type () == LUA_TFUNCTION) {
		_lua_latency = new luabridge::LuaRef (lua_dsp_latency);
	}

	// initialize the DSP if needed
	luabridge::LuaRef lua_dsp_init = luabridge::getGlobal (L, "dsp_init");
	if (lua_dsp_init.type () == LUA_TFUNCTION) {
		try {
			lua_dsp_init (_session.nominal_sample_rate ());
		} catch (luabridge::LuaException const& e) {
			return true; // error
		} catch (...) {
			return true;
		}
	}

	_ctrl_params.clear ();

	luabridge::LuaRef lua_render = luabridge::getGlobal (L, "render_inline");
	if (lua_render.isFunction ()) {
		_lua_has_inline_display = true;
	}

	luabridge::LuaRef lua_params = luabridge::getGlobal (L, "dsp_params");
	if (lua_params.isFunction ()) {

		// call function // add try {} catch (luabridge::LuaException const& e)
		luabridge::LuaRef params = lua_params ();

		if (params.isTable ()) {

			for (luabridge::Iterator i (params); !i.isNil (); ++i) {
				// required fields
				if (!i.key ().isNumber ())           { return true; }
				if (!i.value ().isTable ())          { return true; }
				if (!i.value ()["type"].isString ()) { return true; }
				if (!i.value ()["name"].isString ()) { return true; }
				if (!i.value ()["min"].isNumber ())  { return true; }
				if (!i.value ()["max"].isNumber ())  { return true; }

				int pn = i.key ().cast<int> ();
				std::string type = i.value ()["type"].cast<std::string> ();
				if (type == "input") {
					if (!i.value ()["default"].isNumber ()) {
						return true; // error
					}
					_ctrl_params.push_back (std::make_pair (false, pn));
				}
				else if (type == "output") {
					_ctrl_params.push_back (std::make_pair (true, pn));
				} else {
					return true; // error
				}
				assert (pn == (int) _ctrl_params.size ());

				//_param_desc[pn] = boost::shared_ptr<ParameterDescriptor> (new ParameterDescriptor());
				luabridge::LuaRef lr = i.value ();

				if (type == "input") {
					_param_desc[pn].normal     = lr["default"].cast<float> ();
				} else {
					_param_desc[pn].normal     = lr["min"].cast<float> (); // output-port, no default
				}
				_param_desc[pn].lower        = lr["min"].cast<float> ();
				_param_desc[pn].upper        = lr["max"].cast<float> ();
				_param_desc[pn].toggled      = lr["toggled"].isBoolean () && (lr["toggled"]).cast<bool> ();
				_param_desc[pn].logarithmic  = lr["logarithmic"].isBoolean () && (lr["logarithmic"]).cast<bool> ();
				_param_desc[pn].integer_step = lr["integer"].isBoolean () && (lr["integer"]).cast<bool> ();
				_param_desc[pn].sr_dependent = lr["ratemult"].isBoolean () && (lr["ratemult"]).cast<bool> ();
				_param_desc[pn].enumeration  = lr["enum"].isBoolean () && (lr["enum"]).cast<bool> ();

				if (lr["bypass"].isBoolean () && (lr["bypass"]).cast<bool> ()) {
					_designated_bypass_port = pn - 1; // lua table starts at 1.
				}

				if (lr["unit"].isString ()) {
					std::string unit = lr["unit"].cast<std::string> ();
					if (unit == "dB")             { _param_desc[pn].unit = ParameterDescriptor::DB; }
					else if (unit == "Hz")        { _param_desc[pn].unit = ParameterDescriptor::HZ; }
					else if (unit == "Midi Note") { _param_desc[pn].unit = ParameterDescriptor::MIDI_NOTE; }
				}
				_param_desc[pn].label        = (lr["name"]).cast<std::string> ();
				_param_desc[pn].scale_points = parse_scale_points (&lr);

				luabridge::LuaRef doc = lr["doc"];
				if (doc.isString ()) {
					_param_doc[pn] = doc.cast<std::string> ();
				} else {
					_param_doc[pn] = "";
				}
				assert (!(_param_desc[pn].toggled && _param_desc[pn].logarithmic));
			}
		}
	}

	_control_data = new float[parameter_count ()];
	_shadow_data  = new float[parameter_count ()];

	for (uint32_t i = 0; i < parameter_count (); ++i) {
		if (parameter_is_input (i)) {
			_control_data[i] = _shadow_data[i] = default_value (i);
		}
	}

	// expose ctrl-ports to global lua namespace
	luabridge::push <float *> (L, _control_data);
	lua_setglobal (L, "CtrlPorts");

	return false; // no error
}

bool
LuaProc::can_support_io_configuration (const ChanCount& in, ChanCount& out, ChanCount* imprecise)
{
	// caller must hold process lock (no concurrent calls to interpreter
	_output_configs.clear ();

	lua_State* L = lua.getState ();
	luabridge::LuaRef ioconfig = luabridge::getGlobal (L, "dsp_ioconfig");

	luabridge::LuaRef *_iotable = NULL; // can't use reference :(

	if (ioconfig.isFunction ()) {
		try {
			luabridge::LuaRef iotable = ioconfig ();
			if (iotable.isTable ()) {
				_iotable = new luabridge::LuaRef (iotable);
			}
		} catch (luabridge::LuaException const& e) {
			_iotable = NULL;
		} catch (...) {
			_iotable = NULL;
		}
	}

	if (!_iotable) {
		/* empty table as default */
		luabridge::LuaRef iotable = luabridge::newTable(L);
		_iotable = new luabridge::LuaRef (iotable);
	}

	// now we can reference it.
	luabridge::LuaRef iotable (*_iotable);
	delete _iotable;

	if ((iotable).length () < 1) {
		/* empty table as only config, to get default values */
		luabridge::LuaRef ioconf = luabridge::newTable(L);
		iotable[1] = ioconf;
	}

	const int audio_in = in.n_audio ();
	const int midi_in = in.n_midi ();

	// preferred setting (provided by plugin_insert)
	const int preferred_out = out.n_audio ();
	const int preferred_midiout = out.n_midi ();

	int midi_out = -1;
	int audio_out = -1;
	float penalty = 9999;
	bool found = false;

#define FOUNDCFG_PENALTY(in, out, p) {                              \
  _output_configs.insert (out);                                     \
  if (p < penalty) {                                                \
    audio_out = (out);                                              \
    midi_out = possible_midiout;                                    \
    if (imprecise) {                                                \
      imprecise->set (DataType::AUDIO, (in));                       \
      imprecise->set (DataType::MIDI, possible_midiin);             \
    }                                                               \
    _has_midi_input = (possible_midiin > 0);                        \
    _has_midi_output = (possible_midiout > 0);                      \
    penalty = p;                                                    \
    found = true;                                                   \
  }                                                                 \
}

#define FOUNDCFG_IMPRECISE(in, out) {                               \
  const float p = fabsf ((float)(out) - preferred_out) *            \
                      (((out) > preferred_out) ? 1.1 : 1)           \
                + fabsf ((float)possible_midiout - preferred_midiout) *    \
                      ((possible_midiout - preferred_midiout) ? 0.6 : 0.5) \
                + fabsf ((float)(in) - audio_in) *                  \
                      (((in) > audio_in) ? 275 : 250)               \
                + fabsf ((float)possible_midiin - midi_in) *        \
                      ((possible_midiin - midi_in) ? 100 : 110);    \
  FOUNDCFG_PENALTY(in, out, p);                                     \
}

#define FOUNDCFG(out)                                               \
  FOUNDCFG_IMPRECISE(audio_in, out)

#define ANYTHINGGOES                                                \
  _output_configs.insert (0);

#define UPTO(nch) {                                                 \
  for (int n = 1; n < nch; ++n) {                                   \
    _output_configs.insert (n);                                     \
  }                                                                 \
}

	if (imprecise) {
		*imprecise = in;
	}

	for (luabridge::Iterator i (iotable); !i.isNil (); ++i) {
		luabridge::LuaRef io (i.value ());
		if (!io.isTable()) {
			continue;
		}

		int possible_in = io["audio_in"].isNumber() ? io["audio_in"] : -1;
		int possible_out = io["audio_out"].isNumber() ? io["audio_out"] : -1;
		int possible_midiin = io["midi_in"].isNumber() ? io["midi_in"] : 0;
		int possible_midiout = io["midi_out"].isNumber() ? io["midi_out"] : 0;

		if (midi_in != possible_midiin && !imprecise) {
			continue;
		}

		// exact match
		if ((possible_in == audio_in) && (possible_out == preferred_out)) {
			/* Set penalty so low that this output configuration
			 * will trump any other one */
			FOUNDCFG_PENALTY(audio_in, preferred_out, -1);
		}

		if (possible_out == 0 && possible_midiout == 0) {
			/* skip configurations with no output at all */
			continue;
		}

		if (possible_in == -1 || possible_in == -2) {
			/* wildcard for input */
			if (possible_out == possible_in) {
				/* either both -1 or both -2 (invalid and
				 * interpreted as both -1): out must match in */
				FOUNDCFG (audio_in);
			} else if (possible_out == -3 - possible_in) {
				/* one is -1, the other is -2: any output configuration
				 * possible, pick what the insert prefers */
				FOUNDCFG (preferred_out);
				ANYTHINGGOES;
			} else if (possible_out < -2) {
				/* variable number of outputs up to -N,
				 * invalid if in == -2 but we accept it anyway */
				FOUNDCFG (min (-possible_out, preferred_out));
				UPTO (-possible_out)
			} else {
				/* exact number of outputs */
				FOUNDCFG (possible_out);
			}
		}

		if (possible_in < -2 || possible_in >= 0) {
			/* specified number, exact or up to */
			int desired_in;
			if (possible_in >= 0) {
				/* configuration can only match possible_in */
				desired_in = possible_in;
			} else {
				/* configuration can match up to -possible_in */
				desired_in = min (-possible_in, audio_in);
			}
			if (!imprecise && audio_in != desired_in) {
				/* skip that configuration, it cannot match
				 * the required audio input count, and we
				 * cannot ask for change via \imprecise */
			} else if (possible_out == -1 || possible_out == -2) {
				/* any output configuration possible
				 * out == -2 is invalid, interpreted as out == -1.
				 * Really imprecise only if desired_in != audio_in */
				FOUNDCFG_IMPRECISE (desired_in, preferred_out);
				ANYTHINGGOES;
			} else if (possible_out < -2) {
				/* variable number of outputs up to -N
				 * not specified if in > 0, but we accept it anyway.
				 * Really imprecise only if desired_in != audio_in */
				FOUNDCFG_IMPRECISE (desired_in, min (-possible_out, preferred_out));
				UPTO (-possible_out)
			} else {
				/* exact number of outputs
				 * Really imprecise only if desired_in != audio_in */
				FOUNDCFG_IMPRECISE (desired_in, possible_out);
			}
		}

	}

	if (!found) {
		return false;
	}

	if (imprecise) {
		_selected_in = *imprecise;
	} else {
		_selected_in = in;
	}

	out.set (DataType::MIDI, midi_out);
	out.set (DataType::AUDIO, audio_out);
	_selected_out = out;

	return true;
}

bool
LuaProc::configure_io (ChanCount in, ChanCount out)
{
	in.set (DataType::MIDI, _has_midi_input ? 1 : 0);
	out.set (DataType::MIDI, _has_midi_output ? 1 : 0);

	_info->n_inputs = _selected_in;
	_info->n_outputs = _selected_out;

	// configure the DSP if needed
	if (in != _configured_in || out != _configured_out || !_configured) {
		lua_State* L = lua.getState ();
		luabridge::LuaRef lua_dsp_configure = luabridge::getGlobal (L, "dsp_configure");
		if (lua_dsp_configure.type () == LUA_TFUNCTION) {
			try {
				luabridge::LuaRef io = lua_dsp_configure (in, out);
				if (io.isTable ()) {
					ChanCount lin (_selected_in);
					ChanCount lout (_selected_out);

					if (io["audio_in"].type() == LUA_TNUMBER) {
						const int c = io["audio_in"].cast<int> ();
						if (c >= 0) {
							lin.set (DataType::AUDIO, c);
						}
					}
					if (io["audio_out"].type() == LUA_TNUMBER) {
						const int c = io["audio_out"].cast<int> ();
						if (c >= 0) {
							lout.set (DataType::AUDIO, c);
						}
					}
					if (io["midi_in"].type() == LUA_TNUMBER) {
						const int c = io["midi_in"].cast<int> ();
						if (c >= 0) {
							lin.set (DataType::MIDI, c);
						}
					}
					if (io["midi_out"].type() == LUA_TNUMBER) {
						const int c = io["midi_out"].cast<int> ();
						if (c >= 0) {
							lout.set (DataType::MIDI, c);
						}
					}
					_info->n_inputs = lin;
					_info->n_outputs = lout;
				}
				_configured = true;
			} catch (luabridge::LuaException const& e) {
				PBD::error << "LuaException: " << e.what () << "\n";
#ifndef NDEBUG
				std::cerr << "LuaException: " << e.what () << "\n";
#endif
				return false;
			} catch (...) {
				return false;
			}
		}
	}

	_configured_in = in;
	_configured_out = out;

	return true;
}

int
LuaProc::connect_and_run (BufferSet& bufs,
		samplepos_t start, samplepos_t end, double speed,
		ChanMapping const& in, ChanMapping const& out,
		pframes_t nframes, samplecnt_t offset)
{
	if (!_lua_dsp) {
		return 0;
	}

	Plugin::connect_and_run (bufs, start, end, speed, in, out, nframes, offset);

	// This is needed for ARDOUR::Session requests :(
	assert (SessionEvent::has_per_thread_pool ());

	uint32_t const n = parameter_count ();
	for (uint32_t i = 0; i < n; ++i) {
		if (parameter_is_control (i) && parameter_is_input (i)) {
			_control_data[i] = _shadow_data[i];
		}
	}

#ifdef WITH_LUAPROC_STATS
	int64_t t0 = g_get_monotonic_time ();
#endif

	try {
		if (_lua_does_channelmapping) {
			// run the DSP function
			(*_lua_dsp)(&bufs, &in, &out, nframes, offset);
		} else {
			// map buffers
			BufferSet& silent_bufs  = _session.get_silent_buffers (ChanCount (DataType::AUDIO, 1));
			BufferSet& scratch_bufs = _session.get_scratch_buffers (ChanCount (DataType::AUDIO, 1));

			lua_State* L = lua.getState ();
			luabridge::LuaRef in_map (luabridge::newTable (L));
			luabridge::LuaRef out_map (luabridge::newTable (L));

			const uint32_t audio_in = _configured_in.n_audio ();
			const uint32_t audio_out = _configured_out.n_audio ();
			const uint32_t midi_in = _configured_in.n_midi ();

			for (uint32_t ap = 0; ap < audio_in; ++ap) {
				bool valid;
				const uint32_t buf_index = in.get(DataType::AUDIO, ap, &valid);
				if (valid) {
					in_map[ap + 1] = bufs.get_audio (buf_index).data (offset);
				} else {
					in_map[ap + 1] = silent_bufs.get_audio (0).data (offset);
				}
			}
			for (uint32_t ap = 0; ap < audio_out; ++ap) {
				bool valid;
				const uint32_t buf_index = out.get(DataType::AUDIO, ap, &valid);
				if (valid) {
					out_map[ap + 1] = bufs.get_audio (buf_index).data (offset);
				} else {
					out_map[ap + 1] = scratch_bufs.get_audio (0).data (offset);
				}
			}

			luabridge::LuaRef lua_midi_src_tbl (luabridge::newTable (L));
			int e = 1; // > 1 port, we merge events (unsorted)
			for (uint32_t mp = 0; mp < midi_in; ++mp) {
				bool valid;
				const uint32_t idx = in.get(DataType::MIDI, mp, &valid);
				if (valid) {
					for (MidiBuffer::iterator m = bufs.get_midi(idx).begin();
							m != bufs.get_midi(idx).end(); ++m, ++e) {
						const Evoral::Event<samplepos_t> ev(*m, false);
						luabridge::LuaRef lua_midi_data (luabridge::newTable (L));
						const uint8_t* data = ev.buffer();
						for (uint32_t i = 0; i < ev.size(); ++i) {
							lua_midi_data [i + 1] = data[i];
						}
						luabridge::LuaRef lua_midi_event (luabridge::newTable (L));
						lua_midi_event["time"] = 1 + (*m).time();
						lua_midi_event["data"] = lua_midi_data;
						lua_midi_event["bytes"] = data;
						lua_midi_event["size"] = ev.size();
						lua_midi_src_tbl[e] = lua_midi_event;
					}
				}
			}

			if (_has_midi_input) {
				// XXX TODO This needs a better solution than global namespace
				luabridge::push (L, lua_midi_src_tbl);
				lua_setglobal (L, "midiin");
			}

			luabridge::LuaRef lua_midi_sink_tbl (luabridge::newTable (L));
			if (_has_midi_output) {
				luabridge::push (L, lua_midi_sink_tbl);
				lua_setglobal (L, "midiout");
			}

			// run the DSP function
			(*_lua_dsp)(in_map, out_map, nframes);

			// copy back midi events
			if (_has_midi_output && lua_midi_sink_tbl.isTable ()) {
				bool valid;
				const uint32_t idx = out.get(DataType::MIDI, 0, &valid);
				if (valid && bufs.count().n_midi() > idx) {
					MidiBuffer& mbuf = bufs.get_midi(idx);
					mbuf.silence(0, 0);
					for (luabridge::Iterator i (lua_midi_sink_tbl); !i.isNil (); ++i) {
						if (!i.key ().isNumber ()) { continue; }
						if (!i.value ()["time"].isNumber ()) { continue; }
						if (!i.value ()["data"].isTable ()) { continue; }
						luabridge::LuaRef data_tbl (i.value ()["data"]);
						samplepos_t tme = i.value ()["time"];
						if (tme < 1 || tme > nframes) { continue; }
						uint8_t data[64];
						size_t size = 0;
						for (luabridge::Iterator di (data_tbl); !di.isNil () && size < sizeof(data); ++di, ++size) {
							data[size] = di.value ();
						}
						if (size > 0 && size < 64) {
							mbuf.push_back(tme - 1, size, data);
						}
					}

				}
			}
		}

		if (_lua_latency) {
			_signal_latency = (*_lua_latency)();
		}

	} catch (luabridge::LuaException const& e) {
		PBD::error << "LuaException: " << e.what () << "\n";
#ifndef NDEBUG
		std::cerr << "LuaException: " << e.what () << "\n";
#endif
		return -1;
	} catch (...) {
		return -1;
	}
#ifdef WITH_LUAPROC_STATS
	int64_t t1 = g_get_monotonic_time ();
#endif

	lua.collect_garbage_step ();
#ifdef WITH_LUAPROC_STATS
	if (++_stats_cnt > 0) {
		int64_t t2 = g_get_monotonic_time ();
		int64_t ela0 = t1 - t0;
		int64_t ela1 = t2 - t1;
		if (ela0 > _stats_max[0]) _stats_max[0] = ela0;
		if (ela1 > _stats_max[1]) _stats_max[1] = ela1;
		_stats_avg[0] += ela0;
		_stats_avg[1] += ela1;
	}
#endif
	return 0;
}


void
LuaProc::add_state (XMLNode* root) const
{
	XMLNode*    child;

	gchar* b64 = g_base64_encode ((const guchar*)_script.c_str (), _script.size ());
	std::string b64s (b64);
	g_free (b64);
	XMLNode* script_node = new XMLNode (X_("script"));
	script_node->set_property (X_("lua"), LUA_VERSION);
	script_node->set_property (X_("origin"), _origin);
	script_node->add_content (b64s);
	root->add_child_nocopy (*script_node);

	for (uint32_t i = 0; i < parameter_count(); ++i) {
		if (parameter_is_input(i) && parameter_is_control(i)) {
			child = new XMLNode("Port");
			child->set_property("id", i);
			child->set_property("value", _shadow_data[i]);
			root->add_child_nocopy(*child);
		}
	}
}

int
LuaProc::set_script_from_state (const XMLNode& node)
{
	XMLNode* child;
	if (node.name () != state_node_name ()) {
		return -1;
	}

	if ((child = node.child (X_("script"))) != 0) {
		XMLProperty const* prop;
		if ((prop = node.property ("origin")) != 0) {
			_origin = prop->value();
		}
		for (XMLNodeList::const_iterator n = child->children ().begin (); n != child->children ().end (); ++n) {
			if (!(*n)->is_content ()) { continue; }
			gsize size;
			guchar* buf = g_base64_decode ((*n)->content ().c_str (), &size);
			_script = std::string ((const char*)buf, size);
			g_free (buf);
			if (load_script ()) {
				PBD::error << _("Failed to load Lua script from session state.") << endmsg;
#ifndef NDEBUG
				std::cerr << "Failed Lua Script: " << _script << std::endl;
#endif
				_script = "";
			}
			break;
		}
	}
	if (_script.empty ()) {
		PBD::error << _("Session State for LuaProcessor did not include a Lua script.") << endmsg;
		return -1;
	}
	if (!_lua_dsp) {
		PBD::error << _("Invalid/incompatible Lua script found for LuaProcessor.") << endmsg;
		return -1;
	}
	return 0;
}

int
LuaProc::set_state (const XMLNode& node, int version)
{
	XMLNodeList nodes;
	XMLNodeConstIterator iter;
	XMLNode *child;

	if (_script.empty ()) {
		if (set_script_from_state (node)) {
			return -1;
		}
	}

	if (node.name() != state_node_name()) {
		error << _("Bad node sent to LuaProc::set_state") << endmsg;
		return -1;
	}

	nodes = node.children ("Port");
	for (iter = nodes.begin(); iter != nodes.end(); ++iter) {
		child = *iter;

		uint32_t port_id;
		float value;

		if (!child->get_property("id", port_id)) {
			warning << _("LuaProc: port has no symbol, ignored") << endmsg;
			continue;
		}

		if (!child->get_property("value", value)) {
			warning << _("LuaProc: port has no value, ignored") << endmsg;
			continue;
		}

		set_parameter (port_id, value);
	}

	return Plugin::set_state (node, version);
}

uint32_t
LuaProc::parameter_count () const
{
	return _ctrl_params.size ();
}

float
LuaProc::default_value (uint32_t port)
{
	if (_ctrl_params[port].first) {
		assert (0);
		return 0;
	}
	int lp = _ctrl_params[port].second;
	return _param_desc[lp].normal;
}

void
LuaProc::set_parameter (uint32_t port, float val)
{
	assert (port < parameter_count ());
	if (get_parameter (port) == val) {
		return;
	}
	_shadow_data[port] = val;
	Plugin::set_parameter (port, val);
}

float
LuaProc::get_parameter (uint32_t port) const
{
	if (parameter_is_input (port)) {
		return _shadow_data[port];
	} else {
		return _control_data[port];
	}
}

int
LuaProc::get_parameter_descriptor (uint32_t port, ParameterDescriptor& desc) const
{
	assert (port <= parameter_count ());
	int lp = _ctrl_params[port].second;
	const ParameterDescriptor& d (_param_desc.find(lp)->second);

	desc.lower        = d.lower;
	desc.upper        = d.upper;
	desc.normal       = d.normal;
	desc.toggled      = d.toggled;
	desc.logarithmic  = d.logarithmic;
	desc.integer_step = d.integer_step;
	desc.sr_dependent = d.sr_dependent;
	desc.enumeration  = d.enumeration;
	desc.unit         = d.unit;
	desc.label        = d.label;
	desc.scale_points = d.scale_points;

	desc.update_steps ();
	return 0;
}

std::string
LuaProc::get_parameter_docs (uint32_t port) const {
	assert (port <= parameter_count ());
	int lp = _ctrl_params[port].second;
	return _param_doc.find(lp)->second;
}

uint32_t
LuaProc::nth_parameter (uint32_t port, bool& ok) const
{
	if (port < _ctrl_params.size ()) {
		ok = true;
		return port;
	}
	ok = false;
	return 0;
}

bool
LuaProc::parameter_is_input (uint32_t port) const
{
	assert (port < _ctrl_params.size ());
	return (!_ctrl_params[port].first);
}

bool
LuaProc::parameter_is_output (uint32_t port) const
{
	assert (port < _ctrl_params.size ());
	return (_ctrl_params[port].first);
}

std::set<Evoral::Parameter>
LuaProc::automatable () const
{
	std::set<Evoral::Parameter> automatables;
	for (uint32_t i = 0; i < _ctrl_params.size (); ++i) {
		if (parameter_is_input (i)) {
			automatables.insert (automatables.end (), Evoral::Parameter (PluginAutomation, 0, i));
		}
	}
	return automatables;
}

std::string
LuaProc::describe_parameter (Evoral::Parameter param)
{
	if (param.type () == PluginAutomation && param.id () < parameter_count ()) {
		int lp = _ctrl_params[param.id ()].second;
		return _param_desc[lp].label;
	}
	return "??";
}

boost::shared_ptr<ScalePoints>
LuaProc::parse_scale_points (luabridge::LuaRef* lr)
{
	if (!(*lr)["scalepoints"].isTable()) {
		return boost::shared_ptr<ScalePoints> ();
	}

	int cnt = 0;
	boost::shared_ptr<ScalePoints> rv = boost::shared_ptr<ScalePoints>(new ScalePoints());
	luabridge::LuaRef scalepoints ((*lr)["scalepoints"]);

	for (luabridge::Iterator i (scalepoints); !i.isNil (); ++i) {
		if (!i.key ().isString ())    { continue; }
		if (!i.value ().isNumber ())  { continue; }
		rv->insert(make_pair(i.key ().cast<std::string> (),
					i.value ().cast<float> ()));
		++cnt;
	}

	if (rv->size() > 0) {
		return rv;
	}
	return boost::shared_ptr<ScalePoints> ();
}

boost::shared_ptr<ScalePoints>
LuaProc::get_scale_points (uint32_t port) const
{
	int lp = _ctrl_params[port].second;
	return _param_desc.find(lp)->second.scale_points;
}

void
LuaProc::setup_lua_inline_gui (LuaState *lua_gui)
{
	lua_State* LG = lua_gui->getState ();
	LuaBindings::stddef (LG);
	LuaBindings::common (LG);
	LuaBindings::dsp (LG);
	LuaBindings::osc (LG);

	lua_gui->Print.connect (sigc::mem_fun (*this, &LuaProc::lua_print));
	lua_gui->do_command ("function ardour () end");
	lua_gui->do_command (_script);

	// TODO think: use a weak-pointer here ?
	// (the GUI itself uses a shared ptr to this plugin, so we should be good)
	luabridge::getGlobalNamespace (LG)
		.beginNamespace ("Ardour")
		.beginClass <LuaProc> ("LuaProc")
		.addFunction ("shmem", &LuaProc::instance_shm)
		.addFunction ("table", &LuaProc::instance_ref)
		.endClass ()
		.endNamespace ();

	luabridge::push <LuaProc *> (LG, this);
	lua_setglobal (LG, "self");

	luabridge::push <float *> (LG, _control_data);
	lua_setglobal (LG, "CtrlPorts");
}
////////////////////////////////////////////////////////////////////////////////

#include "ardour/search_paths.h"
#include "sha1.c"

std::string
LuaProc::preset_name_to_uri (const std::string& name) const
{
	std::string uri ("urn:lua:");
	char hash[41];
	Sha1Digest s;
	sha1_init (&s);
	sha1_write (&s, (const uint8_t *) name.c_str(), name.size ());
	sha1_write (&s, (const uint8_t *) _script.c_str(), _script.size ());
	sha1_result_hash (&s, hash);
	return uri + hash;
}

std::string
LuaProc::presets_file () const
{
	return string_compose ("lua-%1", _info->unique_id);
}

XMLTree*
LuaProc::presets_tree () const
{
	XMLTree* t = new XMLTree;
	std::string p = Glib::build_filename (ARDOUR::user_config_directory (), "presets");

	if (!Glib::file_test (p, Glib::FILE_TEST_IS_DIR)) {
		if (g_mkdir_with_parents (p.c_str(), 0755) != 0) {
			error << _("Unable to create LuaProc presets directory") << endmsg;
		};
	}

	p = Glib::build_filename (p, presets_file ());

	if (!Glib::file_test (p, Glib::FILE_TEST_EXISTS)) {
		t->set_root (new XMLNode (X_("LuaPresets")));
		return t;
	}

	t->set_filename (p);
	if (!t->read ()) {
		delete t;
		return 0;
	}
	return t;
}

bool
LuaProc::load_preset (PresetRecord r)
{
	boost::shared_ptr<XMLTree> t (presets_tree ());
	if (t == 0) {
		return false;
	}

	XMLNode* root = t->root ();
	for (XMLNodeList::const_iterator i = root->children().begin(); i != root->children().end(); ++i) {
		std::string str;
		if (!(*i)->get_property (X_("label"), str)) {
			assert (false);
		}
		if (str != r.label) {
			continue;
		}

		for (XMLNodeList::const_iterator j = (*i)->children().begin(); j != (*i)->children().end(); ++j) {
			if ((*j)->name() == X_("Parameter")) {
				uint32_t index;
				float value;
				if (!(*j)->get_property (X_("index"), index) ||
				    !(*j)->get_property (X_("value"), value)) {
					assert (false);
				}
				set_parameter (index, value);
				PresetPortSetValue (index, value); /* EMIT SIGNAL */
			}
		}
		return Plugin::load_preset(r);
	}
	return false;
}

std::string
LuaProc::do_save_preset (std::string name) {

	boost::shared_ptr<XMLTree> t (presets_tree ());
	if (t == 0) {
		return "";
	}

	// prevent dups -- just in case
	t->root()->remove_nodes_and_delete (X_("label"), name);

	std::string uri (preset_name_to_uri (name));

	XMLNode* p = new XMLNode (X_("Preset"));
	p->set_property (X_("uri"), uri);
	p->set_property (X_("label"), name);

	for (uint32_t i = 0; i < parameter_count(); ++i) {
		if (parameter_is_input (i)) {
			XMLNode* c = new XMLNode (X_("Parameter"));
			c->set_property (X_("index"), i);
			c->set_property (X_("value"), get_parameter (i));
			p->add_child_nocopy (*c);
		}
	}
	t->root()->add_child_nocopy (*p);

	std::string f = Glib::build_filename (ARDOUR::user_config_directory (), "presets");
	f = Glib::build_filename (f, presets_file ());

	t->write (f);
	return uri;
}

void
LuaProc::do_remove_preset (std::string name)
{
	boost::shared_ptr<XMLTree> t (presets_tree ());
	if (t == 0) {
		return;
	}
	t->root()->remove_nodes_and_delete (X_("label"), name);
	std::string f = Glib::build_filename (ARDOUR::user_config_directory (), "presets");
	f = Glib::build_filename (f, presets_file ());
	t->write (f);
}

void
LuaProc::find_presets ()
{
	boost::shared_ptr<XMLTree> t (presets_tree ());
	if (t) {
		XMLNode* root = t->root ();
		for (XMLNodeList::const_iterator i = root->children().begin(); i != root->children().end(); ++i) {
			std::string uri;
			std::string label;

			if (!(*i)->get_property (X_("uri"), uri) || !(*i)->get_property (X_("label"), label)) {
				assert (false);
			}

			PresetRecord r (uri, label, true);
			_presets.insert (make_pair (r.uri, r));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

LuaPluginInfo::LuaPluginInfo (LuaScriptInfoPtr lsi) {
	if (lsi->type != LuaScriptInfo::DSP) {
		throw failed_constructor ();
	}

	path = lsi->path;
	name = lsi->name;
	creator = lsi->author;
	category = lsi->category;
	unique_id = lsi->unique_id;

	n_inputs.set (DataType::AUDIO, 1);
	n_outputs.set (DataType::AUDIO, 1);
	type = Lua;

	// TODO, parse script, get 'dsp_ioconfig', see can_support_io_configuration()
	_max_outputs = 0;
}

PluginPtr
LuaPluginInfo::load (Session& session)
{
	std::string script = "";
	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		return PluginPtr ();
	}

	try {
		script = Glib::file_get_contents (path);
	} catch (Glib::FileError const& err) {
		return PluginPtr ();
	}

	if (script.empty ()) {
		return PluginPtr ();
	}

	try {
		LuaProc* lp = new LuaProc (session.engine (), session, script);
		lp->set_origin (path);
		PluginPtr plugin (lp);
		return plugin;
	} catch (failed_constructor& err) {
		;
	}
	return PluginPtr ();
}

std::vector<Plugin::PresetRecord>
LuaPluginInfo::get_presets (bool /*user_only*/) const
{
	std::vector<Plugin::PresetRecord> p;
	XMLTree* t = new XMLTree;
	std::string pf = Glib::build_filename (ARDOUR::user_config_directory (), "presets", string_compose ("lua-%1", unique_id));
	if (Glib::file_test (pf, Glib::FILE_TEST_EXISTS)) {
		t->set_filename (pf);
		if (t->read ()) {
			XMLNode* root = t->root ();
			for (XMLNodeList::const_iterator i = root->children().begin(); i != root->children().end(); ++i) {
				XMLProperty const * uri = (*i)->property (X_("uri"));
				XMLProperty const * label = (*i)->property (X_("label"));
				p.push_back (Plugin::PresetRecord (uri->value(), label->value(), true));
			}
		}
	}
	delete t;
	return p;
}
