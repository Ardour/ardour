ardour {
	["type"]    = "dsp",
	name        = "MIDI Generator",
	category    = "Example", -- "Utility"
	license     = "MIT",
	author      = "Ardour Team",
	description = [[An Example Midi Generator for prototyping.]]
}

function dsp_ioconfig ()
	return { { midi_out = 1, audio_in = 0, audio_out = 0}, }
end

local tme = 0 -- sample-counter
local seq = 1 -- sequence-step
local spb = 0 -- samples per beat

local midi_sequence = {
	{ 0x90, 64, 127 },
	{ 0x80, 64,   0 },
}

function dsp_init (rate)
	local bpm = 120
	spb = rate * 60 / bpm
	if spb < 2 then spb = 2 end
end

function dsp_run (_, _, n_samples)
	assert (type(midiout) == "table")
	assert (spb > 1)
	local m = 1

	for time = 1,n_samples do -- not very efficient
		-- TODO, timestamp the sequence in beats, calc/skip to next event
		tme = tme + 1

		if tme >= spb then
			midiout[m] = {}
			midiout[m]["time"] = time
			midiout[m]["data"] = midi_sequence[seq]

			tme = 0
			m = m + 1
			if seq == #midi_sequence then seq = 1 else seq = seq + 1 end
		end
	end
end
