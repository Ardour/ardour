ardour {
	["type"]    = "dsp",
	name        = "Notch Bank",
	category    = "Example",
	license     = "MIT",
	author      = "Ardour Lua Task Force",
	description = [[An Example Filter Plugin]]
}

function dsp_ioconfig ()
	return
	{
		{ audio_in = 1, audio_out = 1},
	}
end

function dsp_params ()
	return
	{
		{ ["type"] = "input", name = "Base Freq", min = 10, max = 1000, default = 100, unit="Hz", logarithmic = true },
		{ ["type"] = "input", name = "Quality", min = 1.0, max = 16.0, default = 8.0 },
		{ ["type"] = "input", name = "Stages", min = 1.0, max = 20, default = 6.0, integer = true },
	}
end

local filters = {} -- the biquad filter instances
local freq = 0
local bw = 0

function dsp_init (rate)
	for i = 1,20 do
		filters[i] = ARDOUR.DSP.Biquad (rate)
	end
end

function dsp_run (ins, outs, n_samples)
	assert (#outs == 1)
	assert (n_samples < 8192)

	-- this is quick/dirty: no declick, no de-zipper, no latency reporting,...
	-- and no documentation :)

	local ctrl = CtrlPorts:array() -- get control parameters
	if freq ~= ctrl[1] or bw ~= ctrl[2] then
		freq = ctrl[1]
		bw = ctrl[2]
		for i = 1,20 do
			filters[i]:compute (ARDOUR.DSP.BiquadType.Notch, freq * i, bw, 0)
		end
	end

	if not ins[1]:sameinstance (outs[1]) then
		ARDOUR.DSP.copy_vector (outs[1], outs[1], n_samples)
	end

	local stages = math.floor (ctrl['3'])
	if stages < 1 then stages = 1; end
	if stages > 20 then stages = 20; end

	for i = 1, stages do
		filters[i]:run (outs[1], n_samples)
	end
end
