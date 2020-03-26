ardour {
	["type"]    = "dsp",
	name        = "NoiseGen",
	category    = "Instrument",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Noise Generator (v-1.02)]]
}

function dsp_params ()
	return
	{
		{ ["type"] = "input", name = "White/Pink", min = 0, max = 1, default = 0, toggled = true },
		{ ["type"] = "input", name = "Gain", min = -60, max = 0, default = -18, unit="dB" },
	}
end

function dsp_ioconfig ()
	return { [1] = { audio_in = -1, audio_out = -1}, }
end

local sr = 0

function dsp_init (rate)
	sr = rate
end

local ao = 0
local draw = 0

function dsp_run (ins, outs, n_samples)

	local a = {} -- init array
	local ctrl = CtrlPorts:array ()
	local noise = ctrl[1] or 0
	local amplitude =  ARDOUR.DSP.dB_to_coefficient (ctrl[2]) or ARDOUR.DSP.dB_to_coefficient (-18)

	local b0 = 0.0
	local b1 = 0.0
	local b2 = 0.0
	local b3 = 0.0
	local b4 = 0.0
	local b5 = 0.0
	local b6 = 0.0

	--Pink noise generation courtesy of Paul Kellet's refined method
	--http://www.musicdsp.org/files/pink.txt
	--If 'white' consists of uniform random numbers,
	--the pink noise will have an almost gaussian distribution.
	for s = 1, n_samples do
		if noise == 0 then
			a[s] = amplitude * 2 * (math.random() - 0.5)
		end
		if noise == 1 then
			white = (amplitude * 0.25) * 2 * (math.random() - 0.5)
			b0 = 0.99886 * b0 + white * 0.0555179;
			b1 = 0.99332 * b1 + white * 0.0750759;
			b2 = 0.96900 * b2 + white * 0.1538520;
			b3 = 0.86650 * b3 + white * 0.3104856;
			b4 = 0.55000 * b4 + white * 0.5329522;
			b5 = -0.7616 * b5 - white * 0.0168980;
			b6 = white * 0.115926;
			a[s] = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362;
		end
	end

	if (draw > (sr/15)) then
		self:queue_draw()
		draw = 0
	end

	-- passes array a {} into buffer
	for c = 1,#outs do
		outs[c]:set_table(a, n_samples)
	end
	draw = draw + n_samples
end

function render_inline (ctx, w, max_h) --inline display
	local ctrl = CtrlPorts:array()
	h = 30
	p = 0
	inc = 0
	ycy = 0.5
	pink = false
	local amplitude = ARDOUR.DSP.dB_to_coefficient(ctrl[2])
	if ctrl[1] == 1 then pink = true end
	if pink then inc = 0.7/w end

	--draw rectangle
	ctx:rectangle(0, 0, w, h)
	ctx:set_source_rgba(0, 0, 0, 1.0)
	ctx:fill()
	ctx:set_line_width(1.5)
	ctx:set_source_rgba(0.8, 0.8, 0.8, 1.0)

	l_x = 0
	l_y = 0
	for x = 0,w do
		if pink then ycy = 0.3 else ycy = 0.5 end --slant slightly like an actual pink noise spectrum
		y = math.log(20^amplitude) * (math.random() - 0.5) - p
		yc = ycy * h + ((-0.5 * h) * y)
		ctx:move_to (x, yc + 3)
		ctx:line_to (l_x, l_y + 3)
		l_x = x
		l_y = yc
		ctx:stroke()
		p = p + inc
	end
	return {w, h + 6}
end
