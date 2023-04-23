ardour {
	["type"]    = "dsp",
	name        = "ACE 5.1 to Stereo",
	category    = "Amplifier",
	license     = "MIT",
	author      = "Ardour Community",
	description = [[Downmix 5.1 Surround to Stereo]]
}

function dsp_ioconfig ()
	return {
		connect_all_audio_outputs = true, -- override strict-i/o
		{ audio_in = 6, audio_out = 2},
	}
end

function dsp_params ()
	return {
		{ ["type"] = "input", name = "Channel Layout", min = 0, max = 1, default = 0, enum = true, scalepoints =
			{
				["L R C LFE Ls Rs"] = 0,
				["L C R Ls Lr LFE"] = 1,
			}
		},
		{ ["type"] = "input", name = "Normalize", min = 0, max = 1, default = 0, toggled = true },
		{ ["type"] = "input", name = "Include LFE", min = 0, max = 1, default = 0, toggled = true },
	}
end

-- see ATSCA/52:2018 Chapter 7.8.2
clev = 0.707945784 -- -3dB
slev = 0.707945784 -- -3dB
norm = 1.0 / (1 + clev + slev)
normlf = 1.0 / (1 + 2 * clev + slev)

-- the DSP callback function
function dsp_run (ins, outs, n_samples)
	local ctrl = CtrlPorts:array() -- get control port array
	assert (#ins == 6)
	assert (#outs == 2)

	if ctrl[1] == 0 then
		-- L R C LFE Ls Rs
		if ins[1] ~= outs[1] then
			ARDOUR.DSP.copy_vector (outs[1], ins[1], n_samples) -- left to left
		end
		if ins[2] ~= outs[2] then
			ARDOUR.DSP.copy_vector (outs[2], ins[2], n_samples) -- right to right
		end
		ARDOUR.DSP.mix_buffers_with_gain (outs[1], ins[3], n_samples, clev) -- C to left
		ARDOUR.DSP.mix_buffers_with_gain (outs[2], ins[3], n_samples, clev) -- C to right
		ARDOUR.DSP.mix_buffers_with_gain (outs[1], ins[5], n_samples, slev) -- Ls
		ARDOUR.DSP.mix_buffers_with_gain (outs[2], ins[6], n_samples, slev) -- Rs
		if ctrl[3] > 0 then
			ARDOUR.DSP.mix_buffers_with_gain (outs[1], ins[4], n_samples, clev) -- LFE to left
			ARDOUR.DSP.mix_buffers_with_gain (outs[2], ins[4], n_samples, clev) -- LFE to right
		end
	else
		-- L C R Ls Rs LFE
		if ins[1] ~= outs[1] then
			ARDOUR.DSP.copy_vector (outs[1], ins[1], n_samples) -- left to left
		end
		if ins[2] ~= outs[2] then
			ARDOUR.DSP.copy_vector (outs[2], ins[2], n_samples) -- copy center
		end
		ARDOUR.DSP.apply_gain_to_buffer (outs[2], n_samples, clev); -- C to right
		ARDOUR.DSP.mix_buffers_no_gain (outs[1], outs[2], n_samples) -- C to left
		ARDOUR.DSP.mix_buffers_no_gain (outs[2], ins[3], n_samples) -- right to right
		ARDOUR.DSP.mix_buffers_with_gain (outs[1], ins[4], n_samples, slev) -- Ls
		ARDOUR.DSP.mix_buffers_with_gain (outs[2], ins[5], n_samples, slev) -- Rs
		if ctrl[3] > 0 then
			ARDOUR.DSP.mix_buffers_with_gain (outs[1], ins[6], n_samples, clev) -- LFE to left
			ARDOUR.DSP.mix_buffers_with_gain (outs[2], ins[6], n_samples, clev) -- LFE to right
		end
	end

	if ctrl[2] > 0 then
		if ctrl[3] > 0 then
			ARDOUR.DSP.apply_gain_to_buffer (outs[1], n_samples, normlf);
			ARDOUR.DSP.apply_gain_to_buffer (outs[2], n_samples, normlf);
		else
			ARDOUR.DSP.apply_gain_to_buffer (outs[1], n_samples, norm);
			ARDOUR.DSP.apply_gain_to_buffer (outs[2], n_samples, norm);
		end
	end

end
