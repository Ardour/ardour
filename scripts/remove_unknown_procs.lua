ardour { ["type"] = "EditorAction", name = "Remove Unknown Plugins",
	license     = "MIT",
	author      = "Robin Gareus",
	email       = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[Remove all unknown plugins/processors from all tracks and busses]]
}

function factory (params) return function ()
	for route in Session:get_routes ():iter () do
		local i = 0;
		repeat
			proc = route:nth_processor (i)
			if not proc:isnil () and not proc:to_unknownprocessor ():isnil () then
				route:remove_processor (proc, nil, true)
			else
				i = i + 1
			end
		until proc:isnil()
	end
end end
