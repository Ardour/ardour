ardour {
	["type"]    = "dsp",
	name        = "Inline Spectrogram",
	category    = "Visualization",
	license     = "GPLv2",
	author      = "Robin Gareus",
	email       = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[An Example DSP Plugin to display a spectrom on the mixer strip]]
}

-- return possible i/o configurations
function dsp_ioconfig ()
	-- -1, -1 = any number of channels as long as input and output count matches
	return { [1] = { audio_in = -1, audio_out = -1}, }
end

function dsp_params ()
	return
	{
		{ ["type"] = "input", name = "Logscale", min = 0, max = 1, default = 0, toggled = true },
		{ ["type"] = "input", name = "1/f scale", min = 0, max = 1, default = 1, toggled = true },
		{ ["type"] = "input", name = "FFT Size", min = 0, max = 4, default = 3, enum = true, scalepoints =
			{
				["512"]  = 0,
				["1024"] = 1,
				["2048"] = 2,
				["4096"] = 3,
				["8192"] = 4,
			}
		},
		{ ["type"] = "input", name = "Height (Aspect)", min = 0, max = 3, default = 1, enum = true, scalepoints =
			{
				["Min"] = 0,
				["16:10"] = 1,
				["1:1"] = 2,
				["Max"] = 3
			}
		},
		{ ["type"] = "input", name = "Range", min = 20, max = 160, default = 60, unit="dB"},
		{ ["type"] = "input", name = "Offset", min = -40, max = 40, default = 0, unit="dB"},
	}
end

function dsp_init (rate)
	-- global variables (DSP part only)
	samplerate = rate
	bufsiz = 2 * rate
	dpy_hz = rate / 25
	dpy_wr = 0
end

function dsp_configure (ins, outs)
	-- store configuration in global variable
	audio_ins = ins:n_audio ()
	-- allocate shared memory area, ringbuffer between DSP/GUI
	self:shmem ():allocate (4 + bufsiz)
	self:shmem ():clear ()
	self:shmem ():atomic_set_int (0, 0)
	local cfg = self:shmem ():to_int (1):array ()
	cfg[1] = samplerate
	cfg[2] = bufsiz
end

function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	local shmem = self:shmem ()
	local write_ptr = shmem:atomic_get_int (0)

	-- sum channels, copy to ringbuffer
	for c = 1,audio_ins do
		-- Note: lua starts counting at 1, ardour's ChanMapping::get() at 0
		local ib = in_map:get (ARDOUR.DataType ("audio"), c - 1) -- get id of mapped input buffer for given cannel
		local ob = out_map:get (ARDOUR.DataType ("audio"), c - 1) -- get id of mapped output buffer for given cannel
		if (ib ~= ARDOUR.ChanMapping.Invalid) then
			-- check ringbuffer wrap-around
			if (write_ptr + n_samples < bufsiz) then
				if c == 1 then
					ARDOUR.DSP.copy_vector (shmem:to_float (4 + write_ptr), bufs:get_audio (ib):data (offset), n_samples)
				else
					ARDOUR.DSP.mix_buffers_no_gain (shmem:to_float (4 + write_ptr), bufs:get_audio (ib):data (offset), n_samples)
				end
			else
				local w0 = bufsiz - write_ptr
				if c == 1 then
					ARDOUR.DSP.copy_vector (shmem:to_float (4 + write_ptr), bufs:get_audio (ib):data (offset), w0)
					ARDOUR.DSP.copy_vector (shmem:to_float (4)            , bufs:get_audio (ib):data (offset + w0), n_samples - w0)
				else
					ARDOUR.DSP.mix_buffers_no_gain (shmem:to_float (4 + write_ptr), bufs:get_audio (ib):data (offset), w0)
					ARDOUR.DSP.mix_buffers_no_gain (shmem:to_float (4)            , bufs:get_audio (ib):data (offset + w0), n_samples - w0)
				end
			end
			-- copy data to output (if not processing in-place)
			if (ob ~= ARDOUR.ChanMapping.Invalid and ib ~= ob) then
				ARDOUR.DSP.copy_vector (bufs:get_audio (ob):data (offset), bufs:get_audio (ib):data (offset), n_samples)
			end
		else
			-- invalid (unconnnected) input
			if (write_ptr + n_samples < bufsiz) then
				ARDOUR.DSP.memset (shmem:to_float (4 + write_ptr), 0, n_samples)
			else
				local w0 = bufsiz - write_ptr
				ARDOUR.DSP.memset (shmem:to_float (4 + write_ptr), 0, w0)
				ARDOUR.DSP.memset (shmem:to_float (4)            , 0, n_samples - w0)
			end
		end
	end

	-- normalize  1 / channel-count
	if audio_ins > 1 then
		if (write_ptr + n_samples < bufsiz) then
			ARDOUR.DSP.apply_gain_to_buffer (shmem:to_float (4 + write_ptr), n_samples, 1 / audio_ins)
		else
			local w0 = bufsiz - write_ptr
			ARDOUR.DSP.apply_gain_to_buffer (shmem:to_float (4 + write_ptr), w0, 1 / audio_ins)
			ARDOUR.DSP.apply_gain_to_buffer (shmem:to_float (4)            , n_samples - w0, 1 / audio_ins)
		end
	end

	-- clear unconnected inplace buffers
	for c = 1,audio_ins do
		local ib = in_map:get (ARDOUR.DataType ("audio"), c - 1) -- get id of mapped input buffer for given cannel
		local ob = out_map:get (ARDOUR.DataType ("audio"), c - 1) -- get id of mapped output buffer for given cannel
		if (ib == ARDOUR.ChanMapping.Invalid and ob ~= ARDOUR.ChanMapping.Invalid) then
			bufs:get_audio (ob):silence (n_samples, offset)
		end
	end

	write_ptr = (write_ptr + n_samples) % bufsiz
	shmem:atomic_set_int (0, write_ptr)

	-- emit QueueDraw every FPS
	-- TODO: call every window-size worth of samples, at most every FPS
	dpy_wr = dpy_wr + n_samples
	if (dpy_wr > dpy_hz) then
		dpy_wr = dpy_wr % dpy_hz
		self:queue_draw ()
	end
end

----------------------------------------------------------------
-- GUI

local fft = nil
local read_ptr = 0
local line = 0
local img = nil
local fft_size = 0
local last_log = false

function render_inline (ctx, w, max_h)
	local ctrl = CtrlPorts:array () -- get control port array (read/write)
	local shmem = self:shmem () -- get shared memory region
	local cfg = shmem:to_int (1):array () -- "cast" into lua-table
	local rate = cfg[1]
	local buf_size = cfg[2]

	if buf_size == 0 then
		return
	end

	-- get settings
	local logscale = ctrl[1] or 0; logscale = logscale > 0 -- x-axis logscale
	local pink = ctrl[2] or 0; pink = pink > 0 -- 1/f scale
	local fftsizeenum = ctrl[3] or 3 -- fft-size enum
	local hmode = ctrl[4] or 1 -- height mode enum
	local dbrange = ctrl[5] or 60
	local gaindb = ctrl[6] or 0

	local fftsize
	if fftsizeenum == 0 then fftsize = 512
	elseif fftsizeenum == 1 then fftsize = 1024
	elseif fftsizeenum == 2 then fftsize = 2048
	elseif fftsizeenum == 4 then fftsize = 8192
	else fftsize = 4096
	end

	if fftsize ~= fft_size then
		fft_size = fftsize
		fft = nil
	end

	if dbrange < 20 then dbrange = 20; end
	if dbrange > 160 then dbrange = 160; end
	if gaindb < -40 then dbrange = -40; end
	if gaindb >  40 then dbrange =  40; end


	if not fft then
		fft = ARDOUR.DSP.FFTSpectrum (fft_size, rate)
	end

	if last_log ~= logscale then
		last_log = logscale
		img = nil
		line = 0
	end

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

	-- re-create image surface
	if not img or img:get_width() ~= w or img:get_height () ~= h then
		img = Cairo.ImageSurface (Cairo.Format.ARGB32, w, h)
	end
	local ictx = img:context ()

	local bins = fft_size / 2 - 1 -- fft bin count
	local bpx = bins / w  -- bins per x-pixel (linear)
	local fpb = rate / fft_size -- freq-step per bin
	local f_e = rate / 2 / fpb -- log-scale exponent
	local f_b = w / math.log (fft_size / 2) -- inverse log-scale base
	local f_l = math.log (fft_size / rate) * f_b -- inverse logscale lower-bound

	-- available samples in ring-buffer
	local write_ptr = shmem:atomic_get_int (0)
	local avail = (write_ptr + buf_size - read_ptr) % buf_size

	while (avail >= fft_size) do
		-- process one line / buffer
		if read_ptr + fft_size < buf_size then
			fft:set_data_hann (shmem:to_float (read_ptr + 4), fft_size, 0)
		else
			local r0 = buf_size - read_ptr
			fft:set_data_hann (shmem:to_float (read_ptr + 4), r0, 0)
			fft:set_data_hann (shmem:to_float (4), fft_size - r0, r0)
		end

		fft:execute ()

		read_ptr = (read_ptr + fft_size) % buf_size
		avail = (write_ptr + buf_size - read_ptr ) % buf_size

		-- draw spectrum
		assert (bpx >= 1)

		-- scroll
		if line == 0 then line = h - 1; else line = line - 1; end

		-- clear this line
		ictx:set_source_rgba (0, 0, 0, 1)
		ictx:rectangle (0, line, w, 1)
		ictx:fill ()

		for x = 0, w - 1 do
			local pk = 0
			local b0, b1
			if logscale then
				-- 20 .. 20k
				b0 = math.floor (f_e ^ (x / w))
				b1 = math.floor (f_e ^ ((x + 1) / w))
			else
				b0 = math.floor (x * bpx)
				b1 = math.floor ((x + 1) * bpx)
			end

			if b1 >= b0 and b1 <= bins and b0 >= 0 then
				for i = b0, b1 do
					local level = gaindb + fft:power_at_bin (i, pink and i or 1) -- pink ? i : 1
					if level > -dbrange then
						local p = (dbrange + level) / dbrange
						if p > pk then pk = p; end
					end
				end
			end
			if pk > 0.0 then
				if pk > 1.0 then pk = 1.0; end
				ictx:set_source_rgba (ARDOUR.LuaAPI.hsla_to_rgba (.70 - .72 * pk, .9, .3 + pk * .4));
				ictx:rectangle (x, line, 1, 1)
				ictx:fill ()
			end
		end
	end

	-- copy image surface
	if line == 0 then
		img:set_as_source (ctx, 0, 0)
		ctx:rectangle (0, 0, w, h)
		ctx:fill ()
	else
		local yp = h - line - 1;
		img:set_as_source (ctx, 0, yp)
		ctx:rectangle (0, yp, w, line)
		ctx:fill ()

		img:set_as_source (ctx, 0, -line)
		ctx:rectangle (0, 0, w, yp)
		ctx:fill ()
	end


	-- draw grid on top
	function x_at_freq (f)
		if logscale then
			return f_l + f_b * math.log (f)
		else
			return 2 * w * f / rate;
		end
	end

	function grid_freq (f)
		-- draw vertical grid line
		local x = .5 + math.floor (x_at_freq (f))
		ctx:move_to (x, 0)
		ctx:line_to (x, h)
		ctx:stroke ()
	end

	-- draw grid on top
	local dash3 = C.DoubleVector ()
	dash3:add ({1, 3})
	ctx:set_line_width (1.0)
	ctx:set_dash (dash3, 2) -- dotted line
	ctx:set_source_rgba (.5, .5, .5, .8)
	grid_freq (100)
	grid_freq (1000)
	grid_freq (10000)
	ctx:unset_dash ()

	return {w, h}
end
