ardour { ["type"] = "Snippet", name = "Export Track XML" }

function factory () return function ()
	local rlp = ARDOUR.RouteListPtr ()
	local sel = Editor:get_selection ()
	for r in sel.tracks:routelist ():iter () do
		rlp:push_back (r)
	end
	print (Session:export_route_state (rlp, "/tmp/rexport", false))
	--[[

	local idmap = ARDOUR.IDMap ()
	local nm = Session:parse_route_state ("/tmp/rexport/rexport.routestate", false)
	for id, name in pairs (nm:table()) do
		print (id:to_s(), name)
		idmap:add ({[id] = id})
	end

	print (Session:import_route_state ("/tmp/rexport/rexport.routestate", idmap))

	--]]
end end
