ardour {
	["type"]    = "dsp",
	name        = "Simple Amp IV",
	category    = "Amplifier",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Versatile +/- 20dB multichannel amplifier]]
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
		{ ["type"] = "input", name = "Gain", min = -20, max = 20, default = 0, unit="dB"},
	}
end

local lpf = 0.02 -- parameter low-pass filter time-constant
local cur_gain = 0 -- current smoothed gain (dB)

-- called once when plugin is instantiated
function dsp_init (rate)
	lpf = 780 / rate -- interpolation time constant
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
	local ctrl = CtrlPorts:array() -- get control port array (read/write)
	local siz = n_samples -- samples remaining to process
	local off = 0 -- already processed samples
	local changed = false

	-- if the gain parameter was changed, process at most 32 samples at a time
	-- and interpolate gain until the current settings match the target values
	if cur_gain ~= ctrl[1] then
		changed = true
		siz = 32
	end

	while n_samples > 0 do
		if siz > n_samples then siz = n_samples end -- process at most "remaining samples"
		if changed then
			-- smooth gain changes above 0.02 dB difference
			-- XXX this interpolates dB, resulting in a double-exponential fade XXX
			cur_gain = low_pass_filter_param (cur_gain, ctrl[1], 0.02)
		end

		local gain = ARDOUR.DSP.dB_to_coefficient (cur_gain) -- 10 ^ (0.05 * cur_gain)

		for c = 1,#ins do -- process all channels
			-- check if output and input buffers for this channel are identical
			-- http://manual.ardour.org/lua-scripting/class_reference/#C:FloatArray
			if ins[c] ~= outs[c] then
				-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:DSP
				ARDOUR.DSP.copy_vector (outs[c]:offset (off), ins[c]:offset (off), siz)
			end
			ARDOUR.DSP.apply_gain_to_buffer (outs[c]:offset (off), siz, gain); -- apply-gain, process in-place
		end
		n_samples = n_samples - siz
		off = off + siz
	end
end
