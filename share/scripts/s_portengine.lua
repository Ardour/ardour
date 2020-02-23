ardour { ["type"] = "Snippet", name = "portengine" }
function factory () return function ()

	local a = Session:engine()
	print ("----- Port objects from Ardour's engine ----");
	_, t = a:get_ports (ARDOUR.DataType("audio"), ARDOUR.PortList())
	-- table 't' holds argument references. t[2] is the PortList
	for p in t[2]:iter() do
		local lp = p:get_connected_latency_range (ARDOUR.LatencyRange(), true)
		local lc = p:get_connected_latency_range (ARDOUR.LatencyRange(), false)
		print (p:name(), " -- Play lat.", lp[1].min, lp[1].max, "Capt lat.", lc[1].min, lc[1].max)
	end

	print ("----- Port names queries from the backend ----");
	_, t = a:get_backend_ports ("", ARDOUR.DataType("audio"), 0, C.StringVector())
	-- table 't' holds argument references. t[4] is the StringVector
	for n in t[4]:iter() do
		print (n)
	end

	print ("----- Connections from the backend ----");
	_, t = a:get_backend_ports ("", ARDOUR.DataType("audio"), ARDOUR.PortFlags.IsOutput, C.StringVector())
	for n in t[4]:iter() do
		local printed_name = false;
		local _, ct = a:get_connections (n, C.StringVector())
		for c in ct[2]:iter() do
			if (not printed_name) then
				printed_name = true;
				print (n)
			end
			print (" ->", c)
		end
	end

end end
