ardour {
	["type"]    = "dsp",
	name        = "ACE Mono to Stereo Pan",
	category    = "Utility",
	license     = "MIT",
	author      = "Ardour Community",
}

function dsp_ioconfig ()
	return {
		connect_all_audio_outputs = true, -- override strict-i/o
		{ audio_in = 1, audio_out = 2}
	}
end

function dsp_params ()
	return {
		{ ["type"] = "input", name = "Pan", min = 0, max = 1, default = 0.5 },
	}
end

local current_gain_l = 0.7071
local current_gain_r = 0.7071
local sample_rate    = 48000
local scale          = -0.831783138

function dsp_init (rate)
	sample_rate = rate
end

function dsp_configure (ins, outs)
	n_out = outs
end

function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	ARDOUR.DSP.process_map (bufs, n_out, in_map, out_map, n_samples, offset) -- apply pin connections
	local input = in_map:get (ARDOUR.DataType ("audio"), 0) -- get id of mapped  buffer for given channel
	local out_l = out_map:get (ARDOUR.DataType ("audio"), 0)
	local out_r = out_map:get (ARDOUR.DataType ("audio"), 1)

	local ctrl      = CtrlPorts:array() -- get parameters
	local pan_right = ctrl[1]
	local pan_left  = 1 - pan_right

	local target_gain_l = pan_left * (scale * pan_left + 1.0 - scale)
	local target_gain_r = pan_right * (scale * pan_right + 1.0 - scale)

	ARDOUR.DSP.copy_vector (bufs:get_audio(out_r):data(offset), bufs:get_audio(out_l):data (offset), n_samples)

	current_gain_l = ARDOUR.Amp.apply_gain (bufs:get_audio(out_l), sample_rate, n_samples, current_gain_l, target_gain_l, offset)
	current_gain_r = ARDOUR.Amp.apply_gain (bufs:get_audio(out_r), sample_rate, n_samples, current_gain_r, target_gain_r, offset)
end
