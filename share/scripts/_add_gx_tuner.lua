ardour {
	["type"]    = "EditorAction",
	name        = "Add Guitarix Instrument Tuner",
	license     = "GPL",
	author      = "Vincent Tassy",
	description = [[Adds a tuner on the current track]]
}

function factory () return function ()
	local sel = Editor:get_selection ()
	if not Editor:get_selection ():empty () and not Editor:get_selection ().tracks:routelist ():empty ()  then
		-- for each selected track
		for r in sel.tracks:routelist ():iter () do
			if not r:to_track ():isnil () and not r:to_track ():to_audio_track ():isnil () then
				local proc = ARDOUR.LuaAPI.new_plugin(Session, "http://guitarix.sourceforge.net/plugins/gxtuner#tuner", ARDOUR.PluginType.LV2, "");
				assert (not proc:isnil())
				r:add_processor_by_index(proc, 0, nil, true)
			end
		end
	end
end end
