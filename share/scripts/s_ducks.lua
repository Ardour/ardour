ardour { ["type"] = "Snippet", name = "Ducks" }

function factory (params) return function ()

	local chan_out  = 2
	if not Session:master_out():isnil() then
		chan_out = Session:master_out():n_inputs ():n_audio ()
	end

	-- create two mono tracks
	local tl = Session:new_audio_track (1, chan_out, nil, 2, "Ducks", ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal, true)
	for t in tl:iter() do
		t:set_strict_io (true)
		-- switch tracks to monitor input
		t:monitoring_control():set_value (ARDOUR.MonitorChoice.MonitorInput, PBD.GroupControlDisposition.NoGroup)
	end

	local src = tl:front ();
	local dst = tl:back ();

	assert (not src:isnil() and not dst:isnil())

	-- add "ACE Compressor" to target track
	local p = ARDOUR.LuaAPI.new_plugin (Session, "urn:ardour:a-comp", ARDOUR.PluginType.LV2, "")
	assert (not p:isnil ())

	dst:add_processor_by_index (p, 0, nil, true)
	ARDOUR.LuaAPI.set_processor_param (p, 1, 300) -- 300ms release time
	ARDOUR.LuaAPI.set_processor_param (p, 2, 4)   -- 4dB Knee
	ARDOUR.LuaAPI.set_processor_param (p, 3, 7)   -- ratio 1:7
	ARDOUR.LuaAPI.set_processor_param (p, 4, -25) -- threshold -20dBFS
	ARDOUR.LuaAPI.set_processor_param (p, 9, 1)   -- enable sidechain

	-- add Send to src track before the fader
	local s = ARDOUR.LuaAPI.new_send (Session, src, src:amp ())
	assert (not s:isnil ())

	-- mark as sidechain send
	local send = s:to_send()
	send:set_remove_on_disconnect (true)

	-- now connect send to plugin's sidechain input
	local src_io = send:output()

	-- ACE Compressor already has a sidechain, and sidechain
	-- pin connected. Other plugins see "plugin channel-map dev"
	-- snippet how to change plugin pinout.
	local dst_io = p:to_plugininsert ():sidechain_input ()

	src_io:nth(0):connect (dst_io:nth (0):name ())

end end
