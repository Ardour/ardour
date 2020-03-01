ardour {
	["type"]    = "dsp",
	name        = "MIDI generator with multiple ports",
	category    = "Example",
	license     = "MIT",
	author      = "R8000",
	description = [[An Example Midi Generator for prototyping.]]
}

function dsp_ioconfig () return { { midi_out = 5} } end

local tme = 0 -- sample-counter
local seq = 0 -- sequence-step
local spb = 0 -- samples per beat

local midi_sequence_one = {
	{ 0x90, 64, 127 },
	{ 0x80, 64,   0 },
}
local midi_sequence_two = {
	{ 0x90, 60,  70 },
	{ 0x80, 60,   0 },
}

local midi_sequences = { midi_sequence_one, midi_sequence_two }

function dsp_init (rate)
	local bpm = 120
	spb = rate * 60 / bpm
	if spb < 2 then spb = 2 end
end

function dsp_run (_, _, n_samples)
	assert (type(midiout) == "table")
	assert (spb > 1)

	for time = 1,n_samples do -- not very efficient
		-- TODO, timestamp the sequence in beats, calc/skip to next event
		tme = tme + 1
		if tme >= spb then
			for m = 1, 5 do
				midiout[m] = {time = time, data = midi_sequences[m % 2 + 1][seq + 1]}
			end
			tme = 0
			seq = (seq + 1) % 2
		end
	end
end
