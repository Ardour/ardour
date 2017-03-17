ardour { ["type"] = "Snippet", name = "Region Transient List" }

function factory () return function ()
	local sel = Editor:get_selection ()
	for r in sel.regions:regionlist ():iter () do
		local region_pos = r:position()
		local region_off = r:start()
		print (r:name(), r:position(), r:start())
		local trans = r:transients()
		for t in trans:iter() do
			-- print absolute timeline position of transients
			print (t + region_pos - region_off)
		end
		print ("----")
	end
end end
