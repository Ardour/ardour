ardour {
	["type"]    = "dsp",
	name        = "ACE Inline Scope",
	category    = "Visualization",
	license     = "MIT",
	author      = "Ardour Community",
	description = [[Mixer strip inline waveform display]]
}

-- return possible i/o configurations
function dsp_ioconfig ()
	-- -1, -1 = any number of channels as long as input and output count matches
	return { [1] = { audio_in = -1, audio_out = -1}, }
end

function dsp_params ()
	return
	{
		{ ["type"] = "input", name = "Timescale", min = .1, max = 5, default = 2, unit="sec", logarithmic = true },
		{ ["type"] = "input", name = "Logscale", min = 0, max = 1, default = 0, toggled = true },
		{ ["type"] = "input", name = "Height (Aspect)", min = 0, max = 3, default = 1, enum = true, scalepoints =
			{
				["Min"] = 0,
				["16:10"] = 1,
				["1:1"] = 2,
				["Max"] = 3
			}
		},
	}
end


function dsp_init (rate)
	-- global variables (DSP part only)
	samplerate = rate
	bufsiz = 6 * rate
	dpy_hz = rate / 25
	dpy_wr = 0
end

function dsp_configure (ins, outs)
	-- store configuration in global variable
	audio_ins = ins:n_audio ()
	-- allocate shared memory area
	-- this is used to speed up DSP computaton (using a C array)
	-- and to share data with the GUI
	self:shmem ():allocate (4 + bufsiz * audio_ins)
	self:shmem ():clear ()
	self:shmem ():atomic_set_int (0, 0)
	local cfg = self:shmem ():to_int (1):array ()
	cfg[1] = samplerate
	cfg[2] = bufsiz
	cfg[3] = audio_ins
end

function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	local shmem = self:shmem ()
	local write_ptr = shmem:atomic_get_int (0)

	for c = 1,audio_ins do
		-- Note: lua starts counting at 1, ardour's ChanMapping::get() at 0
		local ib = in_map:get (ARDOUR.DataType ("audio"), c - 1); -- get id of mapped input buffer for given cannel
		local ob = out_map:get (ARDOUR.DataType ("audio"), c - 1); -- get id of mapped output buffer for given cannel
		local chn_off = 4 + bufsiz * (c - 1)
		if (ib ~= ARDOUR.ChanMapping.Invalid) then
			if (write_ptr + n_samples < bufsiz) then
				ARDOUR.DSP.copy_vector (shmem:to_float (write_ptr + chn_off), bufs:get_audio (ib):data (offset), n_samples)
			else
				local w0 = bufsiz - write_ptr;
				ARDOUR.DSP.copy_vector (shmem:to_float (write_ptr + chn_off), bufs:get_audio (ib):data (offset), w0)
				ARDOUR.DSP.copy_vector (shmem:to_float (chn_off)            , bufs:get_audio (ib):data (offset + w0), n_samples - w0)
			end
			if (ob ~= ARDOUR.ChanMapping.Invalid and ib ~= ob) then
				ARDOUR.DSP.copy_vector (bufs:get_audio (ob):data (offset), bufs:get_audio (ib):data (offset), n_samples)
			end
		else
			if (write_ptr + n_samples < bufsiz) then
				ARDOUR.DSP.memset (shmem:to_float (write_ptr + chn_off), 0, n_samples)
			else
				local w0 = bufsiz - write_ptr;
				ARDOUR.DSP.memset (shmem:to_float (write_ptr + chn_off), 0, w0)
				ARDOUR.DSP.memset (shmem:to_float (chn_off)            , 0, n_samples - w0)
			end
		end
	end
	-- clear unconnected inplace buffers
	for c = 1,audio_ins do
		local ib = in_map:get (ARDOUR.DataType ("audio"), c - 1); -- get id of mapped input buffer for given cannel
		local ob = out_map:get (ARDOUR.DataType ("audio"), c - 1); -- get id of mapped output buffer for given cannel
		if (ib == ARDOUR.ChanMapping.Invalid and ob ~= ARDOUR.ChanMapping.Invalid) then
			bufs:get_audio (ob):silence (n_samples, offset)
		end
	end

	write_ptr = (write_ptr + n_samples) % bufsiz
	shmem:atomic_set_int (0, write_ptr)

	-- emit QueueDraw every FPS
	dpy_wr = dpy_wr + n_samples
	if (dpy_wr > dpy_hz) then
		dpy_wr = dpy_wr % dpy_hz;
		self:queue_draw ()
	end
end


-- helper function for drawing symmetric grid
function gridline (ctx, x, xr, h, val)
	ctx:move_to (math.floor (.5 + x + val * xr) -.5, 1)
	ctx:line_to (math.floor (.5 + x + val * xr) -.5, h - 1)
	ctx:stroke ()
	ctx:move_to (math.floor (.5 + x - val * xr) -.5, 1)
	ctx:line_to (math.floor (.5 + x - val * xr) -.5, h - 1)
	ctx:stroke ()
end

function render_inline (ctx, w, max_h)
	local ctrl = CtrlPorts:array () -- get control port array (read/write)
	local shmem = self:shmem () -- get shared memory region
	local cfg = shmem:to_int (1):array () -- "cast" into lua-table
	local rate = cfg[1]
	local buf_size = cfg[2]
	local n_chn = cfg[3]

	-- get settings
	local timescale = ctrl[1] or 1.0 -- display size in seconds
	local logscale = ctrl[2] or 0; logscale = logscale > 0 -- logscale
	local hmode = ctrl[3] or 1 -- height mode

	-- calc height
	if hmode == 0 then
		h = math.ceil (w * 10 / 16)
		if (h > 44) then
			h = 44
		end
	elseif (hmode == 2) then
		h = w
	elseif (hmode == 3) then
		h = max_h
	else
		h = math.ceil (w * 10 / 16)
	end

	if (h > max_h) then
		h = max_h
	end

	-- display settings
	local spp = math.floor (timescale * rate / (h - 2)) -- samples per pixel
	local spl = spp * (h - 1) -- total number of audio samples to read
	local read_ptr = (shmem:atomic_get_int (0) + buf_size - spl - 1) % buf_size -- read pointer
	local xr = math.ceil ((w - 2) * (0.47 / n_chn)) -- x-axis range (per channel)

	-- clear background
	ctx:rectangle (0, 0, w, h)
	ctx:set_source_rgba (.2, .2, .2, 1.0)
	ctx:fill ()

	-- prepare drawing
	ctx:set_line_width (1.0)
	local dash3 = C.DoubleVector ()
	dash3:add ({1, 3})
	local dash4 = C.DoubleVector ()
	dash4:add ({1, 4})

	-- plot every channel
	for c = 1,n_chn do
		local x = math.floor ((w - 2) * (c - .5) / n_chn) + 1.5  -- x-axis center for given channel

		-- draw grid --
		ctx:set_source_rgba (.5, .5, .5, 1.0)
		ctx:move_to (x, 1) ctx:line_to (x, h - 1) ctx:stroke ()

		ctx:set_dash (dash4, 2)
		ctx:set_source_rgba (.4, .4, .4, 1.0)
		if (logscale) then
			gridline (ctx, x, xr, h, ARDOUR.DSP.log_meter(-18))
			gridline (ctx, x, xr, h, ARDOUR.DSP.log_meter(-6))
			ctx:set_dash (dash3, 2)
			ctx:set_source_rgba (.5, .1, .1, 1.0)
			gridline (ctx, x, xr, h, ARDOUR.DSP.log_meter(-3))
		else
			gridline (ctx, x, xr, h, .1258)
			gridline (ctx, x, xr, h, .5)
			ctx:set_dash (dash3, 2)
			ctx:set_source_rgba (.5, .1, .1, 1.0)
			gridline (ctx, x, xr, h, .7079)
		end
		ctx:unset_dash ()
		ctx:set_source_rgba (.5, .1, .1, 0.7)
		gridline (ctx, x, xr, h, 1)


		-- prepare waveform display drawing
		ctx:set_source_rgba (.8, .8, .8, .7)
		ctx:save ()
		ctx:rectangle (math.floor (x - xr), 0, math.ceil (2 * xr), h)
		ctx:clip ()

		local chn_off = 4 + buf_size * (c - 1)
		local buf_off = read_ptr;

		-- iterate over every y-axis pixel
		for y = 1, h - 1 do
			local s_min = 0
			local s_max = 0
			-- calc min/max values for given range
			if (buf_off + spp < buf_size) then
				_, s_min, s_max = table.unpack (ARDOUR.DSP.peaks (shmem:to_float (chn_off + buf_off), s_min, s_max, spp))
			else
				local r0 = buf_size - buf_off;
				_, s_min, s_max = table.unpack (ARDOUR.DSP.peaks (shmem:to_float (chn_off + buf_off), s_min, s_max, r0))
				_, s_min, s_max = table.unpack (ARDOUR.DSP.peaks (shmem:to_float (chn_off)          , s_min, s_max, spp - r0))
			end
			buf_off = (buf_off + spp) % buf_size;

			if (logscale) then
				s_max = ARDOUR.DSP.log_meter_coeff (s_max)
				s_min = - ARDOUR.DSP.log_meter_coeff (-s_min)
			end

			ctx:move_to (x + s_min * xr, h - y + .5)
			ctx:line_to (x + s_max * xr, h - y + .5)
		end
		ctx:stroke ()
		ctx:restore ()
	end
	return {w, h}
end
