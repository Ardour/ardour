ardour { ["type"] = "dsp", name = "Lua FIR Convolver", license = "MIT", author = "Ardour Team", description = [[Another simple DSP example]] }

function dsp_ioconfig () return
	{
		{ audio_in = 1, audio_out = 1},
	}
end

local conv

function dsp_configure (ins, outs)
	conv = ARDOUR.DSP.Convolution (Session, ins:n_audio (), outs:n_audio ())

	local cmem = ARDOUR.DSP.DspShm (4)
	cmem:clear ()
	local d = cmem:to_float (0):array()
	d[1] = .5
	d[2] = .5
	local ar = ARDOUR.AudioRom.new_rom (cmem:to_float (0), 4)
	conv:add_impdata (0, 0, ar, 1.0, 0, 0, 0, 0)

	cmem:to_float (0):set_table({1, -1, 0, 0}, 4)
	ar = ARDOUR.AudioRom.new_rom (cmem:to_float (0), 3)
	conv:add_impdata (0, 0, ar, 1.0, 0, 0, 0, 0)

	conv:restart ()
	collectgarbage ()
end

function dsp_latency ()
	return conv:latency()
end

function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	conv:run (bufs, in_map, out_map, n_samples, offset)
end
