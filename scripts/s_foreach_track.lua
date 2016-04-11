ardour { ["type"] = "Snippet", name = "Foreach Track" }

function factory () return function ()
	for r in Session:get_tracks():iter() do
		print (r:name())
		-- see http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Track
		-- for available methods e.g.
		r:set_active (true, nil)
	end
end end
