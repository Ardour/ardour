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

/** GUI-thread side of the realtime->GUI Lua dispatch primitive.
 *
 *  Problem
 *  -------
 *  A DSP (realtime) Lua plugin runs dsp_run() on the audio process thread,
 *  where it must not allocate, take contended locks, or call code that does
 *  either.  That rules out much of what a plugin might want to do in response
 *  to incoming data: load()/compile Lua source, pcall() arbitrary user code,
 *  mutate routes (Route::set_active), set controls (Session::set_control), or
 *  iterate the route list.  The MIDI PC/CC plugin (midi_pc_and_cc.lua) needs
 *  all of these, which is why this primitive exists.
 *
 *  Approach: three interpreters per plugin
 *  ----------------------------------------
 *  A LuaProc script can be run by up to three independent Lua interpreters,
 *  each a separate lua_State with its own globals.  The same script text is
 *  loaded into each, so script-level constants and functions appear in all of
 *  them; the three differ in which thread they run on and what they do:
 *
 *    - the RT interpreter (LuaProc's own lua_State).  Always present; runs the
 *      script's realtime callbacks (dsp_run() on the audio thread).  It does
 *      only RT-safe work and, when it wants something done off the RT thread,
 *      calls self:dispatch(handler_id, value).
 *
 *    - the inline-display interpreter.  Present only if the script defines a
 *      render_inline() function and its inline display is shown; runs on the
 *      GUI thread, owned by ProcessorEntry::LuaPluginDisplay (gtk2_ardour) and
 *      set up by LuaProc::setup_lua_inline_gui().  It draws the small in-mixer
 *      plugin display and is otherwise unrelated to the dispatch feature -- it
 *      is noted here only because the handler interpreter below is modelled on
 *      it (a GUI-owned second interpreter over the same LuaProc).
 *
 *    - the handler interpreter (the subject of this file).  Present only
 *      if the script uses dispatch; created and owned here on the GUI thread.
 *      It runs the script again and calls the script's gui_init(), in
 *      which the script should call register_handler(id, fn).  When a dispatch
 *      arrives, the matching fn(value) runs here on the GUI thread, where the
 *      non-RT-safe operations above are legal.
 *
 *  On the RT thread, self:dispatch() emits the static signal
 *  LuaProc::AccessLuaScript (LuaProc*, handler_id, value).  This singleton
 *  connects that signal once, with gui_context(), so the slot is marshalled
 *  from the emitting RT thread onto the GUI thread over
 *  PBD::AbstractUI::call_slot -- the per-thread lock-free ringbuffer +
 *  pipe-wakeup path Ardour already treats as RT-callable, and the same path
 *  BasicUI::AccessAction uses (see editor.cc).  The (LuaProc*, handler_id,
 *  value) payload fits the std::function small-object buffer, so the RT-side
 *  emit allocates nothing.
 *
 *  The dispatch payload is only two ints (handler_id, value).  A plugin that
 *  needs to hand the GUI side more than that -- a multi-byte MIDI event, a
 *  block of parameters -- can move the bulk of the data through self:shmem().
 *  shmem() is the LuaProc's per-instance DSP::DspShm region; because self is
 *  the same C++ LuaProc object in both the RT interpreter and the GUI
 *  interpreter, both see the same physical memory.  The RT side writes the
 *  data into shmem before calling self:dispatch(), passes an index/slot into
 *  it as the int payload, and the GUI interpreter reads it back out.  The
 *  dispatch ordering already gives a clean handoff -- the RT side writes the
 *  slot before emitting, and call_slot publishes that write to the GUI thread
 *  -- so for a write-then-dispatch payload no extra shmem synchronisation is
 *  needed.  Where the GUI side instead polls shmem on its own (the midimon.lua
 *  pattern), use an atomic write-index (shmem():atomic_set_int /
 *  atomic_get_int) over the ring so the reader sees only completed writes.
 *
 *  Sizing the shmem region.  DSP::DspShm is a flat, fixed-size, cache-aligned
 *  array of 4-byte cells (float and int32 alias the same storage); there is no
 *  built-in slot or ring concept, so how the buffer is laid out and sized is
 *  up to the script, which passes the total cell count to shmem():allocate(N).
 *  allocate() calls malloc and is NOT RT-safe: call it once from dsp_init
 *  (which runs off the RT thread), never from dsp_run.  The buffer is then
 *  fixed for the plugin's life, so a burst that would exceed its capacity must
 *  be dropped or coalesced, not grown into.
 *
 *  Why the handler interpreter lives here, not in LuaProc
 *  ------------------------------------------------------
 *  Handlers want the full action-script binding set -- Editor:access_action,
 *  dialogs, OSC -- which is LuaInstance::register_classes(), defined in
 *  gtk2_ardour.  libardour (where LuaProc lives) cannot depend on gtk2_ardour
 *  (that would reverse the library layering), so LuaProc cannot register
 *  those bindings itself.  The split mirrors the inline-display interpreter
 *  (ProcessorEntry::LuaPluginDisplay): libardour provides a bootstrap hook
 *  (LuaProc::setup_lua_handler_gui -- dsp bindings, the script, and
 *  self/Session/CtrlPorts, all needing LuaProc's private members) and the GUI
 *  side layers the GUI bindings + a register_handler() Lua prelude on top.
 *
 *  Lifetime
 *  --------
 *  The handler interpreter is built lazily, inside the dispatch slot itself
 *  (same pattern as the inline-display interpreter): the first dispatch for a
 *  given LuaProc builds the VM and runs gui_init() -- which registers the
 *  handlers -- and then handles that same event, so the first dispatch is not
 *  lost.  A plugin that never calls self:dispatch() never gets a handler
 *  interpreter.
 *
 *  The VM is torn down when the LuaProc is destroyed, via its DropReferences
 *  signal (also connected with gui_context()).  The LuaProc* is used only as
 *  a map key, never dereferenced in the teardown slot, so a teardown callback
 *  that runs after the LuaProc is already gone is safe.
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
