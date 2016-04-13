/*
    Copyright (C) 2016 Robin Gareus <robin@gareus.org>
    Copyright (C) 2006 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include "pbd/pthread_utils.h"

#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/luabindings.h"
#include "ardour/luaproc.h"
#include "ardour/luascripting.h"
#include "ardour/midi_buffer.h"
#include "ardour/plugin.h"
#include "ardour/session.h"

#include "LuaBridge/LuaBridge.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

LuaProc::LuaProc (AudioEngine& engine,
                  Session& session,
                  const std::string &script)
	: Plugin (engine, session)
	, _mempool ("LuaProc", 1048576) // 1 MB is plenty. (64K would be enough)
	, lua (lua_newstate (&PBD::ReallocPool::lalloc, &_mempool))
	, _lua_dsp (0)
	, _script (script)
	, _lua_does_channelmapping (false)
	, _lua_has_inline_display (false)
	, _control_data (0)
	, _shadow_data (0)
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
	, _mempool ("LuaProc", 1048576) // 1 MB is plenty. (64K would be enough)
	, lua (lua_newstate (&PBD::ReallocPool::lalloc, &_mempool))
	, _lua_dsp (0)
	, _script (other.script ())
	, _lua_does_channelmapping (false)
	, _lua_has_inline_display (false)
	, _control_data (0)
	, _shadow_data (0)
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
		printf ("LuaProc: '%s' run()  avg: %.3f  max: %.3f [ms]\n",
				_info->name.c_str (),
				0.0001f * _stats_avg[0] / (float) _stats_cnt,
				0.0001f * _stats_max[0]);
		printf ("LuaProc: '%s' gc()   avg: %.3f  max: %.3f [ms]\n",
				_info->name.c_str (),
				0.0001f * _stats_avg[1] / (float) _stats_cnt,
				0.0001f * _stats_max[1]);
	}
#endif
	lua.do_command ("collectgarbage();");
	delete (_lua_dsp);
	delete [] _control_data;
	delete [] _shadow_data;
}

void
LuaProc::init ()
{
#ifdef WITH_LUAPROC_STATS
	_stats_avg[0] = _stats_avg[1] = _stats_max[0] = _stats_max[1] = _stats_cnt = 0;
#endif

#ifndef NDEBUG
	lua.Print.connect (sigc::mem_fun (*this, &LuaProc::lua_print));
#endif
	// register session object
	lua_State* L = lua.getState ();
	LuaBindings::stddef (L);
	LuaBindings::common (L);
	LuaBindings::dsp (L);

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Ardour")
		.beginClass <LuaProc> ("LuaProc")
		.addFunction ("queue_draw", &LuaProc::queue_draw)
		.addFunction ("shmem", &LuaProc::instance_shm)
		.endClass ()
		.endNamespace ();

	// add session to global lua namespace
	luabridge::push <Session *> (L, &_session);
	lua_setglobal (L, "Session");

	// instance
	luabridge::push <LuaProc *> (L, this);
	lua_setglobal (L, "self");

	// sandbox
	lua.do_command ("io = nil os = nil loadfile = nil require = nil dofile = nil package = nil debug = nil");
#if 0
	lua.do_command ("for n in pairs(_G) do print(n) end print ('----')"); // print global env
#endif
	lua.do_command ("function ardour () end");
}

void
LuaProc::lua_print (std::string s) {
	std::cout <<"LuaProc: " << s << "\n";
}

bool
LuaProc::load_script ()
{
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
	}

	// initialize the DSP if needed
	luabridge::LuaRef lua_dsp_init = luabridge::getGlobal (L, "dsp_init");
	if (lua_dsp_init.type () == LUA_TFUNCTION) {
		try {
			lua_dsp_init (_session.nominal_frame_rate ());
		} catch (luabridge::LuaException const& e) {
			;
		}
	}

	luabridge::LuaRef lua_dsp_midi_in = luabridge::getGlobal (L, "dsp_midi_input");
	if (lua_dsp_midi_in.type () == LUA_TFUNCTION) {
		try {
			_has_midi_input = lua_dsp_midi_in ();
		} catch (luabridge::LuaException const& e) {
			;
		}
	}
	lpi->_is_instrument = _has_midi_input;

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
				if (!i.key ().isNumber ())           { return false; }
				if (!i.value ().isTable ())          { return false; }
				if (!i.value ()["type"].isString ()) { return false; }
				if (!i.value ()["name"].isString ()) { return false; }
				if (!i.value ()["min"].isNumber ())  { return false; }
				if (!i.value ()["max"].isNumber ())  { return false; }

				int pn = i.key ().cast<int> ();
				std::string type = i.value ()["type"].cast<std::string> ();
				if (type == "input") {
					if (!i.value ()["default"].isNumber ()) { return false; }
					_ctrl_params.push_back (std::make_pair (false, pn));
				}
				else if (type == "output") {
					_ctrl_params.push_back (std::make_pair (true, pn));
				} else {
					return false;
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

	if (in.n_midi() > 0 && !_has_midi_input && !imprecise) {
		return false;
	}

	lua_State* L = lua.getState ();
	luabridge::LuaRef ioconfig = luabridge::getGlobal (L, "dsp_ioconfig");
	if (!ioconfig.isFunction ()) {
		return false;
	}

	luabridge::LuaRef table = luabridge::getGlobal (L, "table"); //lua std lib
	luabridge::LuaRef tablesort = table["sort"];
	assert (tablesort.isFunction ());

	luabridge::LuaRef *_iotable = NULL; // can't use reference :(
	try {
		luabridge::LuaRef iotable = ioconfig ();
		tablesort (iotable);
		if (iotable.isTable ()) {
			_iotable = new luabridge::LuaRef (iotable);
		}
	} catch (luabridge::LuaException const& e) {
		return false;
	}

	if (!_iotable) {
		return false;
	}

	// now we can reference it.
	luabridge::LuaRef iotable (*_iotable);
	delete _iotable;

	if ((iotable).length () < 1) {
		return false;
	}

	const int32_t audio_in = in.n_audio ();
	int32_t audio_out;
	int32_t midi_out = 0; // TODO handle  _has_midi_output

	if (in.n_midi() > 0 && audio_in == 0) {
		audio_out = 2; // prefer stereo version if available.
	} else {
		audio_out = audio_in;
	}


	for (luabridge::Iterator i (iotable); !i.isNil (); ++i) {
		assert (i.value ().type () == LUA_TTABLE);
		luabridge::LuaRef io (i.value ());

		int possible_in = io["audio_in"];
		int possible_out = io["audio_out"];

		// exact match
		if ((possible_in == audio_in) && (possible_out == audio_out)) {
			out.set (DataType::MIDI, 0);
			out.set (DataType::AUDIO, audio_out);
			return true;
		}
	}

	/* now allow potentially "imprecise" matches */
	audio_out = -1;
	bool found = false;

	for (luabridge::Iterator i (iotable); !i.isNil (); ++i) {
		assert (i.value ().type () == LUA_TTABLE);
		luabridge::LuaRef io (i.value ());

		int possible_in = io["audio_in"];
		int possible_out = io["audio_out"];

		if (possible_out == 0) {
			continue;
		}
		if (possible_in == 0) {
			/* no inputs, generators & instruments */
			if (possible_out == -1) {
				/* any configuration possible, provide stereo output */
				audio_out = 2;
				found = true;
			} else if (possible_out == -2) {
				/* invalid, should be (0, -1) */
				audio_out = 2;
				found = true;
			} else if (possible_out < -2) {
				/* variable number of outputs. -> whatever */
				audio_out = 2;
				found = true;
			} else {
				/* exact number of outputs */
				audio_out = possible_out;
				found = true;
			}
		}

		if (possible_in == -1) {
			/* wildcard for input */
			if (possible_out == -1) {
				/* out must match in */
				audio_out = audio_in;
				found = true;
			} else if (possible_out == -2) {
				/* any configuration possible, pick matching */
				audio_out = audio_in;
				found = true;
			} else if (possible_out < -2) {
				/* explicitly variable number of outputs, pick maximum */
				audio_out = -possible_out;
				found = true;
			} else {
				/* exact number of outputs */
				audio_out = possible_out;
				found = true;
			}
		}

		if (possible_in == -2) {

			if (possible_out == -1) {
				/* any configuration possible, pick matching */
				audio_out = audio_in;
				found = true;
			} else if (possible_out == -2) {
				/* invalid. interpret as (-1, -1) */
				audio_out = audio_in;
				found = true;
			} else if (possible_out < -2) {
				/* explicitly variable number of outputs, pick maximum */
				audio_out = -possible_out;
				found = true;
			} else {
				/* exact number of outputs */
				audio_out = possible_out;
				found = true;
			}
		}

		if (possible_in < -2) {
			/* explicit variable number of inputs */
			if (audio_in > -possible_in && imprecise != NULL) {
				// hide inputs ports
				imprecise->set (DataType::AUDIO, -possible_in);
			}

			if (audio_in > -possible_in && imprecise == NULL) {
				/* request is too large */
			} else if (possible_out == -1) {
				/* any output configuration possible, provide stereo out */
				audio_out = 2;
				found = true;
			} else if (possible_out == -2) {
				/* invalid. interpret as (<-2, -1) */
				audio_out = 2;
				found = true;
			} else if (possible_out < -2) {
				/* explicitly variable number of outputs, pick stereo */
				audio_out = 2;
				found = true;
			} else {
				/* exact number of outputs */
				audio_out = possible_out;
				found = true;
			}
		}

		if (possible_in && (possible_in == audio_in)) {
			/* exact number of inputs ... must match obviously */
			if (possible_out == -1) {
				/* any output configuration possible, provide stereo output */
				audio_out = 2;
				found = true;
			} else if (possible_out == -2) {
				/* invalid. interpret as (>0, -1) */
				audio_out = 2;
				found = true;
			} else if (possible_out < -2) {
				/* explicitly variable number of outputs, pick maximum */
				audio_out = -possible_out;
				found = true;
			} else {
				/* exact number of outputs */
				audio_out = possible_out;
				found = true;
			}
		}

		if (found) {
			break;
		}
	}

	if (!found && imprecise) {
		/* try harder */
		for (luabridge::Iterator i (iotable); !i.isNil (); ++i) {
			assert (i.value ().type () == LUA_TTABLE);
			luabridge::LuaRef io (i.value ());

			int possible_in = io["audio_in"];
			int possible_out = io["audio_out"];

			assert (possible_in > 0); // all other cases will have been matched above
			assert (possible_out !=0 || possible_in !=0); // already handled above

			imprecise->set (DataType::AUDIO, possible_in);
			if (possible_out == -1 || possible_out == -2) {
				audio_out = 2;
				found = true;
			} else if (possible_out < -2) {
				/* explicitly variable number of outputs, pick maximum */
				audio_out = -possible_out;
				found = true;
			} else {
				/* exact number of outputs */
				audio_out = possible_out;
				found = true;
			}

			if (found) {
				// ideally we'll keep iterating and take the "best match"
				// whatever "best" means:
				// least unconnected inputs, least silenced inputs,
				// closest match of inputs == outputs
				break;
			}
		}
	}


	if (!found) {
		return false;
	}

	out.set (DataType::MIDI, midi_out); // currently always zero
	out.set (DataType::AUDIO, audio_out);
	return true;
}

bool
LuaProc::configure_io (ChanCount in, ChanCount out)
{
	_configured_in = in;
	_configured_out = out;

	_configured_in.set (DataType::MIDI, _has_midi_input ? 1 : 0);
	_configured_out.set (DataType::MIDI, _has_midi_output ? 1 : 0);

	// configure the DSP if needed
	lua_State* L = lua.getState ();
	luabridge::LuaRef lua_dsp_configure = luabridge::getGlobal (L, "dsp_configure");
	if (lua_dsp_configure.type () == LUA_TFUNCTION) {
		try {
			lua_dsp_configure (&in, &out);
		} catch (luabridge::LuaException const& e) {
			return false;
		}
	}

	_info->n_inputs = _configured_in;
	_info->n_outputs = _configured_out;
	return true;
}

int
LuaProc::connect_and_run (BufferSet& bufs,
		ChanMapping in, ChanMapping out,
		pframes_t nframes, framecnt_t offset)
{
	if (!_lua_dsp) {
		return 0;
	}

	Plugin::connect_and_run (bufs, in, out, nframes, offset);

	// This is needed for ARDOUR::Session requests :(
	if (! SessionEvent::has_per_thread_pool ()) {
		char name[64];
		snprintf (name, 64, "Proc-%p", this);
		pthread_set_name (name);
		SessionEvent::create_per_thread_pool (name, 64);
		PBD::notify_event_loops_about_thread_creation (pthread_self(), name, 64);
	}

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
			(*_lua_dsp)(&bufs, in, out, nframes, offset);
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

			luabridge::LuaRef lua_midi_tbl (luabridge::newTable (L));
			int e = 1; // > 1 port, we merge events (unsorted)
			for (uint32_t mp = 0; mp < midi_in; ++mp) {
				bool valid;
				const uint32_t idx = in.get(DataType::MIDI, mp, &valid);
				if (valid) {
					for (MidiBuffer::iterator m = bufs.get_midi(idx).begin();
							m != bufs.get_midi(idx).end(); ++m, ++e) {
						const Evoral::MIDIEvent<framepos_t> ev(*m, false);
						luabridge::LuaRef lua_midi_data (luabridge::newTable (L));
						const uint8_t* data = ev.buffer();
						for (uint32_t i = 0; i < ev.size(); ++i) {
							lua_midi_data [i + 1] = data[i];
						}
						luabridge::LuaRef lua_midi_event (luabridge::newTable (L));
						lua_midi_event["time"] = 1 + (*m).time();
						lua_midi_event["data"] = lua_midi_data;
						lua_midi_tbl[e] = lua_midi_event;
					}
				}
			}

			if (_has_midi_input) {
				// XXX TODO This needs a better solution than global namespace
				luabridge::push (L, lua_midi_tbl);
				lua_setglobal (L, "mididata");
			}


			// run the DSP function
			(*_lua_dsp)(in_map, out_map, nframes);
		}
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		printf ("LuaException: %s\n", e.what ());
#endif
		return -1;
	}
#ifdef WITH_LUAPROC_STATS
	int64_t t1 = g_get_monotonic_time ();
#endif
	lua.collect_garbage (); // rt-safe, slight *regular* performance overhead
#ifdef WITH_LUAPROC_STATS
	++_stats_cnt;
	int64_t t2 = g_get_monotonic_time ();
	int64_t ela0 = t1 - t0;
	int64_t ela1 = t2 - t1;
	if (ela0 > _stats_max[0]) _stats_max[0] = ela0;
	if (ela1 > _stats_max[1]) _stats_max[1] = ela1;
	_stats_avg[0] += ela0;
	_stats_avg[1] += ela1;
#endif
	return 0;
}


void
LuaProc::add_state (XMLNode* root) const
{
	XMLNode*    child;
	char        buf[32];
	LocaleGuard lg(X_("C"));

	gchar* b64 = g_base64_encode ((const guchar*)_script.c_str (), _script.size ());
	std::string b64s (b64);
	g_free (b64);
	XMLNode* script_node = new XMLNode (X_("script"));
	script_node->add_property (X_("lua"), LUA_VERSION);
	script_node->add_content (b64s);
	root->add_child_nocopy (*script_node);

	for (uint32_t i = 0; i < parameter_count(); ++i) {
		if (parameter_is_input(i) && parameter_is_control(i)) {
			child = new XMLNode("Port");
			snprintf(buf, sizeof(buf), "%u", i);
			child->add_property("id", std::string(buf));
			snprintf(buf, sizeof(buf), "%+f", _shadow_data[i]);
			child->add_property("value", std::string(buf));
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
#ifndef NO_PLUGIN_STATE
	XMLNodeList nodes;
	XMLProperty *prop;
	XMLNodeConstIterator iter;
	XMLNode *child;
	const char *value;
	const char *port;
	uint32_t port_id;
#endif
	LocaleGuard lg (X_("C"));

	if (_script.empty ()) {
		if (set_script_from_state (node)) {
			return -1;
		}
	}

#ifndef NO_PLUGIN_STATE
	if (node.name() != state_node_name()) {
		error << _("Bad node sent to LuaProc::set_state") << endmsg;
		return -1;
	}

	nodes = node.children ("Port");
	for (iter = nodes.begin(); iter != nodes.end(); ++iter) {
		child = *iter;
		if ((prop = child->property("id")) != 0) {
			port = prop->value().c_str();
		} else {
			warning << _("LuaProc: port has no symbol, ignored") << endmsg;
			continue;
		}
		if ((prop = child->property("value")) != 0) {
			value = prop->value().c_str();
		} else {
			warning << _("LuaProc: port has no value, ignored") << endmsg;
			continue;
		}
		sscanf (port, "%" PRIu32, &port_id);
		set_parameter (port_id, atof(value));
	}
#endif

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

void
LuaProc::print_parameter (uint32_t param, char* buf, uint32_t len) const
{
	if (buf && len) {
		if (param < parameter_count ()) {
			snprintf (buf, len, "%.3f", get_parameter (param));
		} else {
			strcat (buf, "0");
		}
	}
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

#ifndef NDEBUG
	lua_gui->Print.connect (sigc::mem_fun (*this, &LuaProc::lua_print));
#endif

	lua_gui->do_command ("function ardour () end");
	lua_gui->do_command (_script);

	// TODO think: use a weak-pointer here ?
	// (the GUI itself uses a shared ptr to this plugin, so we should be good)
	luabridge::getGlobalNamespace (LG)
		.beginNamespace ("Ardour")
		.beginClass <LuaProc> ("LuaProc")
		.addFunction ("shmem", &LuaProc::instance_shm)
		.endClass ()
		.endNamespace ();

	luabridge::push <LuaProc *> (LG, this);
	lua_setglobal (LG, "self");

	luabridge::push <float *> (LG, _shadow_data);
	lua_setglobal (LG, "CtrlPorts");
}

////////////////////////////////////////////////////////////////////////////////
#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

LuaPluginInfo::LuaPluginInfo (LuaScriptInfoPtr lsi) {
	if (lsi->type != LuaScriptInfo::DSP) {
		throw failed_constructor ();
	}

	path = lsi->path;
	name = lsi->name;
	creator = lsi->author;
	category = lsi->category;
	unique_id = "luascript"; // the interpreter is not unique.

	n_inputs.set (DataType::AUDIO, 1);
	n_outputs.set (DataType::AUDIO, 1);
	type = Lua;
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
	} catch (Glib::FileError err) {
		return PluginPtr ();
	}

	if (script.empty ()) {
		return PluginPtr ();
	}

	try {
		PluginPtr plugin (new LuaProc (session.engine (), session, script));
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
	return p;
}
