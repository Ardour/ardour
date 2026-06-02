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

#include <iostream>

#include "pbd/error.h"

#include "ardour/luaproc.h"

#include "lua/luastate.h"
#include "LuaBridge/LuaBridge.h"

#include "gtkmm2ext/gui_thread.h"

#include "public_editor.h"
#include "luainstance.h"
#include "lua_proc_handlers.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

/* The Lua "prelude" loaded, on the GUI thread, into every handler
 * interpreter.  It supplies the script-facing registry API (register_handler
 * / unregister_handler / clear_handlers) and the single C++-callable entry
 * point __dispatch(id,val).
 *
 * The handler interpreter does not exist until the DSP (RT) thread sends its
 * first message.  That first self:dispatch() emits AccessLuaScript, which is
 * marshalled to the GUI thread, where dispatch() -> get_or_create() builds the
 * interpreter, loads this prelude, and runs the script's gui_init() (which
 * should call register_handler to populate __rt_handlers).  That first
 * message -- and every message after it -- is then delivered by calling
 * __dispatch(id, val) in the interpreter, which looks up the handler
 * registered for id and invokes it.
 *
 * Keeping the registry in Lua keeps the C++ surface minimal and gives handlers
 * a plain Lua table to reason about.  pcall isolates a throwing handler so one
 * bad callback can't take down the GUI thread. */
static const char* handler_prelude =
	"__rt_handlers = {}\n"
	"function register_handler (id, fn) __rt_handlers[id] = fn end\n"
	"function unregister_handler (id) __rt_handlers[id] = nil end\n"
	"function clear_handlers () __rt_handlers = {} end\n"
	"function __dispatch (id, val)\n"
	"  local fn = __rt_handlers[id]\n"
	"  if fn then\n"
	"    local ok, err = pcall (fn, val)\n"
	"    if not ok then print ('RT-dispatch handler ' .. tostring(id) .. ' error: ' .. tostring(err)) end\n"
	"  end\n"
	"end\n";

LuaProcHandlers* LuaProcHandlers::_instance = 0;

LuaProcHandlers*
LuaProcHandlers::instance ()
{
	if (!_instance) {
		_instance = new LuaProcHandlers ();
	}
	return _instance;
}

LuaProcHandlers::LuaProcHandlers ()
{
	/* One process-wide connection.  gui_context() marshals every RT-side
	 * emit onto the GUI thread via PBD::AbstractUI::call_slot -- exactly
	 * the path BasicUI::AccessAction uses (editor.cc). */
	LuaProc::AccessLuaScript.connect (
			*this, MISSING_INVALIDATOR,
			std::bind (&LuaProcHandlers::dispatch, this,
			           std::placeholders::_1,
			           std::placeholders::_2,
			           std::placeholders::_3),
			gui_context ());
}

LuaProcHandlers::~LuaProcHandlers ()
{
	_vms.clear ();
}

LuaProcHandlers::HandlerVM::~HandlerVM ()
{
	delete dispatch_fn;
	delete lua;
}

std::shared_ptr<LuaProcHandlers::HandlerVM>
LuaProcHandlers::get_or_create (LuaProc* proc)
{
	VMMap::iterator i = _vms.find (proc);
	if (i != _vms.end ()) {
		return i->second;
	}

	std::shared_ptr<HandlerVM> vm (new HandlerVM ());

	const bool sandbox = UIConfiguration::instance ().get_sandbox_all_lua_scripts ();
	vm->lua = new LuaState (sandbox, true);
	lua_State* L = vm->lua->getState ();

	try {
		/* GUI-owned bindings: the full action-script surface, so handlers
		 * can do Editor:access_action, dialogs, OSC, etc. */
		LuaInstance::register_classes (L, sandbox);
		LuaInstance::register_hooks (L);

		luabridge::push <PublicEditor *> (L, &PublicEditor::instance ());
		lua_setglobal (L, "Editor");

		/* libardour-owned bootstrap: dsp bindings, the LuaProc class,
		 * self / Session / CtrlPorts, and do_command(_script). */
		proc->setup_lua_handler_gui (vm->lua);

		/* registry API + __dispatch entry point */
		vm->lua->do_command (handler_prelude);

		/* one-time GUI initialisation; this is where the script is
		 * expected to call register_handler(...) to populate __rt_handlers */
		luabridge::LuaRef gi = luabridge::getGlobal (L, "gui_init");
		if (gi.isFunction ()) {
			gi ();
		}

		vm->dispatch_fn = new luabridge::LuaRef (luabridge::getGlobal (L, "__dispatch"));
	} catch (luabridge::LuaException const& e) {
		PBD::error << string_compose (_("LuaProc handler init failed: %1"), e.what ()) << endmsg;
		vm->dispatch_fn = 0;
	} catch (...) {
		PBD::error << _("LuaProc handler init failed (unknown error)") << endmsg;
		vm->dispatch_fn = 0;
	}

	/* tear the interpreter down when the plugin goes away.  We only use
	 * 'proc' as a map key here, never dereference it, so a deferred
	 * (gui_context) callback after the LuaProc is gone is safe. */
	proc->DropReferences.connect (
			vm->drop_connection, MISSING_INVALIDATOR,
			std::bind (&LuaProcHandlers::drop, this, proc),
			gui_context ());

	_vms[proc] = vm;
	return vm;
}

void
LuaProcHandlers::dispatch (LuaProc* proc, int handler_id, int value)
{
	std::shared_ptr<HandlerVM> vm = get_or_create (proc);
	if (!vm->dispatch_fn) {
		return;
	}
	try {
		(*vm->dispatch_fn) (handler_id, value);
	} catch (luabridge::LuaException const& e) {
		PBD::error << string_compose (_("LuaProc handler dispatch error: %1"), e.what ()) << endmsg;
	} catch (...) {
		PBD::error << _("LuaProc handler dispatch error (unknown)") << endmsg;
	}
}

void
LuaProcHandlers::drop (LuaProc* proc)
{
	VMMap::iterator i = _vms.find (proc);
	if (i != _vms.end ()) {
		_vms.erase (i);
	}
}
