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
		-- allow any number of I/O as long as port-count matches
		{ audio_in = -1, audio_out = -1},
	}
end

function dsp_params ()
	return
	{
		{ ["type"] = "input", name = "Base Freq", min = 10, max = 1000, default = 100, unit="Hz", logarithmic = true },
		{ ["type"] = "input", name = "Quality", min = 1.0, max = 16.0, default = 8.0 },
		{ ["type"] = "input", name = "Stages", min = 1.0, max = 100, default = 8.0, integer = true },
	}
end

local filters = {} -- the biquad filter instances
local sample_rate = 0
local chn = 0 -- channel count
local max_stages = 100
local freq = 0
local qual = 0

function dsp_init (rate)
	sample_rate = rate
end

function dsp_configure (ins, outs)
	assert (ins:n_audio () == outs:n_audio ())
	chn = ins:n_audio ()
	for c = 1, chn do
		filters[c] = {}
		for i = 1, max_stages do
			filters[c][i] = ARDOUR.DSP.Biquad (sample_rate)
		end
	end
end

function dsp_run (ins, outs, n_samples)
	assert (#ins == chn)
	assert (n_samples < 8192)

	-- this is quick/dirty: no declick, no de-zipper, no latency reporting,...
	-- and no documentation :)

	local ctrl = CtrlPorts:array() -- get control parameters
	if freq ~= ctrl[1] or qual ~= ctrl[2] then
		freq = ctrl[1]
		qual = ctrl[2]
		for c = 1, chn do
			for i = 1, max_stages do
				filters[c][i]:compute (ARDOUR.DSP.BiquadType.Notch, freq * i, qual, 0)
			end
		end
	end

	local limit = math.floor (sample_rate / ( 2 * freq ))
	local stages = math.floor (ctrl['3'])
	if stages < 1 then stages = 1 end
	if stages > max_stages then stages = max_stages end
	if stages > limit then stages = limit end

	-- process all channels
	for c = 1, chn do
		if not ins[c]:sameinstance (outs[c]) then
			ARDOUR.DSP.copy_vector (outs[c], outs[c], n_samples)
		end

		for i = 1, stages do
			filters[c][i]:run (outs[c], n_samples)
		end
	end
end
