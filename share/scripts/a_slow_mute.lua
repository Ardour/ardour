ardour {
	["type"]    = "dsp",
	name        = "ACE Slow-Mute",
	category    = "Amplifier",
	license     = "MIT",
	author      = "Ardour Community",
	description = [[Mute button with slow fade in/out (approx. 1sec exponential)]]
}

function dsp_ioconfig ()
	-- -1, -1 = any number of channels as long as input and output count matches
	return { { audio_in = -1, audio_out = -1} }
end


function dsp_params ()
	return { { ["type"] = "input", name = "Mute", min = 0, max = 1, default = 0, toggled = true } }
end

local cur_gain = 1
local lpf = 0.002 -- parameter low-pass filter time-constant

function dsp_init (rate)
	lpf = 100 / rate -- interpolation time constant
end

function low_pass_filter_param (old, new, limit)
	if math.abs (old - new) < limit  then
		return new
	else
		return old + lpf * (new - old)
	end
end

-- the DSP callback function
-- "ins" and "outs" are http://manual.ardour.org/lua-scripting/class_reference/#C:FloatArray
function dsp_run (ins, outs, n_samples)
	local ctrl = CtrlPorts:array() -- get control port array
	local target_gain = ctrl[1] > 0 and 0.0 or 1.0;  -- when muted, target_gain = 0.0; otherwise use 1.0
	local siz = n_samples -- samples remaining to process
	local off = 0 -- already processed samples
	local changed = false

	-- if the target gain changes, process at most 32 samples at a time,
	-- and interpolate gain until the current settings match the target values
	if cur_gain ~= target_gain then
		changed = true
		siz = 32
	end

	while n_samples > 0 do
		if siz > n_samples then siz = n_samples end
		if changed then
			cur_gain = low_pass_filter_param (cur_gain, target_gain, 0.001)
		end

		for c = 1,#ins do -- process all channels
			if ins[c] ~= outs[c] then
				ARDOUR.DSP.copy_vector (outs[c]:offset (off), ins[c]:offset (off), siz)
			end
			ARDOUR.DSP.apply_gain_to_buffer (outs[c]:offset (off), siz, cur_gain);
		end
		n_samples = n_samples - siz
		off = off + siz
	end
end
