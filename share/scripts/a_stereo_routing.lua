ardour {
	["type"]    = "dsp",
	name        = "ACE Stereo Routing",
	category    = "Utility",
	license     = "MIT",
	author      = "Ardour Community",
	description = [[Stereo Signal Routing, re-assign or mix channels. This is a drop-in replacement for https://x42-plugins.com/x42/x42-stereoroute]]
}

function dsp_ioconfig ()
	return { { audio_in = 2, audio_out = 2} }
end

function dsp_params ()
	return {
		{ ["type"] = "input", name = "Mode", min = 0, max = 8, default = 0, enum = true, scalepoints =
			{
				["No-OP: L->L, R->R (Straight Bypass)"]    = 0,
				["Downmix to Mono -3dB (Equal Power)"]     = 1,
				["Downmix to Mono -6dB (Equal Amplitude)"] = 2,
				["Downmix to Mono -0dB (Sum Channels)"]    = 3,
				["Route Left Only: L->L, L->R"]            = 4,
				["Route Right Only: R->R, R->L"]           = 5,
				["Swap Channels: L->R, R->L"]              = 6,
				["M/S: (L+R)/2, (L-R)/2 (to Mid/Side)"]    = 7,
				["M/S: (L+R), (L-R) (from Mid/Side)"]      = 8,
			}
		}
	}
end

function dsp_init (rate)
	cmem = ARDOUR.DSP.DspShm (8192)
end

function copy_no_inplace (ins, outs, n_samples)
	assert (#ins == #outs)
	for c = 1, #outs do
		if ins[c] ~= outs[c] then
			ARDOUR.DSP.copy_vector (outs[c], ins[c], n_samples) -- ..copy data from input to output.
		end
	end
end

function dsp_run (ins, outs, n_samples)
	assert (n_samples <= 8192)
	local ctrl = CtrlPorts:array()
	local m = ctrl[1]
	if (m < 1) then
		-- NO-OP
		copy_no_inplace (ins, outs, n_samples)
	elseif (m < 2) then
		-- Sum to Mono -3dB
		copy_no_inplace (ins, outs, n_samples)
		ARDOUR.DSP.mix_buffers_no_gain (outs[1], outs[2], n_samples)
		ARDOUR.DSP.apply_gain_to_buffer (outs[1], n_samples, 0.707945784);
		ARDOUR.DSP.copy_vector (outs[2], outs[1], n_samples)
	elseif (m < 3) then
		-- Sum to Mono -6dB
		copy_no_inplace (ins, outs, n_samples)
		ARDOUR.DSP.mix_buffers_no_gain (outs[1], outs[2], n_samples)
		ARDOUR.DSP.apply_gain_to_buffer (outs[1], n_samples, 0.5);
		ARDOUR.DSP.copy_vector (outs[2], outs[1], n_samples)
	elseif (m < 4) then
		-- Sum to Mono
		copy_no_inplace (ins, outs, n_samples)
		ARDOUR.DSP.mix_buffers_no_gain (outs[1], outs[2], n_samples)
		ARDOUR.DSP.copy_vector (outs[2], outs[1], n_samples)
	elseif (m < 5) then
		-- Left only
		if ins[1] ~= outs[1] then
			ARDOUR.DSP.copy_vector (outs[1], ins[1], n_samples)
		end
		ARDOUR.DSP.copy_vector (outs[2], outs[1], n_samples)
	elseif (m < 6) then
		-- Right only
		if ins[2] ~= outs[2] then
			ARDOUR.DSP.copy_vector (outs[2], ins[2], n_samples)
		end
		ARDOUR.DSP.copy_vector (outs[1], outs[2], n_samples)
	elseif (m < 7) then
		-- Swap channels
		ARDOUR.DSP.copy_vector (cmem:to_float(0), ins[1], n_samples)
		ARDOUR.DSP.copy_vector (outs[1], ins[2], n_samples)
		ARDOUR.DSP.copy_vector (outs[2], cmem:to_float(0), n_samples)
	elseif (m < 8) then
		-- to Mid/Side
		ARDOUR.DSP.copy_vector (cmem:to_float(0), ins[1], n_samples)
		ARDOUR.DSP.mix_buffers_with_gain (cmem:to_float(0), ins[2], n_samples, -1) -- (L - R)
		if ins[1] ~= outs[1] then
			ARDOUR.DSP.copy_vector (outs[1], ins[1], n_samples)
		end
		ARDOUR.DSP.mix_buffers_no_gain (outs[1], ins[2], n_samples) -- (L + R)
		ARDOUR.DSP.copy_vector (outs[2], cmem:to_float(0), n_samples)
		ARDOUR.DSP.apply_gain_to_buffer (outs[1], n_samples, 0.5);
		ARDOUR.DSP.apply_gain_to_buffer (outs[2], n_samples, 0.5);
	else
		-- from Mid/Side
		ARDOUR.DSP.copy_vector (cmem:to_float(0), ins[1], n_samples)
		ARDOUR.DSP.mix_buffers_with_gain (cmem:to_float(0), ins[2], n_samples, -1) -- (L - R)
		if ins[1] ~= outs[1] then
			ARDOUR.DSP.copy_vector (outs[1], ins[1], n_samples)
		end
		ARDOUR.DSP.mix_buffers_no_gain (outs[1], ins[2], n_samples) -- (L + R)
		ARDOUR.DSP.copy_vector (outs[2], cmem:to_float(0), n_samples)
	end
end
