/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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
#include <cstring>
#include <vamp-hostsdk/PluginLoader.h>

#include "pbd/basename.h"
#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"

#include "ardour/analyser.h"
#include "ardour/audioengine.h"
#include "ardour/audiofilesource.h"
#include "ardour/audiosource.h"
#include "ardour/internal_send.h"
#include "ardour/lua_api.h"
#include "ardour/luaproc.h"
#include "ardour/luascripting.h"
#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/plugin_manager.h"
#include "ardour/readable.h"
#include "ardour/region_factory.h"
#include "ardour/source_factory.h"

#include "LuaBridge/LuaBridge.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

int
ARDOUR::LuaAPI::datatype_ctor_null (lua_State *L)
{
	DataType dt (DataType::NIL);
	luabridge::Stack <DataType>::push (L, dt);
	return 1;
}

int
ARDOUR::LuaAPI::datatype_ctor_audio (lua_State *L)
{
	DataType dt (DataType::AUDIO);
	// NB luabridge will copy construct the object and manage lifetime.
	luabridge::Stack <DataType>::push (L, dt);
	return 1;
}

int
ARDOUR::LuaAPI::datatype_ctor_midi (lua_State *L)
{
	DataType dt (DataType::MIDI);
	luabridge::Stack <DataType>::push (L, dt);
	return 1;
}

boost::shared_ptr<Processor>
ARDOUR::LuaAPI::nil_processor ()
{
	return boost::shared_ptr<Processor> ();
}

boost::shared_ptr<Processor>
ARDOUR::LuaAPI::new_luaproc (Session *s, const string& name)
{
	if (!s) {
		return boost::shared_ptr<Processor> ();
	}

	LuaScriptInfoPtr spi;
	ARDOUR::LuaScriptList & _scripts (LuaScripting::instance ().scripts (LuaScriptInfo::DSP));
	for (LuaScriptList::const_iterator i = _scripts.begin (); i != _scripts.end (); ++i) {
		if (name == (*i)->name) {
			spi = *i;
			break;
		}
	}

	if (!spi) {
		warning << _("Script with given name was not found\n");
		return boost::shared_ptr<Processor> ();
	}

	PluginPtr p;
	try {
		LuaPluginInfoPtr lpi (new LuaPluginInfo (spi));
		p = (lpi->load (*s));
	} catch (...) {
		warning << _("Failed to instantiate Lua Processor\n");
		return boost::shared_ptr<Processor> ();
	}

	return boost::shared_ptr<Processor> (new PluginInsert (*s, p));
}

boost::shared_ptr<Processor>
ARDOUR::LuaAPI::new_send (Session* s, boost::shared_ptr<Route> r, boost::shared_ptr<Processor> before)
{
	if (!s) {
		return boost::shared_ptr<Processor> ();
	}

	boost::shared_ptr<Send> send (new Send (*s, r->pannable (), r->mute_master ()));

	/* make an educated guess at the initial number of outputs for the send */
	ChanCount outs = before ? before->input_streams () : r->n_outputs();

	try {
		Glib::Threads::Mutex::Lock lm (AudioEngine::instance ()->process_lock ());
		send->output()->ensure_io (outs, false, r.get());
	} catch (AudioEngine::PortRegistrationFailure& err) {
		error << string_compose (_("Cannot set up new send: %1"), err.what ()) << endmsg;
		return boost::shared_ptr<Processor> ();
	}

	if (0 == r->add_processor (send, before)) {
		return send;
	}

	return boost::shared_ptr<Processor> ();
}

std::string
ARDOUR::LuaAPI::dump_untagged_plugins ()
{
	PluginManager& manager = PluginManager::instance ();
	return manager.dump_untagged_plugins();
}

PluginInfoList
ARDOUR::LuaAPI::list_plugins ()
{
	PluginManager& manager = PluginManager::instance ();
	PluginInfoList all_plugs;
	all_plugs.insert (all_plugs.end (), manager.ladspa_plugin_info ().begin (), manager.ladspa_plugin_info ().end ());
	all_plugs.insert (all_plugs.end (), manager.lua_plugin_info ().begin (), manager.lua_plugin_info ().end ());
#ifdef WINDOWS_VST_SUPPORT
	all_plugs.insert (all_plugs.end (), manager.windows_vst_plugin_info ().begin (), manager.windows_vst_plugin_info ().end ());
#endif
#ifdef MACVST_SUPPORT
	all_plugs.insert (all_plugs.end (), manager.mac_vst_plugin_info ().begin (), manager.mac_vst_plugin_info ().end ());
#endif
#ifdef LXVST_SUPPORT
	all_plugs.insert (all_plugs.end (), manager.lxvst_plugin_info ().begin (), manager.lxvst_plugin_info ().end ());
#endif
#ifdef VST3_SUPPORT
	all_plugs.insert (all_plugs.end (), manager.vst3_plugin_info ().begin (), manager.vst3_plugin_info ().end ());
#endif
#ifdef AUDIOUNIT_SUPPORT
	all_plugs.insert (all_plugs.end (), manager.au_plugin_info ().begin (), manager.au_plugin_info ().end ());
#endif
	all_plugs.insert (all_plugs.end (), manager.lv2_plugin_info ().begin (), manager.lv2_plugin_info ().end ());
	all_plugs.insert (all_plugs.end (), manager.lua_plugin_info ().begin (), manager.lua_plugin_info ().end ());

	return all_plugs;
}

PluginInfoPtr
ARDOUR::LuaAPI::new_plugin_info (const string& name, ARDOUR::PluginType type)
{
	PluginManager& manager = PluginManager::instance ();
	PluginInfoList all_plugs;
	all_plugs.insert (all_plugs.end (), manager.ladspa_plugin_info ().begin (), manager.ladspa_plugin_info ().end ());
	all_plugs.insert (all_plugs.end (), manager.lua_plugin_info ().begin (), manager.lua_plugin_info ().end ());
#ifdef WINDOWS_VST_SUPPORT
	all_plugs.insert (all_plugs.end (), manager.windows_vst_plugin_info ().begin (), manager.windows_vst_plugin_info ().end ());
#endif
#ifdef MACVST_SUPPORT
	all_plugs.insert (all_plugs.end (), manager.mac_vst_plugin_info ().begin (), manager.mac_vst_plugin_info ().end ());
#endif
#ifdef LXVST_SUPPORT
	all_plugs.insert (all_plugs.end (), manager.lxvst_plugin_info ().begin (), manager.lxvst_plugin_info ().end ());
#endif
#ifdef VST3_SUPPORT
	all_plugs.insert (all_plugs.end (), manager.vst3_plugin_info ().begin (), manager.vst3_plugin_info ().end ());
#endif
#ifdef AUDIOUNIT_SUPPORT
	all_plugs.insert (all_plugs.end (), manager.au_plugin_info ().begin (), manager.au_plugin_info ().end ());
#endif
	all_plugs.insert (all_plugs.end (), manager.lv2_plugin_info ().begin (), manager.lv2_plugin_info ().end ());
	all_plugs.insert (all_plugs.end (), manager.lua_plugin_info ().begin (), manager.lua_plugin_info ().end ());

	for (PluginInfoList::const_iterator i = all_plugs.begin (); i != all_plugs.end (); ++i) {
		if (((*i)->name == name || (*i)->unique_id == name) && (*i)->type == type) {
			return *i;
		}
	}
	return PluginInfoPtr ();
}

boost::shared_ptr<Processor>
ARDOUR::LuaAPI::new_plugin (Session *s, const string& name, ARDOUR::PluginType type, const string& preset)
{
	if (!s) {
		return boost::shared_ptr<Processor> ();
	}

	PluginInfoPtr pip = new_plugin_info (name, type);

	if (!pip) {
		return boost::shared_ptr<Processor> ();
	}

	PluginPtr p = pip->load (*s);
	if (!p) {
		return boost::shared_ptr<Processor> ();
	}

	if (!preset.empty ()) {
		const Plugin::PresetRecord *pr = p->preset_by_label (preset);
		if (pr) {
			p->load_preset (*pr);
		}
	}

	return boost::shared_ptr<Processor> (new PluginInsert (*s, p));
}

bool
ARDOUR::LuaAPI::set_plugin_insert_param (boost::shared_ptr<PluginInsert> pi, uint32_t which, float val)
{
	boost::shared_ptr<Plugin> plugin = pi->plugin ();
	if (!plugin) { return false; }

	bool ok=false;
	uint32_t controlid = plugin->nth_parameter (which, ok);
	if (!ok) { return false; }
	if (!plugin->parameter_is_input (controlid)) { return false; }

	ParameterDescriptor pd;
	if (plugin->get_parameter_descriptor (controlid, pd) != 0) { return false; }
	if (val < pd.lower || val > pd.upper) { return false; }

	boost::shared_ptr<AutomationControl> c = pi->automation_control (Evoral::Parameter (PluginAutomation, 0, controlid));
	c->set_value (val, PBD::Controllable::NoGroup);
	return true;
}

float
ARDOUR::LuaAPI::get_plugin_insert_param (boost::shared_ptr<PluginInsert> pi, uint32_t which, bool &ok)
{
	ok=false;
	boost::shared_ptr<Plugin> plugin = pi->plugin ();
	if (!plugin) { return 0; }
	uint32_t controlid = plugin->nth_parameter (which, ok);
	if (!ok) { return 0; }
	return plugin->get_parameter ( controlid );
}

bool
ARDOUR::LuaAPI::set_processor_param (boost::shared_ptr<Processor> proc, uint32_t which, float val)
{
	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (proc);
	if (!pi) { return false; }
	return set_plugin_insert_param (pi, which, val);
}

float
ARDOUR::LuaAPI::get_processor_param (boost::shared_ptr<Processor> proc, uint32_t which, bool &ok)
{
	ok=false;
	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (proc);
	if (!pi) { ok = false; return 0;}
	return get_plugin_insert_param (pi, which, ok);
}

bool
ARDOUR::LuaAPI::reset_processor_to_default ( boost::shared_ptr<Processor> proc )
{
	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (proc);
	if (pi) {
		pi->reset_parameters_to_default();
		return true;
	}
	return false;
}

int
ARDOUR::LuaAPI::plugin_automation (lua_State *L)
{
	typedef boost::shared_ptr<Processor> T;

	int top = lua_gettop (L);
	if (top < 2) {
		return luaL_argerror (L, 1, "invalid number of arguments, :plugin_automation (plugin, parameter_number)");
	}
	T* const p = luabridge::Userdata::get<T> (L, 1, false);
	uint32_t which = luabridge::Stack<uint32_t>::get (L, 2);
	if (!p) {
		return luaL_error (L, "Invalid pointer to Ardour:Processor");
	}
	boost::shared_ptr<PluginInsert> pi = boost::dynamic_pointer_cast<PluginInsert> (*p);
	if (!pi) {
		return luaL_error (L, "Given Processor is not a Plugin Insert");
	}
	boost::shared_ptr<Plugin> plugin = pi->plugin ();
	if (!plugin) {
		return luaL_error (L, "Given Processor is not a Plugin");
	}

	bool ok=false;
	uint32_t controlid = plugin->nth_parameter (which, ok);
	if (!ok) {
		return luaL_error (L, "Invalid Parameter");
	}
	if (!plugin->parameter_is_input (controlid)) {
		return luaL_error (L, "Given Parameter is not an input");
	}

	ParameterDescriptor pd;
	if (plugin->get_parameter_descriptor (controlid, pd) != 0) {
		return luaL_error (L, "Cannot describe parameter");
	}

	boost::shared_ptr<AutomationControl> c = pi->automation_control (Evoral::Parameter (PluginAutomation, 0, controlid));

	luabridge::Stack<boost::shared_ptr<AutomationList> >::push (L, c->alist ());
	luabridge::Stack<boost::shared_ptr<Evoral::ControlList> >::push (L, c->list ());
	luabridge::Stack<ParameterDescriptor>::push (L, pd);
	return 3;
}

int
ARDOUR::LuaAPI::desc_scale_points (lua_State *L)
{
	typedef ParameterDescriptor T;

	int top = lua_gettop (L);
	if (top < 1) {
		return luaL_argerror (L, 1, "invalid number of arguments, :plugin_scale_points (ParameterDescriptor)");
	}

	T* const pd = luabridge::Userdata::get<T> (L, 1, false);
	luabridge::LuaRef tbl (luabridge::newTable (L));

	if (pd && pd->scale_points) {
		for (ARDOUR::ScalePoints::const_iterator i = pd->scale_points->begin(); i != pd->scale_points->end(); ++i) {
			tbl[i->first] = i->second;
		}
	}
	luabridge::push (L, tbl);
	return 1;
}

int
ARDOUR::LuaAPI::sample_to_timecode (lua_State *L)
{
	int top = lua_gettop (L);
	if (top < 3) {
		return luaL_argerror (L, 1, "invalid number of arguments sample_to_timecode (TimecodeFormat, sample_rate, sample)");
	}
	typedef Timecode::TimecodeFormat T;
	T tf = luabridge::Stack<T>::get (L, 1);
	double sample_rate = luabridge::Stack<double>::get (L, 2);
	int64_t sample = luabridge::Stack<int64_t>::get (L, 3);

	Timecode::Time timecode;

	Timecode::sample_to_timecode (
			sample, timecode, false, false,
			Timecode::timecode_to_frames_per_second (tf),
			Timecode::timecode_has_drop_frames (tf),
			sample_rate,
			0, false, 0);

	luabridge::Stack<uint32_t>::push (L, timecode.hours);
	luabridge::Stack<uint32_t>::push (L, timecode.minutes);
	luabridge::Stack<uint32_t>::push (L, timecode.seconds);
	luabridge::Stack<uint32_t>::push (L, timecode.frames);
	return 4;
}

int
ARDOUR::LuaAPI::timecode_to_sample (lua_State *L)
{
	int top = lua_gettop (L);
	if (top < 6) {
		return luaL_argerror (L, 1, "invalid number of arguments sample_to_timecode (TimecodeFormat, sample_rate, hh, mm, ss, ff)");
	}
	typedef Timecode::TimecodeFormat T;
	T tf = luabridge::Stack<T>::get (L, 1);
	double sample_rate = luabridge::Stack<double>::get (L, 2);
	int hh = luabridge::Stack<int>::get (L, 3);
	int mm = luabridge::Stack<int>::get (L, 4);
	int ss = luabridge::Stack<int>::get (L, 5);
	int ff = luabridge::Stack<int>::get (L, 6);

	Timecode::Time timecode;
	timecode.negative = false;
	timecode.hours = hh;
	timecode.minutes = mm;
	timecode.seconds = ss;
	timecode.frames = ff;
	timecode.subframes = 0;
	timecode.rate = Timecode::timecode_to_frames_per_second (tf);
	timecode.drop = Timecode::timecode_has_drop_frames (tf);

	int64_t sample;

	Timecode::timecode_to_sample (
			timecode, sample, false, false,
			sample_rate, 0, false, 0);

	luabridge::Stack<int64_t>::push (L, sample);
	return 1;
}

int
ARDOUR::LuaAPI::sample_to_timecode_lua (lua_State *L)
{
	int top = lua_gettop (L);
	if (top < 2) {
		return luaL_argerror (L, 1, "invalid number of arguments sample_to_timecode (sample)");
	}
	Session const* const s = luabridge::Userdata::get <Session> (L, 1, true);
	int64_t sample = luabridge::Stack<int64_t>::get (L, 2);

	Timecode::Time timecode;

	Timecode::sample_to_timecode (
			sample, timecode, false, false,
			s->timecode_frames_per_second (),
			s->timecode_drop_frames (),
			s->sample_rate (),
			0, false, 0);

	luabridge::Stack<uint32_t>::push (L, timecode.hours);
	luabridge::Stack<uint32_t>::push (L, timecode.minutes);
	luabridge::Stack<uint32_t>::push (L, timecode.seconds);
	luabridge::Stack<uint32_t>::push (L, timecode.frames);
	return 4;
}
int
ARDOUR::LuaAPI::timecode_to_sample_lua (lua_State *L)
{
	int top = lua_gettop (L);
	if (top < 5) {
		return luaL_argerror (L, 1, "invalid number of arguments sample_to_timecode (hh, mm, ss, ff)");
	}
	Session const* const s = luabridge::Userdata::get <Session> (L, 1, true);
	int hh = luabridge::Stack<int>::get (L, 2);
	int mm = luabridge::Stack<int>::get (L, 3);
	int ss = luabridge::Stack<int>::get (L, 4);
	int ff = luabridge::Stack<int>::get (L, 5);

	Timecode::Time timecode;
	timecode.negative = false;
	timecode.hours = hh;
	timecode.minutes = mm;
	timecode.seconds = ss;
	timecode.frames = ff;
	timecode.subframes = 0;
	timecode.rate = s->timecode_frames_per_second ();
	timecode.drop = s->timecode_drop_frames ();

	int64_t sample;

	Timecode::timecode_to_sample (
			timecode, sample, false, false,
			s->sample_rate (),
			0, false, 0);

	luabridge::Stack<int64_t>::push (L, sample);
	return 1;
}


static
void proc_cycle_start (size_t* cnt)
{
	++*cnt;
}

bool
ARDOUR::LuaAPI::wait_for_process_callback (size_t n_cycles, int64_t timeout_ms)
{
	if (!AudioEngine::instance()->running()) {
		return false;
	}
#if 0
	if (AudioEngine::instance()->freewheeling()) {
		return false;
	}
#endif
	if (AudioEngine::instance()->measuring_latency() != AudioEngine::MeasureNone) {
		return false;
	}
	if (!AudioEngine::instance()->session() ) {
		return false;
	}

	size_t cnt = 0;
	ScopedConnection c;

	InternalSend::CycleStart.connect_same_thread (c, boost::bind (&proc_cycle_start, &cnt));
	while (cnt <= n_cycles) {
		Glib::usleep (1000);
		if (timeout_ms > 0) {
			if (--timeout_ms == 0) {
				return cnt > n_cycles;
			}
		}
	}
	return true;
}

void
ARDOUR::LuaAPI::segfault ()
{
	int* p = NULL;
	*p = 0;
}

int
ARDOUR::LuaOSC::Address::send (lua_State *L)
{
	Address * const luaosc = luabridge::Userdata::get <Address> (L, 1, false);
	if (!luaosc) {
		return luaL_error (L, "Invalid pointer to OSC.Address");
	}
	if (!luaosc->_addr) {
		return luaL_error (L, "Invalid Destination Address");
	}

	int top = lua_gettop (L);
	if (top < 3) {
		return luaL_argerror (L, 1, "invalid number of arguments, :send (path, type, ...)");
	}

	const char* path = luaL_checkstring (L, 2);
	const char* type = luaL_checkstring (L, 3);
	assert (path && type);

	if ((int) strlen (type) != top - 3) {
		return luaL_argerror (L, 3, "type description does not match arguments");
	}

	lo_message msg = lo_message_new ();

	for (int i = 4; i <= top; ++i) {
		char t = type[i - 4];
		int lt = lua_type (L, i);
		int ok = -1;
		switch (lt) {
			case LUA_TSTRING:
				if (t == LO_STRING) {
					ok = lo_message_add_string (msg, luaL_checkstring (L, i));
				} else if (t == LO_CHAR) {
					char c = luaL_checkstring (L, i) [0];
					ok = lo_message_add_char (msg, c);
				}
				break;
			case LUA_TBOOLEAN:
				if (t == LO_TRUE || t == LO_FALSE) {
					if (lua_toboolean (L, i)) {
						ok = lo_message_add_true (msg);
					} else {
						ok = lo_message_add_false (msg);
					}
				}
				break;
			case LUA_TNUMBER:
				if (t == LO_INT32) {
					ok = lo_message_add_int32 (msg, (int32_t) luaL_checkinteger (L, i));
				}
				else if (t == LO_FLOAT) {
					ok = lo_message_add_float (msg, (float) luaL_checknumber (L, i));
				}
				else if (t == LO_DOUBLE) {
					ok = lo_message_add_double (msg, (double) luaL_checknumber (L, i));
				}
				else if (t == LO_INT64) {
					ok = lo_message_add_double (msg, (int64_t) luaL_checknumber (L, i));
				}
				break;
			default:
				break;
		}
		if (ok != 0) {
			return luaL_argerror (L, i, "type description does not match parameter");
		}
	}

	int rv = lo_send_message (luaosc->_addr, path, msg);
	lo_message_free (msg);
	luabridge::Stack<bool>::push (L, (rv == 0));
	return 1;
}

static double hue2rgb (const double p, const double q, double t) {
	if (t < 0.0) t += 1.0;
	if (t > 1.0) t -= 1.0;
	if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
	if (t < 1.0 / 2.0) return q;
	if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
	return p;
}

int
ARDOUR::LuaAPI::hsla_to_rgba (lua_State *L)
{
	int top = lua_gettop (L);
	if (top < 3) {
		return luaL_argerror (L, 1, "invalid number of arguments, :hsla_to_rgba (h, s, l [,a])");
	}
	double h = luabridge::Stack<double>::get (L, 1);
	double s = luabridge::Stack<double>::get (L, 2);
	double l = luabridge::Stack<double>::get (L, 3);
	double a = 1.0;
	if (top > 3) {
		a = luabridge::Stack<double>::get (L, 4);
	}

	// we can't use Gtkmm2ext::hsva_to_color here
	// besides we want HSL not HSV and without intermediate
	// color_to_rgba (rgba_to_color ())
	double r, g, b;
	const double cq = l < 0.5 ? l * (1 + s) : l + s - l * s;
	const double cp = 2.f * l - cq;
	r = hue2rgb (cp, cq, h + 1.0 / 3.0);
	g = hue2rgb (cp, cq, h);
	b = hue2rgb (cp, cq, h - 1.0 / 3.0);

	luabridge::Stack<double>::push (L, r);
	luabridge::Stack<double>::push (L, g);
	luabridge::Stack<double>::push (L, b);
	luabridge::Stack<double>::push (L, a);
	return 4;
}

std::string
ARDOUR::LuaAPI::ascii_dtostr (const double d)
{
	gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_dtostr (buf, sizeof(buf), d);
	return std::string (buf);
}

int
ARDOUR::LuaAPI::color_to_rgba (lua_State *L)
{
	int top = lua_gettop (L);
	if (top < 1) {
		return luaL_argerror (L, 1, "invalid number of arguments, color_to_rgba (uint32_t)");
	}
	uint32_t color = luabridge::Stack<uint32_t>::get (L, 1);
	double r, g, b, a;

	/* libardour is no user of libcanvas, otherwise
	 * we could just call
	 * Gtkmm2ext::color_to_rgba (color, r, g, b, a);
	 */
	r = ((color >> 24) & 0xff) / 255.0;
	g = ((color >> 16) & 0xff) / 255.0;
	b = ((color >>  8) & 0xff) / 255.0;
	a = ((color >>  0) & 0xff) / 255.0;

	luabridge::Stack <double>::push (L, r);
	luabridge::Stack <double>::push (L, g);
	luabridge::Stack <double>::push (L, b);
	luabridge::Stack <double>::push (L, a);
	return 4;
}

int
ARDOUR::LuaAPI::build_filename (lua_State *L)
{
	std::vector<std::string> elem;
	int top = lua_gettop (L);
	if (top < 1) {
		return luaL_argerror (L, 1, "invalid number of arguments, build_filename (path, ...)");
	}
	for (int i = 1; i <= top; ++i) {
		int lt = lua_type (L, i);
		if (lt != LUA_TSTRING) {
			return luaL_argerror (L, i, "invalid argument type, expected string");
		}
		elem.push_back (luaL_checkstring (L, i));
	}

	luabridge::Stack<std::string>::push (L, Glib::build_filename (elem));
	return 1;
}

luabridge::LuaRef::Proxy&
luabridge::LuaRef::Proxy::clone_instance (const void* classkey, void* p) {
	lua_rawgeti (m_L, LUA_REGISTRYINDEX, m_tableRef);
	lua_rawgeti (m_L, LUA_REGISTRYINDEX, m_keyRef);

	luabridge::UserdataPtr::push_raw (m_L, p, classkey);

	lua_rawset (m_L, -3);
	lua_pop (m_L, 1);
	return *this;
}

LuaTableRef::LuaTableRef () {}
LuaTableRef::~LuaTableRef () {}

int
LuaTableRef::get (lua_State* L)
{
	luabridge::LuaRef rv (luabridge::newTable (L));
	for (std::vector<LuaTableEntry>::const_iterator i = _data.begin (); i != _data.end (); ++i) {
		switch ((*i).keytype) {
			case LUA_TSTRING:
				assign (&rv, i->k_s, *i);
				break;
			case LUA_TNUMBER:
				assign (&rv, i->k_n, *i);
				break;
		}
	}
	luabridge::push (L, rv);
	return 1;
}

int
LuaTableRef::set (lua_State* L)
{
	if (!lua_istable (L, -1)) { return luaL_error (L, "argument is not a table"); }
	_data.clear ();

	lua_pushvalue (L, -1);
	lua_pushnil (L);
	while (lua_next (L, -2)) {
		lua_pushvalue (L, -2);

		LuaTableEntry s (lua_type (L, -1), lua_type (L, -2));
		switch (lua_type (L, -1)) {
			case LUA_TSTRING:
				s.k_s = luabridge::Stack<std::string>::get (L, -1);
				break;
				;
			case LUA_TNUMBER:
				s.k_n = luabridge::Stack<unsigned int>::get (L, -1);
				break;
			default:
				// invalid key
				lua_pop (L, 2);
				continue;
		}

		switch (lua_type (L, -2)) {
			case LUA_TSTRING:
				s.s = luabridge::Stack<std::string>::get (L, -2);
				break;
			case LUA_TBOOLEAN:
				s.b = lua_toboolean (L, -2);
				break;
			case LUA_TNUMBER:
				s.n = lua_tonumber (L, -2);
				break;
			case LUA_TUSERDATA:
				{
					bool ok = false;
					lua_getmetatable (L, -2);
					lua_rawgetp (L, -1, luabridge::getIdentityKey ());
					if (lua_isboolean (L, -1)) {
						lua_pop (L, 1);
						const void* key = lua_topointer (L, -1);
						lua_pop (L, 1);
						void const* classkey = findclasskey (L, key);

						if (classkey) {
							ok = true;
							s.c = classkey;
							s.p = luabridge::Userdata::get_ptr (L, -2);
						}
					} else {
						lua_pop (L, 2);
					}

					if (ok) {
						break;
					}
					// invalid userdata -- fall through
				}
				/* fallthrough */
			case LUA_TFUNCTION: // no support -- we could... string.format("%q", string.dump(value, true))
				/* fallthrough */
			case LUA_TTABLE: // no nested tables, sorry.
			case LUA_TNIL:
			default:
				// invalid value
				lua_pop (L, 2);
				continue;
		}

		_data.push_back (s);
		lua_pop (L, 2);
	}
	return 0;
}

void*
LuaTableRef::findclasskey (lua_State *L, const void* key)
{
	lua_pushvalue (L, LUA_REGISTRYINDEX);
	lua_pushnil (L);
	while (lua_next (L, -2)) {
		lua_pushvalue (L, -2);
		if (lua_topointer (L, -2) == key) {
			void* rv = lua_touserdata (L, -1);
			lua_pop (L, 4);
			return rv;
		}
		lua_pop (L, 2);
	}
	lua_pop (L, 1);
	return NULL;
}

template<typename T>
void LuaTableRef::assign (luabridge::LuaRef* rv, T key, const LuaTableEntry& s)
{
	switch (s.valuetype) {
		case LUA_TSTRING:
			(*rv)[key] = s.s;
			break;
		case LUA_TBOOLEAN:
			(*rv)[key] = s.b;
			break;
		case LUA_TNUMBER:
			(*rv)[key] = s.n;
			break;
		case LUA_TUSERDATA:
			(*rv)[key].clone_instance (s.c, s.p);
			break;
		default:
			assert (0);
			break;
	}
}

std::vector<std::string>
LuaAPI::Vamp::list_plugins ()
{
	using namespace ::Vamp::HostExt;
	PluginLoader* loader (PluginLoader::getInstance());
	return loader->listPlugins ();
}

LuaAPI::Vamp::Vamp (const std::string& key, float sample_rate)
	: _plugin (0)
	, _sample_rate (sample_rate)
	, _bufsize (1024)
	, _stepsize (1024)
	, _initialized (false)
{
	using namespace ::Vamp::HostExt;

	PluginLoader* loader (PluginLoader::getInstance());
	_plugin = loader->loadPlugin (key, _sample_rate, PluginLoader::ADAPT_ALL_SAFE);

	if (!_plugin) {
		PBD::error << string_compose (_("VAMP Plugin \"%1\" could not be loaded"), key) << endmsg;
		throw failed_constructor ();
	}

	size_t bs = _plugin->getPreferredBlockSize ();
	size_t ss = _plugin->getPreferredStepSize ();

	if (bs > 0 && ss > 0 && bs <= 8192 && ss <= 8192) {
		_bufsize = bs;
		_stepsize = ss;
	}
}

LuaAPI::Vamp::~Vamp ()
{
	delete _plugin;
}

void
LuaAPI::Vamp::reset ()
{
	_initialized = false;
	if (_plugin) {
		_plugin->reset ();
	}
}

bool
LuaAPI::Vamp::initialize ()
{
	if (!_plugin || _plugin->getMinChannelCount() > 1) {
		return false;
	}
	if (!_plugin->initialise (1, _stepsize, _bufsize)) {
		return false;
	}
	_initialized = true;
	return true;
}

int
LuaAPI::Vamp::analyze (boost::shared_ptr<ARDOUR::AudioReadable> r, uint32_t channel, luabridge::LuaRef cb)
{
	if (!_initialized) {
		if (!initialize ()) {
			return -1;
		}
	}
	assert (_initialized);

	::Vamp::Plugin::FeatureSet features;
	float* data = new float[_bufsize];
	float* bufs[1] = { data };

	samplecnt_t len = r->readable_length_samples();
	samplepos_t pos = 0;

	int rv = 0;
	while (1) {
		samplecnt_t to_read = std::min ((len - pos), _bufsize);
		if (r->read (data, pos, to_read, channel) != to_read) {
			rv = -1;
			break;
		}
		if (to_read != _bufsize) {
			memset (data + to_read, 0, (_bufsize - to_read) * sizeof (float));
		}

		features = _plugin->process (bufs, ::Vamp::RealTime::fromSeconds ((double) pos / _sample_rate));

		if (cb.type () == LUA_TFUNCTION) {
			if (cb (&features, pos)) {
				break;
			}
		}

		pos += std::min (_stepsize, to_read);

		if (pos >= len) {
			break;
		}
	}

	delete [] data;
	return rv;
}

::Vamp::Plugin::FeatureSet
LuaAPI::Vamp::process (const std::vector<float*>& d, ::Vamp::RealTime rt)
{
	if (!_plugin || d.size() == 0) {
		return ::Vamp::Plugin::FeatureSet ();
	}
	const float* const* bufs = &d[0];
	return _plugin->process (bufs, rt);
}

boost::shared_ptr<Evoral::Note<Temporal::Beats> >
LuaAPI::new_noteptr (uint8_t chan, Temporal::Beats beat_time, Temporal::Beats length, uint8_t note, uint8_t velocity)
{
	return boost::shared_ptr<Evoral::Note<Temporal::Beats> > (new Evoral::Note<Temporal::Beats>(chan, beat_time, length, note, velocity));
}

std::list<boost::shared_ptr<Evoral::Note<Temporal::Beats> > >
LuaAPI::note_list (boost::shared_ptr<MidiModel> mm)
{
	typedef boost::shared_ptr<Evoral::Note<Temporal::Beats> > NotePtr;

	std::list<NotePtr> note_ptr_list;

	const MidiModel::Notes& notes = mm->notes();
	for (MidiModel::Notes::const_iterator i = notes.begin(); i != notes.end(); ++i) {
		note_ptr_list.push_back (*i);
	}
	return note_ptr_list;
}

/* ****************************************************************************/

const samplecnt_t LuaAPI::Rubberband::_bufsize = 256;

LuaAPI::Rubberband::Rubberband (boost::shared_ptr<AudioRegion> r, bool percussive)
	: _region (r)
	, _rbs (r->session().sample_rate(), r->n_channels(),
	        percussive ? RubberBand::RubberBandStretcher::DefaultOptions : RubberBand::RubberBandStretcher::PercussiveOptions,
	        r->stretch (), r->shift ())
	, _stretch_ratio (r->stretch ())
	, _pitch_ratio (r->shift ())
	, _cb (0)
{
	_n_channels  = r->n_channels ();
	_read_len    = r->length_samples () / (double)r->stretch ();
	_read_start  = r->ancestral_start_sample () + samplecnt_t (r->start_sample () / (double)r->stretch ());
	_read_offset = _read_start - r->start_sample () + r->position_sample ();
}

LuaAPI::Rubberband::~Rubberband ()
{
}

bool
LuaAPI::Rubberband::set_strech_and_pitch (double stretch_ratio, double pitch_ratio)
{
	if (stretch_ratio <= 0 || pitch_ratio <= 0) {
		return false;
	}
	_stretch_ratio = stretch_ratio * _region->stretch ();
	_pitch_ratio   = pitch_ratio   * _region->shift ();
	return true;
}

bool
LuaAPI::Rubberband::set_mapping (luabridge::LuaRef tbl)
{
	if (!tbl.isTable ()) {
		return false;
	}

	_mapping.clear ();

	for (luabridge::Iterator i (tbl); !i.isNil (); ++i) {
		if (!i.key ().isNumber () || !i.value ().isNumber ()) {
			continue;
		}
		size_t ss = i.key ().cast<double> ();
		size_t ds = i.value ().cast<double> ();
		printf ("ADD %ld %ld\n", ss, ds);
		_mapping[ss] = ds;
	}
	return !_mapping.empty ();
}

samplecnt_t
LuaAPI::Rubberband::read (Sample* buf, samplepos_t pos, samplecnt_t cnt, int channel) const
{
	return _region->master_read_at (buf, NULL, NULL, _read_offset + pos, cnt, channel);
}

static void null_deleter (LuaAPI::Rubberband*) {}

boost::shared_ptr<AudioReadable>
LuaAPI::Rubberband::readable ()
{
	if (!_self) {
		_self = boost::shared_ptr<Rubberband> (this, &null_deleter);
	}
	return boost::dynamic_pointer_cast<AudioReadable> (_self);
}

bool
LuaAPI::Rubberband::read_region (bool study)
{
	samplepos_t pos = 0;

	float** buffers = new float*[_n_channels];
	for (uint32_t c = 0; c < _n_channels; ++c) {
		buffers[c] = new float[_bufsize];
	}

	while (pos < _read_len) {
		samplecnt_t n_read = 0;
		for (uint32_t c = 0; c < _n_channels; ++c) {
			samplepos_t to_read = std::min (_bufsize, _read_len - pos);
			n_read = read (buffers[c], pos, to_read, c);
			if (n_read != to_read) {
				pos = 0;
				goto errout;
			}
		}

		pos += n_read;

		assert (!_cb || _cb->type () == LUA_TFUNCTION);
		if ((*_cb) (NULL, pos * .5 + (study ? 0 : _read_len / 2))) {
			pos = 0;
			goto errout;
		}

		if (study) {
			_rbs.study (buffers, n_read, pos == _read_len);
			continue;
		}

		assert (_asrc.size () == _n_channels);
		_rbs.process (buffers, n_read, pos == _read_len);

		if (!retrieve (buffers)) {
			pos = 0;
			goto errout;
		}
	}

	if (!retrieve (buffers)) {
		pos = 0;
	}

errout:
	if (buffers) {
		for (uint32_t c = 0; c < _n_channels; ++c) {
			delete[] buffers[c];
		}
		delete[] buffers;
	}
	return pos == _read_len;
}

bool
LuaAPI::Rubberband::retrieve (float** buffers)
{
	samplecnt_t avail = 0;
	while ((avail = _rbs.available ()) > 0) {
		samplepos_t to_read = std::min (_bufsize, avail);
		_rbs.retrieve (buffers, to_read);

		for (uint32_t c = 0; c < _asrc.size (); ++c) {
			if (_asrc[c]->write (buffers[c], to_read) != to_read) {
				return false;
			}
		}
	}
	return true;
}

boost::shared_ptr<AudioRegion>
LuaAPI::Rubberband::process (luabridge::LuaRef cb)
{
	boost::shared_ptr<AudioRegion> rv;
	if (cb.type () == LUA_TFUNCTION) {
		_cb = new luabridge::LuaRef (cb);
	}

	_rbs.reset ();
	_rbs.setDebugLevel (1);
	_rbs.setTimeRatio (_stretch_ratio);
	_rbs.setPitchScale (_pitch_ratio);
	_rbs.setExpectedInputDuration (_read_len);

	/* compare to Filter::make_new_sources */
	vector<string> names    = _region->master_source_names ();
	Session&    session     = _region->session ();
	samplecnt_t sample_rate = session.sample_rate ();

	for (uint32_t c = 0; c < _n_channels; ++c) {
		string       name = PBD::basename_nosuffix (names[c]) + "(rb)";
		const string path = session.new_audio_source_path (name, _n_channels, c, false);
		if (path.empty ()) {
			cleanup (true);
			return rv;
		}
		try {

			_asrc.push_back (boost::dynamic_pointer_cast<AudioSource> (SourceFactory::createWritable (DataType::AUDIO, session, path, sample_rate)));

		} catch (failed_constructor& err) {
			cleanup (true);
			return rv;
		}
	}

	/* study */
	if (!read_region (true)) {
		cleanup (true);
		return rv;
	}

	if (!_mapping.empty ()) {
		_rbs.setKeyFrameMap (_mapping);
	}

	/* process */
	if (!read_region (false)) {
		cleanup (true);
		return rv;
	}

	rv = finalize ();

	cleanup (false);
	return rv;
}

boost::shared_ptr<AudioRegion>
LuaAPI::Rubberband::finalize ()
{
	time_t     xnow = time (NULL);
	struct tm* now  = localtime (&xnow);

	/* this is the same as RBEffect::finish, Filter::finish */
	SourceList sl;
	for (std::vector<boost::shared_ptr<AudioSource> >::iterator i = _asrc.begin (); i != _asrc.end (); ++i) {
		boost::shared_ptr<AudioFileSource> afs = boost::dynamic_pointer_cast<AudioFileSource> (*i);
		assert (afs);
		afs->done_with_peakfile_writes ();
		afs->update_header (_region->position_sample (), *now, xnow);
		afs->mark_immutable ();
		Analyser::queue_source_for_analysis (*i, false);
		sl.push_back (*i);
	}

	/* create a new region */
	std::string region_name = RegionFactory::new_region_name (_region->name ());

	PropertyList plist;
	plist.add (Properties::start, 0);
	plist.add (Properties::length, _region->length_samples ());
	plist.add (Properties::name, region_name);
	plist.add (Properties::whole_file, true);
	plist.add (Properties::position, _region->position_sample ());

	boost::shared_ptr<Region>      r  = RegionFactory::create (sl, plist);
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);

	ar->set_scale_amplitude (_region->scale_amplitude ());
	ar->set_fade_in_active (_region->fade_in_active ());
	ar->set_fade_in (_region->fade_in ());
	ar->set_fade_out_active (_region->fade_out_active ());
	ar->set_fade_out (_region->fade_out ());
	*(ar->envelope ()) = *(_region->envelope ());

	ar->set_ancestral_data (timecnt_t (_read_start), timecnt_t (_read_len), _stretch_ratio, _pitch_ratio);
	ar->set_master_sources (_region->master_sources ());
	ar->set_length (ar->length () * _stretch_ratio); // XXX
	if (_stretch_ratio != 1.0) {
		// TODO: apply mapping
		ar->envelope ()->x_scale (_stretch_ratio);
	}

	return ar;
}

void
LuaAPI::Rubberband::cleanup (bool abort)
{
	if (abort) {
		for (std::vector<boost::shared_ptr<AudioSource> >::iterator i = _asrc.begin (); i != _asrc.end (); ++i) {
			(*i)->mark_for_remove ();
		}
	}
	_asrc.clear ();
	delete (_cb);
	_cb = 0;
}
