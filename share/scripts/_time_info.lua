ardour {
	["type"]    = "dsp",
	name        = "Time Info",
	category    = "Utility",
	author      = "Ardour Team",
	license     = "MIT",
	description = [[Example to use processing time info]]
}

function dsp_ioconfig ()
	return { { midi_in = 1, midi_out = 1, audio_in = -1, audio_out = -1}, }
end

function dsp_options ()
	return { time_info = true }
end

function dsp_run (_, _, n_samples)
	assert (type(midiout) == "table")
	assert (type(time) == "table")
	assert (type(midiout) == "table")
	local cnt = 1;

	function tx_midi (time, data)
		midiout[cnt] = {}
		midiout[cnt]["time"] = time;
		midiout[cnt]["data"] = data;
		cnt = cnt + 1;
	end

	-- printing from rt-context is not thread-safe
	print ("---")
	for k,v in pairs (time) do
		print (k, v);
	end

	-- pass-thru MIDI
	for _,b in pairs (midiin) do
		local t = b["time"] -- t = [ 1 .. n_samples ]
		local d = b["data"] -- midi-event data
		tx_midi (t, d)
	end

end
