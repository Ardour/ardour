ardour {
	["type"]    = "EditorAction",
	name        = "Add ACE MIDI Monitor",
	license     = "MIT",
	author      = "Vincent Tassy",
	description = [[Adds an ACE MIDI Monitor on the current track]]
}

function factory () return function ()
	local sel = Editor:get_selection ()
	-- for each selected track/bus
	for r in sel.tracks:routelist ():iter () do
			if not r:to_track ():isnil () and not r:to_track ():to_midi_track ():isnil () then
				local proc = ARDOUR.LuaAPI.new_plugin(Session, "ACE MIDI Monitor", ARDOUR.PluginType.Lua, "");
				assert (not proc:isnil())
				r:add_processor_by_index(proc, 0, nil, true)
				proc = nil;
			end
	end
end end
