ardour {
	["type"]    = "dsp",
	name        = "a-Gain Ratio",
	category    = "Amplifier",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Multichannel amplifier with gain coefficient ratio (not dezippered). Beware this plugin allows for significant gain ratios, it's intended to academic purposes.]]
}

function dsp_ioconfig ()
	return
	{
		-- -1, -1 = any number of channels as long as input and output count matches
		{ audio_in = -1, audio_out = -1},
	}
end

function dsp_params ()
	return
	{
		{ ["type"] = "input", name = "Gain Coefficient numerator", min = 0, max = 1048576, default = 1, unit="", logarithmic = true},
		{ ["type"] = "input", name = "Gain coefficient denominator", min = 1, max = 1048576, default = 1, unit="", logarithmic = true},
	}
end

local sr = 48000
local cur_gain = 0.0

function dsp_init (rate)
	sr = rate
end

function dsp_configure (ins, outs)
	n_out   = outs
	n_audio = outs:n_audio ()
end

function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	local ctrl = CtrlPorts:array()
	local gain = ctrl[1] / ctrl[2]
	ARDOUR.DSP.process_map (bufs, n_out, in_map, out_map, n_samples, offset)
	for c = 1, n_audio do
		local ob = out_map:get (ARDOUR.DataType ("audio"), c - 1); -- get id of mapped output buffer for given cannel
		if (ob ~= ARDOUR.ChanMapping.Invalid) then
			bufs:get_audio (ob):apply_gain (gain, n_samples);
		end
	end
end
