ardour {
	["type"]    = "dsp",
	name        = "Voice/Level Activate",
	license     = "MIT",
	author      = "Robin Gareus",
	authoremail = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[
	An Example Audio Plugin that rolls the transport
	when the signal level on the plugin's input a given threshold.]]
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
		{ ["type"] = "input", name = "Threshold", min = -20, max = 0, default = -6, doc = "Threshold in dBFS for all channels" },
		{ ["type"] = "output", name = "Level", min = -120, max = 0 },
	}
end

function dsp_configure (ins, outs)
	n_channels = ins:n_audio();
end

-- use ardour's vectorized functions
function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	local ctrl = CtrlPorts:array() -- get control port array (read/write)
	if Session:transport_rolling() then ctrl[2] = -math.huge return end
	local threshold = 10 ^ (.05 * ctrl[1]) -- dBFS to coefficient
	local level = -math.huge
	for c = 1,n_channels do
		local b = in_map:get(ARDOUR.DataType("audio"), c - 1); -- get id of buffer for given cannel
		if b ~= ARDOUR.ChanMapping.Invalid then
			local a = ARDOUR.DSP.compute_peak(bufs:get_audio(b):data(offset), n_samples, 0)
			if a > threshold then
					Session:request_transport_speed(1.0, true)
			end
			if a > level then level = a end
		end
	end
	ctrl[2] = ARDOUR.DSP.accurate_coefficient_to_dB (level)
end
