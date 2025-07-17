ardour { ["type"] = "EditorAction", name = "Remove SysEx",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Remove SysEx MIDI events from selected MIDI region(s).]]
}

function factory () return function ()
	local sel = Editor:get_selection ()
	for r in sel.regions:regionlist ():iter () do
		local mr = r:to_midiregion ()
		if mr:isnil () then goto continue end

		local mm = mr:midi_source(0):model ()
		local midi_command = mm:new_sysex_diff_command ("Remove SysEx Events")
		for event in ARDOUR.LuaAPI.sysex_list (mm):iter () do
				midi_command:remove (event)
		end
		mm:apply_command (Session, midi_command)
		::continue::
	end
end end
