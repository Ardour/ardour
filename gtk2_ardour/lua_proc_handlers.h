/*
 * Copyright (C) 2026 Brent Baccala <cosine@freesoft.org>
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

#pragma once

#include <map>
#include <memory>

#include "pbd/signals.h"

namespace ARDOUR { class LuaProc; }
class LuaState;
namespace luabridge { class LuaRef; }

/** GUI-thread side of the RT->GUI Lua dispatch (see project report
 *  ardour-rt-gui-lua-dispatch-design.md / -impl-notes.md).
 *
 *  A process-wide singleton.  It connects ARDOUR::LuaProc::AccessLuaScript
 *  once with gui_context(), so every RT-side self:dispatch(id,val) arrives
 *  here on the GUI thread.  For each LuaProc that uses the feature it lazily
 *  builds a dedicated "handler" Lua interpreter -- full GUI bindings
 *  (LuaInstance::register_classes/register_hooks) plus the libardour-owned
 *  per-plugin bootstrap (LuaProc::setup_lua_handler_gui: dsp bindings, the
 *  script itself, and self/Session/CtrlPorts) -- runs the script's
 *  gui_init(), and thereafter routes (id,val) to the Lua handler the script
 *  registered.  The interpreter is torn down when the LuaProc is destroyed.
 *
 *  The interpreter is created lazily inside the dispatch slot itself (same
 *  pattern as ProcessorEntry::LuaPluginDisplay's inline interpreter): the
 *  first dispatch builds the VM, runs gui_init() -- which registers the
 *  handlers -- and then handles that same event, so nothing is lost.
 */
class LuaProcHandlers : public PBD::ScopedConnectionList
{
public:
	static LuaProcHandlers* instance ();
	~LuaProcHandlers ();

private:
	LuaProcHandlers ();
	static LuaProcHandlers* _instance;

	struct HandlerVM {
		HandlerVM () : lua (0), dispatch_fn (0) {}
		~HandlerVM ();
		LuaState*            lua;
		luabridge::LuaRef*   dispatch_fn; /* the prelude's __dispatch(id,val) */
		PBD::ScopedConnection drop_connection;
	};

	typedef std::map<ARDOUR::LuaProc*, std::shared_ptr<HandlerVM> > VMMap;
	VMMap _vms;

	/* slot for LuaProc::AccessLuaScript, always on the GUI thread */
	void dispatch (ARDOUR::LuaProc* proc, int handler_id, int value);

	std::shared_ptr<HandlerVM> get_or_create (ARDOUR::LuaProc* proc);
	void drop (ARDOUR::LuaProc* proc);
};
