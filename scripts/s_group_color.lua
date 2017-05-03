ardour { ["type"] = "Snippet", name = "Randomize Group Colors" }

function factory () return function ()
	for grb in Session:route_groups ():iter () do
		local r = math.random (0, 255)
		local g = math.random (0, 255)
		local b = math.random (0, 255)
		local rgba = bit32.lshift (r, 24) + bit32.lshift (g, 16) + bit32.lshift (b, 8) + 0xff
		grp:set_rgba(rgba)
	end
end end
