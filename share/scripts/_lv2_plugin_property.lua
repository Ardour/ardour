ardour { ["type"] = "Snippet", name = "Exercise Plugin Props",
	license     = "MIT",
	author      = "Ardour Team",
}

function factory () return function ()
	for r in Session:get_routes ():iter () do -- for every track/bus
		local i = 0
		while true do
			local proc = r:nth_plugin (i) -- for every plugin
			if proc:isnil () then break end
			local pi = proc:to_insert ()
			if pi:plugin(0):unique_id() == "http://gareus.org/oss/lv2/zeroconvolv#CfgStereo" then
				print (ARDOUR.LuaAPI.get_plugin_insert_property(pi, "http://gareus.org/oss/lv2/zeroconvolv#ir"))
				print (ARDOUR.LuaAPI.set_plugin_insert_property(pi, "http://gareus.org/oss/lv2/zeroconvolv#ir", "/tmp/mono-hall.wav"))
				-- in case of ZeroConvo.lv2 the new value will only be returned once the IR is loaded, which may take some time
				print (ARDOUR.LuaAPI.get_plugin_insert_property(pi, "http://gareus.org/oss/lv2/zeroconvolv#ir"))
			end
			i = i + 1
		end
	end
end end

