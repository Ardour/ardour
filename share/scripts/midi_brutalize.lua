ardour { ["type"] = "EditorAction", name = "MIDI Brutalize",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Randomize MIDI Note position (de-quantize).]]
}

function factory () return function ()
local sel = Editor:get_selection ()
-- iterate over all selected regions
for r in sel.regions:regionlist ():iter () do
	local mr = r:to_midiregion ()
	if mr:isnil () then goto continue end

	local ticks_per_beat = Temporal.Beats (1, 0):to_ticks ();
	local max_shift = ticks_per_beat / 4.0

	-- get MIDI Model
	local mm = mr:midi_source(0):model ()
	-- Prepare Undo command
	local midi_command = mm:new_note_diff_command ("MIDI Note Brutalize")

	-- Iterate over all notes of the MIDI region
	for note in ARDOUR.LuaAPI.note_list (mm):iter () do
		-- note is-a https://manual.ardour.org/lua-scripting/class_reference/#Evoral:NotePtr
		local old_pos = note:time ()

		-- shift +/- 1/16th note
		local tickdiff = math.floor ((math.random() - 0.5) * max_shift)
		--print (old_pos:get_beats (), old_pos:get_ticks (), tickdiff)

		local new_pos = Temporal.Beats (old_pos:get_beats (), old_pos:get_ticks () + tickdiff)
		if old_pos ~= new_pos and new_pos > Temporal.Beats (0, 0) then
			local new_note = ARDOUR.LuaAPI.new_noteptr (note:channel (), new_pos, note:length (), note:note (), note:velocity ())
			midi_command:remove (note)
			midi_command:add (new_note)
		end
	end
	mm:apply_command (Session, midi_command)
	::continue::
end
end end
