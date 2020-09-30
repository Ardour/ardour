ardour {
	["type"]    = "dsp",
	name        = "ACE Amplifier",
	category    = "Amplifier",
	license     = "MIT",
	author      = "Ardour Community",
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

local sr = 48000
local cur_gain = 1

function dsp_init (rate)
	sr = rate
end

function dsp_configure (ins, outs)
	n_out   = outs
	n_audio = outs:n_audio ()
	assert (outs:n_midi () == 0)
end

-- the DSP callback function
function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	-- apply I/O map
	ARDOUR.DSP.process_map (bufs, n_out, in_map, out_map, n_samples, offset)

	local ctrl = CtrlPorts:array() -- get control port array
	local target_gain  = ARDOUR.DSP.dB_to_coefficient (ctrl[1]) -- 10 ^ (0.05 * ctrl[1])
	local current_gain = cur_gain -- start with the same for all channels
	cur_gain = target_gain -- use target gain if no channel is mapped.

	for c = 1, n_audio do
		local ob = out_map:get (ARDOUR.DataType ("audio"), c - 1); -- get id of mapped output buffer for given cannel
		if (ob ~= ARDOUR.ChanMapping.Invalid) then
			cur_gain = ARDOUR.Amp.apply_gain (bufs:get_audio(ob), sr, n_samples, current_gain, target_gain, offset)
		end
	end

--[[
	if current_gain ~= cur_gain then
		self:queue_draw () -- notify display
	end
--]]
end

-------------------------------------------------------------------------------
--- inline display + text example

--[[
local txt = nil -- cache pango context globally

function render_inline (ctx, w, max_h)
	local ctrl = CtrlPorts:array () -- get control ports

	if not txt then
		-- allocate PangoLayout and set font
		--http://manual.ardour.org/lua-scripting/class_reference/#Cairo:PangoLayout
		txt = Cairo.PangoLayout (ctx, "Mono 8px")
	end

	txt:set_text (string.format ("%+.2f dB", ctrl[1]));
	tw, th = txt:get_pixel_size ()

	local h = th + 4 -- use text-height with 4px padding
	if (h > max_h) then h = max_h end -- but at most max-height

	-- clear background
	ctx:rectangle (0, 0, w, h)
	ctx:set_source_rgba (.2, .2, .2, 1.0)
	ctx:fill ()

	-- center text
	ctx:set_source_rgba (.8, .8, .8, 1.0)
	ctx:move_to (.5 * (w - tw), .5 * (h - th))
	txt:show_in_cairo_context (ctx)

	return {w, h}
end
--]]
