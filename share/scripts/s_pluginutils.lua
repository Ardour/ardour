ardour { ["type"] = "Snippet", name = "Plugin Utils" }

function factory () return function ()

	-------------------------------------------------------------------------------
	-- List all Plugins
	for p in ARDOUR.LuaAPI.list_plugins():iter() do
		print (p.name, p.unique_id, p.type)
		local psets = p:get_presets()
		if not empty:empty() then
			for pset in psets:iter() do
				print (" - ", pset.label)
			end
		end
	end

	-------------------------------------------------------------------------------
	-- add a Plugin (here LV2) to all mono tracks that contain the pattern "dru"
	-- and load a plugin-preset (if it exists)
	for r in Session:get_routes():iter() do
		if (string.match (r:name(), "dru") and r:n_inputs():n_audio() == 1) then
			local proc = ARDOUR.LuaAPI.new_plugin(Session, "http://gareus.org/oss/lv2/fil4#mono", ARDOUR.PluginType.LV2, "cutbass");
			assert (not proc:isnil())
			r:add_processor_by_index(proc, 0, nil, true)
		end
	end


	-------------------------------------------------------------------------------
	-- load a plugin preset
	route = Session:get_remote_nth_route(2)
	assert (route)
	-- to 4th plugin (from top), ardour starts counting at zero
	plugin = route:nth_plugin(3):to_insert():plugin(0)
	assert (not plugin:isnil())
	ps = plugin:preset_by_label("cutbass") -- get preset by name
	assert (ps)
	print (ps.uri)
	plugin:load_preset (ps)


	-------------------------------------------------------------------------------
	-- add a LuaProcessor (here "Scope") to all tracks
	for t in Session:get_tracks():iter() do
		local pos = 0 -- insert at the top

		-- the following two lines are equivalent
		--local proc = ARDOUR.LuaAPI.new_luaproc(Session, "a-Inline Scope");
		local proc = ARDOUR.LuaAPI.new_plugin (Session, "a-Inline Scope", ARDOUR.PluginType.Lua, "");
		assert (not proc:isnil())

		t:add_processor_by_index(proc, pos, nil, true)
		-- optionally set some parameters
		ARDOUR.LuaAPI.set_processor_param (proc, 0, 5) -- timescale 5sec
	end

end end
