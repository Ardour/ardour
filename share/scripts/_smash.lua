ardour { ["type"] = "dsp", name = "Sound Smasher", category = "Dynamics", license = "MIT", author = "Ardour Team", description = [[Another simple DSP example]] }

function dsp_ioconfig () return
	-- -1, -1 = any number of channels as long as input and output count matches
	{ { audio_in = -1, audio_out = -1}, }
end


-- the DSP callback function to process audio audio
-- "ins" and "outs" are http://manual.ardour.org/lua-scripting/class_reference/#C:FloatArray
function dsp_run (ins, outs, n_samples)
	for c = 1, #outs do -- for each output channel (count from 1 to number of output channels)

		if ins[c] ~= outs[c] then -- if processing is not in-place..
			ARDOUR.DSP.copy_vector (outs[c], ins[c], n_samples) -- ..copy data from input to output.
		end

		-- direct audio data access, in-place processing of output buffer
		local buf = outs[c]:array() -- get channel's 'c' data as lua array reference

		-- process all audio samples
		for s = 1, n_samples do
			buf[s] = math.atan (1.5707 * buf[s]) -- some non-linear gain.

			-- NOTE: doing the maths per sample in lua is not super-efficient
			-- (vs C/C++ vectorized functions -- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:DSP)
			-- but it is very convenient, especially for prototypes and quick solutions.
		end

	end
end
