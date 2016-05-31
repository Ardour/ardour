ardour { ["type"] = "Snippet", name = "plugin channel-map dev" }

function factory () return function ()
	-- first track needs to be stereo and have a stereo plugin
	-- (x42-eq with spectrum display, per channel processing,
	--  and pre/post visualization is very handy here)

	function checksetup (r)
		-- fail if Route ID 1 is not present or not stereo
		assert (r and not r:isnil())
                assert (r:n_inputs():n_audio() == 2)
		-- check first Plugin and make sure it is a "Plugin Insert"
		if not r:nth_plugin(0):isnil() and not r:nth_plugin(0):to_insert():isnil() then return end
		-- insert x42-eq at the top.
		local proc = ARDOUR.LuaAPI.new_plugin(Session, "http://gareus.org/oss/lv2/fil4#stereo", ARDOUR.PluginType.LV2, "");
		r:add_processor_by_index(proc, 0, nil, true)
	end

	r = Session:get_remote_nth_route(1)
	checksetup (r)
	pi = r:nth_plugin(0):to_insert()

	pi:set_no_inplace (true)

	cm = ARDOUR.ChanMapping()
	--cm:set(ARDOUR.DataType("Audio"), 0, 0)
	cm:set(ARDOUR.DataType("Audio"), 1, 0)
	pi:set_input_map (0, cm)

	cm = ARDOUR.ChanMapping()
	--cm:set(ARDOUR.DataType("Audio"), 0, 0)
	cm:set(ARDOUR.DataType("Audio"), 1, 0)
	pi:set_output_map (0, cm)
end end
