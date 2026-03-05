ardour { ["type"] = "dsp", name = "Lua B-Format Reverb", license = "MIT", author = "Ardour Team", description = [[Another simple DSP example]] }

function dsp_ioconfig () return
	{
		connect_all_audio_outputs = true, -- override strict-i/o
		{ audio_in = 5, audio_out = 4},
		--[[
		    1 In.Tail, 2 In.W, 3 In.X, 4 In.Y, 5 In.Z
		    1 Out.W, 2 Out.X, 3 Out.Y, 4 Out.Z
		--]]
	}
end

local conv

function dsp_configure (ins, outs)
	conv = ARDOUR.DSP.Convolution (Session, ins:n_audio (), outs:n_audio ())

	-- SPECIFY PATH TO THE IR FILE (relative to session, or absolute path)
	ir_file = Session:path() .. "/externals/B-Format-Reverb.flac"
	ir_file = '/tmp/minster1_bformat_48k.wav'

	-- load audio file into array of mono Readables
	local ar = ARDOUR.Readable.load (Session, ir_file)

	assert (ar:size() > 0)
	print (string.format ("IR file '%s' channels: %d samples/channel: %d", ir_file, ar:size (), ar:at(0):readable_length()))

	-- make sure that the file has 4 channels
	assert (ar:size() >= 4)

	-- CONFIGURE THE CONVOLVER; format is similar to jconvolver
	--
	--   (in, out, IR-data, gain, delay, length, IR-data-channel)
	--
	-- Note: IR-data-channel is always zero, specify the file's channel as ar:at(N)
	--       with N = 0 for the first channel in the file.

	conv:add_impdata (0, 0, ar:at(0), 0.100, 5496, 8712, 0, 0)
	conv:add_impdata (0, 1, ar:at(1), 0.257, 5496, 8712, 0, 0)
	conv:add_impdata (0, 2, ar:at(2), 0.324, 5496, 8712, 0, 0)
	conv:add_impdata (0, 3, ar:at(3), 0,115, 5496, 8712, 0, 0)

	conv:add_impdata (1, 0, ar:at(0), 0.100, 750, 3966, 4746, 0)
	conv:add_impdata (2, 1, ar:at(1), 0.257, 750, 3966, 4746, 0)
	conv:add_impdata (3, 2, ar:at(2), 0.324, 750, 3966, 4746, 0)
	conv:add_impdata (4, 3, ar:at(3), 0,115, 750, 3966, 4746, 0)
	-- ADD AS MANY AS NEEDED

	conv:restart ()
	collectgarbage ()
end

function dsp_latency ()
	return conv:latency()
end

function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	conv:run (bufs, in_map, out_map, n_samples, offset)
end
