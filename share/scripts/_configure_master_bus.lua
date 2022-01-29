ardour {
	["type"]    = "EditorAction",
	name        = "Configure Master Bus",
	license     = "GPL",
	author      = "Vincent Tassy",
	description = [[Adds meters post-fader and limiter pre-fader on Master Bus]]
}

function factory () return function ()

	local mb = Session:master_out ()
	local i = 0;
	local pos = 0;
	repeat
		local proc = r:nth_processor (i) -- get Nth Ardour::Processor
		local plugin = proc:to_plugininsert ()
		if not plugin:isnil() then
			pos = pos + 1
		end
		i = i + 1
	until (proc:display_name() == "Fader")
	-- Add Limiter
	local proc = ARDOUR.LuaAPI.new_plugin(Session, "http://gareus.org/oss/lv2/dpl#stereo", ARDOUR.PluginType.LV2, "");
	assert (not proc:isnil())
	mb:add_processor_by_index(proc, pos, nil, true)
	-- Add Stereo Phase-Correlation Meter
	proc = ARDOUR.LuaAPI.new_plugin(Session, "http://gareus.org/oss/lv2/meters#COR", ARDOUR.PluginType.LV2, "");
	assert (not proc:isnil())
	mb:add_processor_by_index(proc, -1, nil, true)
	-- Add 1/3 Octave Spectrum Display Stereo
	proc = ARDOUR.LuaAPI.new_plugin(Session, "http://gareus.org/oss/lv2/meters#spectr30stereo", ARDOUR.PluginType.LV2, "");
	assert (not proc:isnil())
	mb:add_processor_by_index(proc, -1, nil, true)
	-- Add EBU R128 Meter
	proc = ARDOUR.LuaAPI.new_plugin(Session, "http://gareus.org/oss/lv2/meters#EBUr128", ARDOUR.PluginType.LV2, "");
	assert (not proc:isnil())
	mb:add_processor_by_index(proc, -1, nil, true)
	-- Add VU Meter
	proc = ARDOUR.LuaAPI.new_plugin(Session, "http://gareus.org/oss/lv2/meters#VUstereo", ARDOUR.PluginType.LV2, "");
	assert (not proc:isnil())
	mb:add_processor_by_index(proc, -1, nil, true)
	proc = nil;
end end
