ardour {
	["type"]    = "dsp",
	name        = "LTC Reader",
	category    = "Utility",
	author      = "Ardour Team",
	license     = "MIT",
	description = [[Linear Timecode (LTC) Decoder with mixer strip inline display]]
}

function dsp_ioconfig ()
	return { { audio_in = 1, audio_out = 1}, }
end

function dsp_init (rate)
	timeout = rate
	samplerate = rate
	ltc_reader = ARDOUR.DSP.LTCReader (rate / 25, ARDOUR.DSP.LTC_TV_STANDARD.LTC_TV_FILM_24)
	self:shmem():allocate(5)
end

function dsp_run (ins, outs, n_samples)
	if ins[1] ~= outs[1] then
		ARDOUR.DSP.copy_vector (outs[1]:offset (0), ins[1]:offset (0), n_samples)
	end
	ltc_reader:write (ins[1]:offset (0), n_samples, 0)
	timeout = timeout + n_samples
	local to_ui = self:shmem():to_int(0):array()
	local rv
	repeat
		local tc
		rv, tc = ltc_reader:read (0, 0, 0, 0)
		if rv >= 0 then
			timeout = 0
			self:shmem():atomic_set_int (0, 1)
			self:shmem():atomic_set_int (1, tc[1])
			self:shmem():atomic_set_int (2, tc[2])
			self:shmem():atomic_set_int (3, tc[3])
			self:shmem():atomic_set_int (4, tc[4])
			self:queue_draw ()
		end
	until rv < 0
	if timeout > samplerate then
		if 0 ~= self:shmem():atomic_get_int (0) then
			self:shmem():atomic_set_int (0, 0)
			self:queue_draw ()
		end
	end
end

-------------------------------------------------------------------------------
-- inline UI
--
local txt = nil -- a pango context
local vpadding = 2

function render_inline (ctx, displaywidth, max_h)
	if not txt then
		txt = Cairo.PangoLayout (ctx, "Mono 10px")
	end

	local d = self:shmem():to_int(0):array()
	if d[1] == 0 then
		txt:set_text("--:--:--:--")
	else
		txt:set_text(string.format("%02d:%02d:%02d:%02d", d[2], d[3], d[4], d[5]))
	end

	-- compute the size of the display
	local txtwidth, lineheight = txt:get_pixel_size()
	local displayheight = math.min(vpadding + (lineheight + vpadding), max_h)

	-- clear background
	ctx:rectangle (0, 0, displaywidth, displayheight)
	ctx:set_source_rgba (.2, .2, .2, 1.0)
	ctx:fill ()
	ctx:set_source_rgba (.8, .8, .8, 1.0)
	ctx:move_to ((displaywidth - txtwidth) / 2, 1)
	txt:show_in_cairo_context (ctx)

	return {displaywidth, displayheight}
end

