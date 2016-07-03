ardour {
	["type"]    = "dsp",
	name        = "Amplifier",
	category    = "Amplifier",
	license     = "MIT",
	author      = "Robin Gareus",
	email       = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[ Versatile +/- 20dB multichannel amplifier]]
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
		{ ["type"] = "input", name = "Gain", min = -20, max = 20, default = 0, unit="dB"},
	}
end

local lpf = 0.02 -- parameter low-pass filter time-constant
local cur_gain = 0 -- current gain (dB)

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

-- use ardour's vectorized functions
--
-- This is as efficient as Ardour doing it itself in C++
-- Lua function overhead is negligible
--
-- this also exemplifies the /simpler/ way of delegating the
-- channel-mapping to ardour.

function dsp_run (ins, outs, n_samples)
	assert (#ins == #outs) -- ensure that we can run in-place (channel count matches)
	local ctrl = CtrlPorts:array() -- get control port array (read/write)
	local siz = n_samples
	local off = 0
	local changed = false

	-- if the gain parameter was changed, process at most 64 samples at a time
	-- and interpolate gain until the current settings match the target values
	if cur_gain ~= ctrl[1] then
		changed = true
		siz = 32
	end

	while n_samples > 0 do
		if siz > n_samples then siz = n_samples end
		if changed then
			cur_gain = low_pass_filter_param (cur_gain, ctrl[1], 0.05)
		end

		local gain = ARDOUR.DSP.dB_to_coefficient (cur_gain) -- 10 ^ (0.05 * cur_gain)
		for c = 1,#ins do -- process all channels
			-- check if output and input buffers for this channel are identical
			-- http://manual.ardour.org/lua-scripting/class_reference/#C:FloatArray
			if not ins[c]:sameinstance (outs[c]) then
				-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:DSP
				ARDOUR.DSP.copy_vector (outs[c]:offset (off), ins[c]:offset (off), siz)
			end
			ARDOUR.DSP.apply_gain_to_buffer (outs[c]:offset (off), siz, gain); -- process in-place
		end
		n_samples = n_samples - siz
		off = off + siz
	end

	if changed then
		self:queue_draw () -- notify display
	end
end

-------------------------------------------------------------------------------
--- inline display + text example

local txt = nil -- cache globally
function render_inline (ctx, w, max_h)
	local ctrl = CtrlPorts:array ()

	if not txt then
		txt = Cairo.PangoLayout (ctx, "Mono 8px")
	end

	txt:set_text (string.format ("%+.2f dB", ctrl[1]));
	tw, th = txt:get_pixel_size ()

	local h = math.ceil (th + 4) -- use text-height with 4px padding
	if (h > max_h) then h = max_h end -- but at most max-height

	-- clear background
	ctx:rectangle (0, 0, w, h)
	ctx:set_source_rgba (.2, .2, .2, 1.0)
	ctx:fill ()

	-- center text
	ctx:set_source_rgba (.8, .8, .8, 1.0)
	ctx:move_to ( .5 * (w - tw), .5 * (h - th))
	txt:show_in_cairo_context (ctx)

	return {w, h}
end
