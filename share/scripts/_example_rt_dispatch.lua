ardour {
	["type"]    = "dsp",
	name        = "RT->GUI Dispatch Example",
	category    = "Example",
	author      = "Brent Baccala",
	description = [[Minimal example of the RT->GUI Lua dispatch primitive.

A DSP (realtime) Lua script may not safely call load(), pcall() of user
code, route/Session mutators, or iterate the route list from dsp_run().
This primitive lets the realtime side forward a tiny (handler_id, value)
pair to the GUI thread, where a handler with the full action-script
binding set does the unsafe work.

Pattern:
  * pick integer handler IDs (plain Lua constants -- both interpreters
    run this script, so both see them);
  * in dsp_run() (realtime), call self:dispatch(id, value) -- emit only;
  * in gui_init() (GUI thread, called once), call register_handler(id, fn)
    for each handler.  fn(value) runs on the GUI thread.

For payloads wider than one int, write into self:shmem() at agreed
offsets on the RT side and dispatch a slot index; read it back in the
handler.  Two 7-bit MIDI bytes also pack into one int as (a << 7) | b.
]]
}

function dsp_ioconfig ()
	return { { midi_in = 1, midi_out = 1, audio_in = 0, audio_out = 0 } }
end

HANDLER_NOTE_ON = 1   -- value = (note_number << 7) | velocity

-- realtime: forward every MIDI note-on to the GUI thread, pass the rest
function dsp_run (_, _, n_samples)
	local oi = 1
	for _, b in pairs (midiin) do
		local d = b["data"]
		local consumed = false
		if #d >= 3 and (d[1] >> 4) == 9 and d[3] > 0 then
			self:dispatch (HANDLER_NOTE_ON, (d[2] << 7) | d[3])
			consumed = true
		end
		if not consumed then
			midiout[oi] = { time = b["time"], data = d }
			oi = oi + 1
		end
	end
end

-- GUI thread: safe to touch Session/routes, print, run user code, etc.
function gui_init ()
	register_handler (HANDLER_NOTE_ON, function (value)
		local note = value >> 7
		local velocity = value & 127
		print (string.format ("note-on %d velocity %d (handled on GUI thread)", note, velocity))
	end)
end
