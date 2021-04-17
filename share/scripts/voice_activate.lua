ardour {
	["type"]    = "dsp",
	name        = "Voice/Level Activate",
	category    = "Utility",
	author      = "Ardour Team",
	license     = "MIT",
	description = [[Roll the transport when the signal level on the plugin's input exceeds a given threshold.]]
}

function dsp_ioconfig ()
	return
	{
		-- support all in/out as long as input port count equals output port count
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
	n_channels = ins:n_audio()
end

function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	local ctrl = CtrlPorts:array() -- get control port array (read/write)

	if Session:transport_rolling() then
		-- don't do anything if the transport is already rolling
		ctrl[2] = -math.huge -- set control output port value
		return
	end

	local threshold = 10 ^ (.05 * ctrl[1]) -- dBFS to coefficient
	local level = -math.huge

	for c = 1,n_channels do
		local b = in_map:get(ARDOUR.DataType("audio"), c - 1) -- get id of audio-buffer for the given channel
		if b ~= ARDOUR.ChanMapping.Invalid then -- check if channel is mapped
			local a = ARDOUR.DSP.compute_peak(bufs:get_audio(b):data(offset), n_samples, 0) -- compute digital peak
			if a > threshold then
					Session:request_roll (ARDOUR.TransportRequestSource.TRS_UI)
			end
			if a > level then level = a end -- max level of all channels
		end
	end
	ctrl[2] = ARDOUR.DSP.accurate_coefficient_to_dB (level) -- set control output port value
end
