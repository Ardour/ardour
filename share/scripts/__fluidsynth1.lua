ardour {
	["type"]    = "dsp",
	name        = "Lua Fluid Synth",
	category    = "Instrument",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[An Example Synth for Prototyping.]]
}

function dsp_ioconfig ()
	return
	{
		{ midi_in = 1, audio_in = 0, audio_out =  2},
	}
end

fluidsynth = nil

function dsp_init (rate)
	fluidsynth = ARDOUR.FluidSynth (rate, 32)
	assert (fluidsynth:load_sf2 ("/usr/share/sounds/sf2/FluidR3_GM.sf2"))
end

function dsp_run (ins, outs, n_samples)
	local tme = 1
	assert (#outs == 2)

	-- parse midi messages
	assert (type(midiin) == "table") -- global table of midi events (for now)
	for _, e in pairs (midiin) do
		local t = e["time"] -- t = [ 1 .. n_samples ]

		-- synth sound until event
		if t > tme then
			local off = tme - 1
			local len = t - tme
			fluidsynth:synth (outs[1]:offset (off), outs[2]:offset (off), len)
		end

		tme = t + 1

		fluidsynth:midi_event (e["bytes"], e["size"]) -- parse midi event
		end

	-- synth rest of cycle
	if tme <= n_samples then
		local off = tme - 1
		local len = 1 + n_samples - tme
		fluidsynth:synth (outs[1]:offset (off), outs[2]:offset (off), len)
	end
end
