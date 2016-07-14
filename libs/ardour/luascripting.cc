/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#include <cstring>
#include <glibmm.h>

#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/compose.h"

#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"
#include "ardour/luascripting.h"
#include "ardour/lua_script_params.h"
#include "ardour/search_paths.h"

#include "lua/luastate.h"
#include "LuaBridge/LuaBridge.h"

#include "pbd/i18n.h"
#include "sha1.c"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

LuaScripting* LuaScripting::_instance = 0;

LuaScripting&
LuaScripting::instance ()
{
	if (!_instance) {
		_instance = new LuaScripting;
	}
	return *_instance;
}

LuaScripting::LuaScripting ()
	: _sl_dsp (0)
	, _sl_session (0)
	, _sl_hook (0)
	, _sl_action (0)
	, _sl_snippet (0)
{
	;
}

LuaScripting::~LuaScripting ()
{
	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {
		// don't bother, just exit quickly.
		delete _sl_dsp;
		delete _sl_session;
		delete _sl_hook;
		delete _sl_action;
		delete _sl_snippet;
	}
}


void
LuaScripting::refresh (bool run_scan)
{
	Glib::Threads::Mutex::Lock lm (_lock);

	delete _sl_dsp;
	delete _sl_session;
	delete _sl_hook;
	delete _sl_action;
	delete _sl_snippet;

	_sl_dsp = 0;
	_sl_session = 0;
	_sl_hook = 0;
	_sl_action = 0;
	_sl_snippet = 0;

	if (run_scan) {
		lm.release ();
		scan ();
	}
}

struct ScriptSorter {
	bool operator () (LuaScriptInfoPtr a, LuaScriptInfoPtr b) {
		return a->name < b->name;
	}
};

LuaScriptInfoPtr
LuaScripting::script_info (const std::string &script) {
	return scan_script ("", script);
}

void
LuaScripting::scan ()
{
	Glib::Threads::Mutex::Lock lm (_lock);

#define CLEAR_OR_NEW(LIST) \
	if (LIST) { LIST->clear (); } else { LIST = new LuaScriptList (); }

	CLEAR_OR_NEW (_sl_dsp)
	CLEAR_OR_NEW (_sl_session)
	CLEAR_OR_NEW (_sl_hook)
	CLEAR_OR_NEW (_sl_action)
	CLEAR_OR_NEW (_sl_snippet)

#undef CLEAR_OR_NEW

	vector<string> luascripts;
	find_files_matching_pattern (luascripts, lua_search_path (), "*.lua");

	for (vector<string>::iterator i = luascripts.begin(); i != luascripts.end (); ++i) {
		LuaScriptInfoPtr lsi = scan_script (*i);
		if (!lsi) {
			PBD::info << string_compose (_("Script '%1' has no valid descriptor."), *i) << endmsg;
			continue;
		}
		switch (lsi->type) {
			case LuaScriptInfo::DSP:
				_sl_dsp->push_back(lsi);
				break;
			case LuaScriptInfo::Session:
				_sl_session->push_back(lsi);
				break;
			case LuaScriptInfo::EditorHook:
				_sl_hook->push_back(lsi);
				break;
			case LuaScriptInfo::EditorAction:
				_sl_action->push_back(lsi);
				break;
			case LuaScriptInfo::Snippet:
				_sl_snippet->push_back(lsi);
				break;
			default:
				break;
		}
	}

	std::sort (_sl_dsp->begin(), _sl_dsp->end(), ScriptSorter());
	std::sort (_sl_session->begin(), _sl_session->end(), ScriptSorter());
	std::sort (_sl_hook->begin(), _sl_hook->end(), ScriptSorter());
	std::sort (_sl_action->begin(), _sl_action->end(), ScriptSorter());
	std::sort (_sl_snippet->begin(), _sl_snippet->end(), ScriptSorter());

	scripts_changed (); /* EMIT SIGNAL */
}

void
LuaScripting::lua_print (std::string s) {
	PBD::info << "Lua: " << s << "\n";
}

LuaScriptInfoPtr
LuaScripting::scan_script (const std::string &fn, const std::string &sc)
{
	LuaState lua;
	if (!(fn.empty() ^ sc.empty())){
		// give either file OR script
		assert (0);
		return LuaScriptInfoPtr();
	}

	lua_State* L = lua.getState();
	lua.Print.connect (&LuaScripting::lua_print);

	lua.do_command ("io = nil;");

	lua.do_command (
			"ardourluainfo = {}"
			"function ardour (entry)"
			"  ardourluainfo['type'] = assert(entry['type'])"
			"  ardourluainfo['name'] = assert(entry['name'])"
			"  ardourluainfo['category'] = entry['category'] or 'Unknown'"
			"  ardourluainfo['author'] = entry['author'] or 'Unknown'"
			"  ardourluainfo['license'] = entry['license'] or ''"
			"  ardourluainfo['description'] = entry['description'] or ''"
			" end"
			);

	try {
		int err;
		if (fn.empty()) {
			err = lua.do_command (sc);
		} else {
			err = lua.do_file (fn);
		}
		if (err) {
#ifndef NDEBUG
		cerr << "failed to load lua script\n";
#endif
			return LuaScriptInfoPtr();
		}
	} catch (...) { // luabridge::LuaException
#ifndef NDEBUG
		cerr << "failed to parse lua script\n";
#endif
		return LuaScriptInfoPtr();
	}
	luabridge::LuaRef nfo = luabridge::getGlobal (L, "ardourluainfo");
	if (nfo.type() != LUA_TTABLE) {
#ifndef NDEBUG
		cerr << "failed to get ardour{} table from script\n";
#endif
		return LuaScriptInfoPtr();
	}

	if (nfo["name"].type() != LUA_TSTRING || nfo["type"].type() != LUA_TSTRING) {
#ifndef NDEBUG
		cerr << "script-type or script-name is not a string\n";
#endif
		return LuaScriptInfoPtr();
	}

	std::string name = nfo["name"].cast<std::string>();
	LuaScriptInfo::ScriptType type = LuaScriptInfo::str2type (nfo["type"].cast<std::string>());

	if (name.empty() || type == LuaScriptInfo::Invalid) {
#ifndef NDEBUG
		cerr << "invalid script-type or missing script name\n";
#endif
		return LuaScriptInfoPtr();
	}

	char hash[41];
	Sha1Digest s;
	sha1_init (&s);

	if (fn.empty()) {
		sha1_write (&s, (const uint8_t *) sc.c_str(), sc.size ());
	} else {
		try {
			std::string script = Glib::file_get_contents (fn);
			sha1_write (&s, (const uint8_t *) script.c_str(), script.size ());
		} catch (Glib::FileError err) {
			return LuaScriptInfoPtr();
		}
	}
	sha1_result_hash (&s, hash);


	LuaScriptInfoPtr lsi (new LuaScriptInfo (type, name, fn, hash));

	for (luabridge::Iterator i(nfo); !i.isNil (); ++i) {
		if (!i.key().isString() || !i.value().isString()) {
			return LuaScriptInfoPtr();
		}
		std::string key = i.key().tostring();
		std::string val = i.value().tostring();

		if (key == "author") { lsi->author = val; }
		if (key == "license") { lsi->license = val; }
		if (key == "description") { lsi->description = val; }
		if (key == "category") { lsi->category = val; }
	}
	return lsi;
}

LuaScriptList &
LuaScripting::scripts (LuaScriptInfo::ScriptType type) {

	if (!_sl_dsp || !_sl_session || !_sl_hook || !_sl_action || !_sl_snippet) {
		scan ();
	}

	switch (type) {
		case LuaScriptInfo::DSP:
			return *_sl_dsp;
			break;
		case LuaScriptInfo::Session:
			return *_sl_session;
			break;
		case LuaScriptInfo::EditorHook:
			return *_sl_hook;
			break;
		case LuaScriptInfo::EditorAction:
			return *_sl_action;
			break;
		case LuaScriptInfo::Snippet:
			return *_sl_snippet;
			break;
		default:
			break;
	}
	return _empty_script_info; // make some compilers happy
}


std::string
LuaScriptInfo::type2str (const ScriptType t) {
	switch (t) {
		case LuaScriptInfo::DSP: return "DSP";
		case LuaScriptInfo::Session: return "Session";
		case LuaScriptInfo::EditorHook: return "EditorHook";
		case LuaScriptInfo::EditorAction: return "EditorAction";
		case LuaScriptInfo::Snippet: return "Snippet";
		default: return "Invalid";
	}
}

LuaScriptInfo::ScriptType
LuaScriptInfo::str2type (const std::string& str) {
	const char* type = str.c_str();
	if (!strcasecmp (type, "DSP")) {return LuaScriptInfo::DSP;}
	if (!strcasecmp (type, "Session")) {return LuaScriptInfo::Session;}
	if (!strcasecmp (type, "EditorHook")) {return LuaScriptInfo::EditorHook;}
	if (!strcasecmp (type, "EditorAction")) {return LuaScriptInfo::EditorAction;}
	if (!strcasecmp (type, "Snippet")) {return LuaScriptInfo::Snippet;}
	return LuaScriptInfo::Invalid;
}

LuaScriptParamList
LuaScriptParams::script_params (LuaScriptInfoPtr lsi, const std::string &pname)
{
	assert (lsi);
	return LuaScriptParams::script_params (lsi->path, pname);
}

LuaScriptParamList
LuaScriptParams::script_params (const std::string& s, const std::string &pname, bool file)
{
	LuaScriptParamList rv;

	LuaState lua;
	lua_State* L = lua.getState();
	lua.do_command ("io = nil;");
	lua.do_command ("function ardour () end");

	try {
		if (file) {
			lua.do_file (s);
		} else {
			lua.do_command (s);
		}
	} catch (luabridge::LuaException const& e) {
		return rv;
	}

	luabridge::LuaRef lua_params = luabridge::getGlobal (L, pname.c_str());
	if (lua_params.isFunction ()) {
		luabridge::LuaRef params = lua_params ();
		if (params.isTable ()) {
			for (luabridge::Iterator i (params); !i.isNil (); ++i) {
				if (!i.key ().isString ())            { continue; }
				if (!i.value ().isTable ())           { continue; }
				if (!i.value ()["title"].isString ()) { continue; }

				std::string name = i.key ().cast<std::string> ();
				std::string title = i.value ()["title"].cast<std::string> ();
				std::string dflt;
				bool optional = false;

				if (i.value ()["default"].isString ()) {
					dflt = i.value ()["default"].cast<std::string> ();
				}
				if (i.value ()["optional"].isBoolean ()) {
					optional = i.value ()["optional"].cast<bool> ();
				}
				LuaScriptParamPtr lsspp (new LuaScriptParam(name, title, dflt, optional));
				rv.push_back (lsspp);
			}
		}
	}
	return rv;
}

void
LuaScriptParams::params_to_ref (luabridge::LuaRef *tbl_args, const LuaScriptParamList& args)
{
	assert (tbl_args &&  (*tbl_args).isTable ());
	for (LuaScriptParamList::const_iterator i = args.begin(); i != args.end(); ++i) {
		if ((*i)->optional && !(*i)->is_set) { continue; }
		(*tbl_args)[(*i)->name] = (*i)->value;
	}
}

void
LuaScriptParams::ref_to_params (LuaScriptParamList& args, luabridge::LuaRef *tbl_ref)
{
	assert (tbl_ref &&  (*tbl_ref).isTable ());
	for (luabridge::Iterator i (*tbl_ref); !i.isNil (); ++i) {
		if (!i.key ().isString ()) { assert(0); continue; }
		std::string name = i.key ().cast<std::string> ();
		std::string value = i.value ().cast<std::string> ();
		for (LuaScriptParamList::const_iterator ii = args.begin(); ii != args.end(); ++ii) {
			if ((*ii)->name == name) {
				(*ii)->value = value;
				break;
			}
		}
	}
}

bool
LuaScripting::try_compile (const std::string& script, const LuaScriptParamList& args)
{
	const std::string& bytecode = get_factory_bytecode (script);
	if (bytecode.empty()) {
		return false;
	}
	LuaState l;
	l.Print.connect (&LuaScripting::lua_print);
	lua_State* L = l.getState();

	l.do_command (""
			" function checkfactory (b, a)"
			"  assert(type(b) == 'string', 'ByteCode must be string')"
			"  load(b)()" // assigns f
			"  assert(type(f) == 'string', 'Assigned ByteCode must be string')"
			"  local factory = load(f)"
			"  assert(type(factory) == 'function', 'Factory is a not a function')"
			"  local env = _ENV;  env.f = nil env.debug = nil os.exit = nil"
			"  load (string.dump(factory, true), nil, nil, env)(a)"
			" end"
			);

	try {
		luabridge::LuaRef lua_test = luabridge::getGlobal (L, "checkfactory");
		l.do_command ("checkfactory = nil"); // hide it.
		l.do_command ("collectgarbage()");

		luabridge::LuaRef tbl_arg (luabridge::newTable(L));
		LuaScriptParams::params_to_ref (&tbl_arg, args);
		lua_test (bytecode, tbl_arg);
		return true; // OK
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << e.what() << "\n";
#endif
		lua_print (e.what());
	}

	return false;
}

std::string
LuaScripting::get_factory_bytecode (const std::string& script)
{
	LuaState l;
	l.Print.connect (&LuaScripting::lua_print);
	lua_State* L = l.getState();

	l.do_command (
			" function ardour () end"
			""
			" function dump_function (f)"
			"  assert(type(f) == 'function', 'Factory is a not a function')"
			"  return string.format(\"f = %q\", string.dump(f, true))"
			" end"
			);

	try {
		luabridge::LuaRef lua_dump = luabridge::getGlobal (L, "dump_function");
		l.do_command ("dump_function = nil"); // hide it
		l.do_command (script); // register "factory"
		luabridge::LuaRef lua_factory = luabridge::getGlobal (L, "factory");

		if (lua_factory.isFunction()) {
			return (lua_dump(lua_factory)).cast<std::string> ();
		}
	} catch (luabridge::LuaException const& e) { }
	return "";
}

std::string
LuaScripting::user_script_dir ()
{
	std::string dir = Glib::build_filename (user_config_directory(), lua_dir_name);
	g_mkdir_with_parents (dir.c_str(), 0744);
	return dir;
}
