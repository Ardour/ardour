ardour { ["type"] = "Snippet", name = "Export Track XML" }

function factory () return function ()
	local rlp = ARDOUR.RouteListPtr ()
	local sel = Editor:get_selection ()
	for r in sel.tracks:routelist ():iter () do
		rlp:push_back (r)
	end
	print (Session:export_track_state (rlp, "/tmp/rexport"))
end end
