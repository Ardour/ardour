ardour {
	["type"]    = "dsp",
	name        = "MIDI LFO",
	category    = "Example", -- Utility
	license     = "MIT",
	author      = "Ardour Team",
	description = [[MIDI CC LFO Example -- Triangle full scale CC Parameter automation]]
}

function dsp_ioconfig ()
	return { { midi_in = 1, midi_out = 1, audio_in = 0, audio_out = 0}, }
end

function dsp_params ()
	return
	{
		{ ["type"] = "input", name = "BPM", min = 40, max = 200, default = 60, unit="BPM"},
		{ ["type"] = "input", name = "CC",  min = 0, max = 127,  default = 1, integer = true },
	}
end

local samplerate
local time = 0
local step = 0

function dsp_init (rate)
	samplerate = rate
	local bpm = 120
	spb = rate * 60 / bpm
end

function dsp_run (_, _, n_samples)
	assert (type(midiin) == "table")
	assert (type(midiout) == "table")

	local ctrl = CtrlPorts:array ()
	local bpm = ctrl[1]
	local cc  = ctrl[2]

	local spb = samplerate * 60 / bpm -- samples per beat
	local sps = spb / 254 -- samples per step (0..127..1 = 254 steps)

	assert (sps > 1)
	local i = 1
	local m = 1

	for ts = 1, n_samples do
		time = time + 1

		-- forward incoming midi
		if i <= #midiin then
			while midiin[i]["time"] == ts do
				midiout[m] = midiin[i]
				i = i + 1
				m = m + 1
				if i > #midiin then break end
			end
		end

		-- inject LFO events
		if time >= spb then
			local val
			if step > 127 then val = 254 - step else val = step end

			midiout[m] = {}
			midiout[m]["time"] = ts
			midiout[m]["data"] = { 0xb0, cc, val }

			m = m + 1
			time = time - sps
			if step == 253 then step = 0 else step = step + 1 end
		end
	end
end
