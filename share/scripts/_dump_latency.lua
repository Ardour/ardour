ardour { ["type"] = "Snippet", name = "Dump Latency",
	license     = "MIT",
	author      = "Ardour Team",
}

function factory () return function ()
	local all_procs = true
	local show_ports = true

	print (" -- Session --")
	print ("Worst Output Latency:  ", Session:worst_output_latency ())
	print ("Worst Input Latency:   ", Session:worst_input_latency ())
	print ("Worst Track Latency:   ", Session:worst_route_latency ())
	print ("Worst Latency Preroll: ", Session:worst_latency_preroll ())
	print ("I/O Latency:           ", Session:io_latency ())

	print (" -- Routes --")
	for t in Session:get_routes ():iter () do
		print (string.format ("%-30s  signal-latency: %4d align: %4d play: %4d || in: %4d out: %4d",
		string.sub (t:name(), 0, 30),
		t:signal_latency (), t:playback_latency (false), t:playback_latency (true),
		t:input():latency(), t:output():latency()))
		local i = 0
		while true do
			local proc = t:nth_processor (i)
			if proc:isnil () then break end
			if all_procs and not proc:to_send():isnil () then
				print (string.format (" * %-27s  L: %4d  in: %4d  out: %4d capt: %4d play %4d  DLY-SRC: %4d DLY-DST: %4d",
				string.sub (proc:name(), 0, 27) , proc:signal_latency(),
				proc:input_latency(), proc:output_latency(),
				proc:capture_offset(), proc:playback_offset(),
				proc:to_send():get_delay_in(), proc:to_send():get_delay_out()
				))
			elseif all_procs or not proc:to_diskioprocessor():isnil () then
				print (string.format (" * %-27s  L: %4d  in: %4d  out: %4d capt: %4d play %4d",
				string.sub (proc:name(), 0, 27) , proc:signal_latency(),
				proc:input_latency(), proc:output_latency(),
				proc:capture_offset(), proc:playback_offset()
				))
			end
			i = i + 1
		end
	end

	if show_ports then
		print (" -- Ports -- (latencies: port, priv, pub)")
		local a = Session:engine()
		_, t = a:get_ports (ARDOUR.DataType("audio"), ARDOUR.PortList())
		-- table 't' holds argument references. t[2] is the PortList
		for p in t[2]:iter() do
			local lp = p:get_connected_latency_range (ARDOUR.LatencyRange(), true)
			local lc = p:get_connected_latency_range (ARDOUR.LatencyRange(), false)
			local ppl = p:private_latency_range (true)
			local pcl = p:private_latency_range (false)
			local bpl = p:public_latency_range (true)
			local bcl = p:public_latency_range (false)
			print (string.format ("%-30s  play: (%4d, %4d) (%4d, %4d) (%4d, %4d)  capt: (%4d, %4d) (%4d, %4d) (%4d, %4d)",
			string.sub (p:name(), 0, 30),
			lp[1].min, lp[1].max, ppl.min, ppl.max, bpl.min, bpl.max,
			lc[1].min, lc[1].max, pcl.min, pcl.max, bcl.min, bcl.max))
		end
		_, t = a:get_ports (ARDOUR.DataType("midi"), ARDOUR.PortList())
		-- table 't' holds argument references. t[2] is the PortList
		for p in t[2]:iter() do
			local lp = p:get_connected_latency_range (ARDOUR.LatencyRange(), true)
			local lc = p:get_connected_latency_range (ARDOUR.LatencyRange(), false)
			local ppl = p:private_latency_range (true)
			local pcl = p:private_latency_range (false)
			local bpl = p:public_latency_range (true)
			local bcl = p:public_latency_range (false)
			print (string.format ("%-30s  play: (%4d, %4d) (%4d, %4d) (%4d, %4d)  capt: (%4d, %4d) (%4d, %4d) (%4d, %4d)",
			string.sub (p:name(), 0, 30),
			lp[1].min, lp[1].max, ppl.min, ppl.max, bpl.min, bpl.max,
			lc[1].min, lc[1].max, pcl.min, pcl.max, bcl.min, bcl.max))
		end
	end
end end
