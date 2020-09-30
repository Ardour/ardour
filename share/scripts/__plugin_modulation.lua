--- session-script example to modulate plugin parameter(s) globally
--
-- Ardour > Menu > Session > Scripting > Add Lua Script
--   "Add" , select "Modulate Plugin Parameter",  click "Add" + OK.
--
-----------------------------------------------------------------------------
-- This script currently assumes you have a track named "Audio"
-- which as a plugin at the top, where the first parameter has a range > 200
-- e.g. "No Delay Line"
--
-- edit below..


-- plugin descriptor
ardour {
	["type"]    = "session",
	name        = "Modulate Plugin Parameter",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[An example session to modulate a plugin parameter.]]
}

function factory () -- generate a new script instance

	local count = 0 -- script-instance "global" variable

	-- the "run" function called at the beginning of every process cycle
	return function (n_samples)
		count = (count + 1) % 200; -- count process cycles
		local tri = math.abs (100 - count) -- triangle wave 0..100

		-- get the track named "Audio"
		-- see also http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Session
		-- and http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Route
		local route = Session:route_by_name ("Audio")
		assert (not route:isnil ()) -- make sure it exists

		-- the 1st plugin (from top) on that track, ardour starts counting at zero
		-- see also http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Processor
		local plugin = route:nth_plugin (0)
		assert (not plugin:isnil ()) -- make sure it exists

		-- modulate the plugin's first parameter (0)  from  200 .. 300
		ARDOUR.LuaAPI.set_processor_param (plugin, 0, 200.0 + tri)
	end
end
