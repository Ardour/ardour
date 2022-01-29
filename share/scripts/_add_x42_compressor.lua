ardour {
	["type"]    = "EditorAction",
	name        = "Add x42 Dynamic Compressor",
	license     = "MIT",
	author      = "Vincent Tassy",
	description = [[Adds a x42 Dynamic Compressor on the current track]]
}

function factory () return function ()
	local sel = Editor:get_selection ()
	local proc = nil
	-- for each selected track/bus
	for r in sel.tracks:routelist ():iter () do
		local i = 0;
		local pos = 0;
		repeat
			proc = r:nth_processor (i) -- get Nth Ardour::Processor
			local plugin = proc:to_plugininsert ()
			if not plugin:isnil() then
				pos = pos + 1
			end
			i = i + 1
		until (proc:display_name() == "Fader")
		-- select mono or stereo version of the plugin
		if proc:input_streams():n_audio () == 2 then
				proc = ARDOUR.LuaAPI.new_plugin(Session, "http://gareus.org/oss/lv2/darc#stereo", ARDOUR.PluginType.LV2, "");
			else
				proc = ARDOUR.LuaAPI.new_plugin(Session, "http://gareus.org/oss/lv2/darc#mono", ARDOUR.PluginType.LV2, "");
		end
		assert (not proc:isnil())
		r:add_processor_by_index(proc, pos, nil, true)
		proc = nil;
	end
end end
