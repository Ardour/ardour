ardour {
	["type"]    = "dsp",
	name        = "ACE Noise",
	category    = "Utility",
	license     = "MIT",
	author      = "Ardour Community",
	description = [[Noise Generator featuring either white noise with uniform or gaussian distribution, or pink-noise. The signal level can be customized.]]
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
		{ ["type"] = "input", name = "Noise Level", min = -80, max = 0, default = -18, unit="dBFS"},
		{ ["type"] = "input", name = "Noise Type", min = 0, max = 2, default = 0, enum = true, scalepoints =
			{
				["White Noise"] = ARDOUR.DSP.NoiseType.UniformWhiteNoise,
				["Gaussian White Noise"] = ARDOUR.DSP.NoiseType.GaussianWhiteNoise,
				["Pink Noise"] = ARDOUR.DSP.NoiseType.PinkNoise,
			}
		},
	}
end

local cmem  = nil
local gen   = nil
local noise = 0

function dsp_init (rate)
	cmem = ARDOUR.DSP.DspShm (8192)
	gen = ARDOUR.DSP.Generator ()
end

function dsp_run (ins, outs, n_samples)
	local ctrl = CtrlPorts:array()
	local lvl = ARDOUR.DSP.dB_to_coefficient (ctrl[1])
	if (noise ~= ctrl[2]) then
		noise = ctrl[2]
		gen:set_type (noise)
	end
	for c = 1,#ins do
		if ins[c] ~= outs[c] then
			ARDOUR.DSP.copy_vector (outs[c], ins[c], n_samples)
		end
		gen:run (cmem:to_float(0), n_samples)
		ARDOUR.DSP.mix_buffers_with_gain (outs[c], cmem:to_float(0), n_samples, lvl)
	end
end
