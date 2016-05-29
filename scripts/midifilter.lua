ardour {
	["type"]    = "dsp",
	name        = "Midi Filter",
	category    = "Utility",
	license     = "MIT",
	author      = "Robin Gareus",
	email       = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[An Example Midi Filter for prototyping.]]
}

function dsp_ioconfig ()
	return { { audio_in = 0, audio_out = 0}, }
end

function dsp_has_midi_input () return true end
function dsp_has_midi_output () return true end

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

		if (#d == 3 and bit32.band (d[1], 240) == 144) then -- note on
			tx_midi (t, d)
		end
		if (#d == 3 and bit32.band (d[1], 240) == 128) then -- note off
			tx_midi (t, d)
		end
	end
end
