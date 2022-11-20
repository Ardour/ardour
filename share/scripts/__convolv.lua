ardour { ["type"] = "dsp", name = "Lua Convolver", license = "MIT", author = "Ardour Team", description = [[Reverb - libardour convolver]] }

function dsp_ioconfig () return
	{
		{ audio_in = 1, audio_out = 1},
		{ audio_in = 1, audio_out = 2},
		{ audio_in = 2, audio_out = 2},
	}
end

local conv, buffered

buffered = true

function dsp_configure (ins, outs)
	local mode
	if outs:n_audio() == 1 then
		assert (ins:n_audio() == 1)
		mode = ARDOUR.DSP.IRChannelConfig.Mono
	elseif ins:n_audio() == 1 then
		assert (outs:n_audio() == 2)
		mode = ARDOUR.DSP.IRChannelConfig.MonoToStereo
	else
		assert (ins:n_audio() == 2)
		assert (outs:n_audio() == 2)
		mode = ARDOUR.DSP.IRChannelConfig.Stereo
	end

	local irs = ARDOUR.DSP.IRSettings()

	ir_file = Session:path() .. "/externals/Vocal_Chamber.flac"
	irs.gain = 0.055584
	ir_file = Session:path() .. "/externals/Percussion_Air.flac"
	irs.gain = 0.119054

	conv = ARDOUR.DSP.Convolver (Session, ir_file, mode, irs)
	collectgarbage ()
end

function dsp_latency ()
	if conv and buffered then
		return conv:latency()
	else
		return 0
	end
end

-- the DSP callback function to process audio audio
-- "ins" and "outs" are http://manual.ardour.org/lua-scripting/class_reference/#C:FloatArray
function dsp_run (ins, outs, n_samples)
	for c = 1, #ins do
		if ins[c] ~= outs[c] then -- if processing is not in-place..
			ARDOUR.DSP.copy_vector (outs[c], ins[c], n_samples) -- ..copy data from input to output.
		end
	end

	if buffered then
		if #outs == 1 then
			conv:run_mono_buffered (outs[1], n_samples)
		else
			conv:run_stereo_buffered (outs[1], outs[2], n_samples)
		end
	else
		if #outs == 1 then
			conv:run_mono_no_latency (outs[1], n_samples)
		else
			conv:run_stereo_no_latency (outs[1], outs[2], n_samples)
		end
	end
end
