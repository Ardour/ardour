ardour { ["type"] = "EditorAction", name = "Remove Unknown Plugins",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Remove all unknown plugins/processors from all tracks and busses]]
}

function factory (params) return function ()
	-- iterate over all tracks and busses
	for route in Session:get_routes ():iter () do
		-- route is-a http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Route
		local i = 0;
		repeat
			proc = route:nth_processor (i)
			-- proc is a http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Processor
			-- try cast it to http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:UnknownProcessor
			if not proc:isnil () and not proc:to_unknownprocessor ():isnil () then
				route:remove_processor (proc, nil, true)
			else
				i = i + 1
			end
		until proc:isnil ()
	end
end end
