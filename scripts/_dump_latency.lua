ardour { ["type"] = "Snippet", name = "Dump Latency",
	license     = "MIT",
	author      = "Ardour Team",
}

function factory () return function ()
	print (" -- Session --")
	print ("Worst Output Latency:  ", Session:worst_output_latency ())
	print ("Worst Input Latency:   ", Session:worst_input_latency ())
	print ("Worst Track Latency:   ", Session:worst_track_latency ())
	print ("Worst Playback Latency:", Session:worst_playback_latency ())
	print (" -- Tracks --")
	for t in Session:get_tracks ():iter () do
		print (string.format ("%-24s  roll-delay: %4d  proc: %4d io: %4d", 
		t:name(), t:initial_delay (), t:signal_latency (), t:output():latency()))
	end
end end
