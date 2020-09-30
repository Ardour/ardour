ardour {
	["type"]    = "dsp",
	name        = "Spike Synth",
	category    = "Instrument",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[A debug and test-instrumentation synth. This plugin is useful with Ardour's "Dummy" backend "Engine-Pulse" mode to verify capture alignment. This plugin generate the exact same audio-signal from MIDI data that the backend also generates: Note-on: +1, Note-off: -1.]]
}

function dsp_ioconfig ()
	return { { midi_in = 1, audio_in = 0, audio_out =  1} }
end

function dsp_run (ins, outs, n_samples)
	local a = {}
	for s = 1, n_samples do a[s] = 0 end

	for c = 1,#outs do
		ARDOUR.DSP.memset (outs[c], 0, n_samples)
	end

	assert (type(midiin) == "table")
	for _,b in pairs (midiin) do
		local t = b["time"] -- t = [ 1 .. n_samples ]
		local d = b["data"] -- get midi-event
		if (#d == 3 and (d[1] & 240) == 144) then -- note on
			for c = 1,#outs do
				outs[c]:array()[t] = 1.0
			end
		end
		if (#d == 3 and (d[1] & 240) == 128) then -- note off
			for c = 1,#outs do
				outs[c]:array()[t] = -1.0
			end
		end
	end
end
