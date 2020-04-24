ardour {
	["type"]    = "dsp",
	name        = "SinGen",
	category    = "Instrument",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Simple sine wave generator with gain and frequency controls]]
}

local lpf = 0

function dsp_params ()
	return
	{
		{ ["type"] = "input", name = "Frequency", min = 20, max = 20000, default = 1000, unit="Hz", logarithmic = true },
		{ ["type"] = "input", name = "Gain", min = -90, max = 0, default = -18, unit="dB" },
	}
end

function dsp_ioconfig ()
	return { [1] = { audio_in = -1, audio_out = -1}, }
end

function dsp_init (rate)
	r = rate
	lpf  = 2048 / rate
end

function low_pass_filter_param(old, new, limit)
	if math.abs (old - new) < limit  then
		return new
	else
		return old + lpf * (new - old)
	end
end

local p  = 0
local fo = 0
local ao = 0

function dsp_run (ins, outs, n_samples)
	local ctrl = CtrlPorts:array() --call parameters
	
	local a = {} --init array
	local f = ctrl[1] or 1000
	local amp =  low_pass_filter_param(ao, ARDOUR.DSP.dB_to_coefficient(ctrl[2]), 0.02)
	local inc = f / r

	for s = 1, n_samples do --fill table with fragments of a sine wave
		p = p + inc
		a[s] = amp * math.sin(p * (2 * math.pi))
	end
	
	for c = 1,#outs do
		outs[c]:set_table(a, n_samples) --passes array into buffer
	end
	
	if (f ~= fo) or (a ~= ao) then
		self:queue_draw()
	end
	fo = f
	ao = amp
end

function render_inline (ctx, w, max_h) --inline display
	local ctrl = CtrlPorts:array()
	h = 30
	p = 0
	inc = 1/w
	f = ctrl[1] / 1000
	if f < 0.5 then f = 0.5 end
	if f > 8 then f  = 8 end
	
	--draw rectangle
	ctx:rectangle(0, 0, w, h)
	ctx:set_source_rgba(0, 0, 0, 1.0)
	ctx:fill()
	ctx:set_line_width(1.5)
	ctx:set_source_rgba(0.8, 0.8, 0.8, 1.0)
	
	l_x = 0
	l_y = 0
	for x = 0,w do
		y = ARDOUR.DSP.dB_to_coefficient(ctrl[2]) * math.sin(f * (2 * math.pi * (p)))
		yc = 0.5 * h + ((-0.5 * h) * y)
		ctx:move_to (x, yc + 3)
		ctx:line_to (l_x, l_y + 3)
		l_x = x
		l_y = yc
		ctx:stroke()
		p = p + inc
	end
	return {w, h + 6}
end
