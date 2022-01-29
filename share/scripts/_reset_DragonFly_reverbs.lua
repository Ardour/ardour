ardour {
	["type"]    = "EditorAction",
	name        = "Reset DragonFly Reverbs",
	license     = "MIT",
	author      = "Vincent Tassy",
	description = [[Switches OFF then ON the DragonFly plugins to stop them creating xruns]]
}

function factory () return function ()

	for r in Session:get_routes ():iter () do -- iterate over all tracks in the session
            local i = 0;
            repeat -- iterate over all plugins/processors
		local proc = r:nth_processor (i)
		if not proc:isnil () then
                   if (string.match (proc:name(), "Dragonfly")) then
		     	  print(r:name(), " -> Deactivating ", proc:name())
			  proc:deactivate()
                    end
		end
		i = i + 1
            until proc:isnil ()
        end
ARDOUR.LuaAPI.usleep(1000000)
	for r in Session:get_routes ():iter () do -- iterate over all tracks in the session
            local i = 0;
            repeat -- iterate over all plugins/processors
		local proc = r:nth_processor (i)
		if not proc:isnil () then
                   if (string.match (proc:name(), "Dragonfly")) then
		     	  print(r:name(), " -> Activating ", proc:name())
			  proc:activate()
			  ARDOUR.LuaAPI.usleep(500000)
                    end
		end
		i = i + 1
            until proc:isnil ()
        end
end end