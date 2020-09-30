ardour {
	["type"]    = "dsp",
	name        = "Midi Filter",
	category    = "Example", -- "Utility"
	license     = "MIT",
	author      = "Ardour Team",
	description = [[An Example Midi Filter for prototyping.]]
}

function dsp_ioconfig ()
	return { { midi_in = 1, midi_out = 1, audio_in = 0, audio_out = 0}, }
end

function dsp_run (_, _, n_samples)
	assert (type(midiin) == "table")
	assert (type(midiout) == "table")
	local cnt = 1;

	function tx_midi (time, data)
		midiout[cnt] = {}
		midiout[cnt]["time"] = time;
		midiout[cnt]["data"] = data;
		cnt = cnt + 1;
	end

	-- for each incoming midi event
	for _,b in pairs (midiin) do
		local t = b["time"] -- t = [ 1 .. n_samples ]
		local d = b["data"] -- get midi-event
		local event_type
		if #d == 0 then event_type = -1 else event_type = d[1] >> 4 end

		if (#d == 3 and event_type == 9) then -- note on
			tx_midi (t, d)
		elseif (#d == 3 and event_type == 8) then -- note off
			tx_midi (t, d)
		end
	end
end
