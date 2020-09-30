ardour {
	["type"]    = "dsp",
	name        = "Simple Amp III",
	category    = "Example",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[
	An Example DSP Plugin for processing audio, to
	be used with Ardour's Lua scripting facility.]]
}

function dsp_ioconfig ()
	return
	{
		{ audio_in = -1, audio_out = -1},
	}
end


function dsp_params ()
	return
	{
		{ ["type"] = "input", name = "Gain", min = -20, max = 20, default = 6, unit="dB", scalepoints = { ["0"] = 0, ["twice as loud"] = 6 , ["half as loud"] = -6 } },
	}
end


-- use ardour's vectorized functions
--
-- This is as efficient as Ardour doing it itself in C++
-- Lua function overhead is negligible
--
-- this also exemplifies the /simpler/ way of delegating the
-- channel-mapping to ardour.

function dsp_run (ins, outs, n_samples)
	local ctrl = CtrlPorts:array() -- get control port array (read/write)
	local gain = ARDOUR.DSP.dB_to_coefficient (ctrl[1])
	assert (#ins == #outs) -- ensure that we can run in-place (channel count matches)
	for c = 1,#ins do
		assert (ins[c] == outs[c]) -- check in-place
		ARDOUR.DSP.apply_gain_to_buffer (ins[c], n_samples, gain); -- process in-place
	end
end
