ardour {
	["type"]    = "dsp",
	name        = "ACE Inline Correlation Meter",
	category    = "Visualization",
	license     = "MIT",
	author      = "Ardour Community",
	description = [[Mixer strip inline stereo correlation meter]]
}

function dsp_ioconfig ()
	return { [1] = { audio_in = 2, audio_out = 2}, }
end

function dsp_params ()
	return {}
end

function dsp_init (rate)
	stdcorr = ARDOUR.DSP.StereoCorrelation (rate, 2000, 0.3)
	samplerate = rate
	dpy_hz = rate / 25
	dpy_wr = 0
end

function dsp_configure (ins, outs)
	self:shmem ():allocate (1)
	self:shmem ():clear()
end

function dsp_run (ins, outs, n_samples)

	stdcorr:process (ins[1], ins[2], n_samples)
	self:shmem ():to_float (0):array()[1] = stdcorr:read ()

	-- emit QueueDraw every FPS
	dpy_wr = dpy_wr + n_samples
	if (dpy_wr > dpy_hz) then
		dpy_wr = dpy_wr % dpy_hz;
		self:queue_draw ()
	end

end

-------------------------------------------------------------------------------
--- inline display

function render_inline (ctx, w, max_h)
	local shmem = self:shmem () -- get shared memory region
	local corr = self:shmem ():to_float (0):array()[1]

	local h = 14

	if (h > max_h) then
		h = max_h
	end

	-- clear background
	ctx:rectangle (0, 0, w, h)
	ctx:set_source_rgba (.2, .2, .2, 1.0)
	ctx:fill ()

	-- TODO add gridlines

	-- draw signal
	if (corr > 0.5) then
		ctx:set_source_rgba (.1, .9, .1, 1.0)
	elseif corr > -0.5 then
		ctx:set_source_rgba (.7, .7, .1, 1.0)
	else
		ctx:set_source_rgba (.9, .1, .1, 1.0)
	end

	local w2 = 0.5 * w
	local x  = 0.5 * (corr * (w - 4))

	if corr > 0 then
		ctx:rectangle (w2,     1, 1 + x, 12)
	else
		ctx:rectangle (w2 + x, 1, 1 - x, 12)
	end

	ctx:fill ()

	return {w, h}
end
