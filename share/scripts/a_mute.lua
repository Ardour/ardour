ardour {
	["type"]    = "dsp",
	name        = "ACE Mute",
	category    = "Amplifier",
	license     = "MIT",
	author      = "Ardour Community",
	description = [[Auotomatable Mute/Gate]]
}

function dsp_ioconfig ()
	-- -1, -1 = any number of channels as long as input and output count matches
	return { { audio_in = -1, audio_out = -1} }
end

function dsp_params ()
	return { { ["type"] = "input", name = "Mute", min = 0, max = 1, default = 0, toggled = true } }
end

local sr = 48000
local cur_gain = 0.0

function dsp_init (rate)
	sr = rate
end

function dsp_configure (ins, outs)
	n_out   = outs
	n_audio = outs:n_audio ()
	n_midi  = outs:n_midi ()
	assert (n_midi == 0)
end

-- the DSP callback function
function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	local ctrl = CtrlPorts:array() -- get control port array
	local target_gain = ctrl[1] > 0 and 0.0 or 1.0; -- when muted, target_gain = 0.0; otherwise use 1.0
	-- apply I/O map
	ARDOUR.DSP.process_map (bufs, n_out, in_map, out_map, n_samples, offset)

	local g = cur_gain
	for c = 1, n_audio do
		local ob = out_map:get (ARDOUR.DataType ("audio"), c - 1); -- get id of mapped output buffer for given cannel
		if (ob ~= ARDOUR.ChanMapping.Invalid) then
			cur_gain = ARDOUR.Amp.apply_gain (bufs:get_audio(ob), sr, n_samples, g, target_gain, offset)
		end
	end

	-- This plugin doesn't allow MIDI I/O, but it could:
	--[[
	if (target_gain == 0) then
		for c = 1, n_midi do
			local ob = out_map:get (ARDOUR.DataType ("midi"), c - 1);
			if (ob ~= ARDOUR.ChanMapping.Invalid) then
				bufs:get_midi(ob):silence (n_samples, offset)
			end
		end
	end
	--]]
end
