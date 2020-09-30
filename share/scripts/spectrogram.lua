ardour {
	["type"]    = "dsp",
	name        = "ACE Inline Spectrogram",
	category    = "Visualization",
	license     = "MIT",
	author      = "Ardour Community",
	description = [[Mixer strip inline spectrum display]]
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

-- symbolic names for shmem offsets
local SHMEM_RATE = 0
local SHMEM_WRITEPTR = 1
local SHMEM_AUDIO = 2

-- a C memory area.
-- It needs to be in global scope.
-- When the variable is set to nil, the allocated memory is free()ed.
-- the memory can be interpeted as float* for use in DSP, or read/write
-- to a C++ Ringbuffer instance.
-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:DSP:DspShm
local cmem = nil

function dsp_init (rate)
	-- global variables (DSP part only)
	dpy_hz = rate / 25
	dpy_wr = 0

	-- create a shared memory area to hold the sample rate, the write_pointer,
	-- and (float) audio-data. Make it big enough to store 2s of audio which
	-- should be enough. If not, the DSP will overwrite the oldest data anyway.
	self:shmem ():allocate(2 + 2 * rate)
	self:shmem ():clear()
	self:shmem ():atomic_set_int (SHMEM_RATE, rate)
	self:shmem ():atomic_set_int (SHMEM_WRITEPTR, 0)

	-- allocate memory, local mix buffer
	cmem = ARDOUR.DSP.DspShm (8192)
end

-- "dsp_runmap" uses Ardour's internal processor API, eqivalent to
-- 'connect_and_run()". There is no overhead (mapping, translating buffers).
-- The lua implementation is responsible to map all the buffers directly.
function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	-- here we sum all audio input channels and then copy the data to a
	-- custom-made circular table for the GUIs to process later

	local audio_ins = in_map:count (): n_audio () -- number of audio input buffers
	local ccnt = 0 -- processed channel count
	local mem = cmem:to_float(0) -- a "FloatArray", float* for direct C API usage from the previously allocated buffer
	local rate = self:shmem ():atomic_get_int (SHMEM_RATE)
	local write_ptr  = self:shmem ():atomic_get_int (SHMEM_WRITEPTR)

	local ringsize = 2 * rate
	local ptr_wrap = math.floor(2^50 / ringsize) * ringsize

	for c = 1,audio_ins do
		-- see http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:ChanMapping
		-- Note: lua starts counting at 1, ardour's ChanMapping::get() at 0
		local ib = in_map:get (ARDOUR.DataType ("audio"), c - 1) -- get index of mapped input buffer
		local ob = out_map:get (ARDOUR.DataType ("audio"), c - 1) -- get index of mapped output buffer

		-- check if the input is connected to a buffer
		if (ib ~= ARDOUR.ChanMapping.Invalid) then

			-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:AudioBuffer
			-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:DSP
			if c == 1 then
				-- first channel, copy as-is
				ARDOUR.DSP.copy_vector (mem, bufs:get_audio (ib):data (offset), n_samples)
			else
				-- all other channels, add to existing data.
				ARDOUR.DSP.mix_buffers_no_gain (mem, bufs:get_audio (ib):data (offset), n_samples)
			end
			ccnt = ccnt + 1;

			-- copy data to output (if not processing in-place)
			if (ob ~= ARDOUR.ChanMapping.Invalid and ib ~= ob) then
				ARDOUR.DSP.copy_vector (bufs:get_audio (ob):data (offset), bufs:get_audio (ib):data (offset), n_samples)
			end
		end
	end

	-- Clear unconnected output buffers.
	-- In case we're processing in-place some buffers may be identical,
	-- so this must be done  *after processing*.
	for c = 1,audio_ins do
		local ib = in_map:get (ARDOUR.DataType ("audio"), c - 1)
		local ob = out_map:get (ARDOUR.DataType ("audio"), c - 1)
		if (ib == ARDOUR.ChanMapping.Invalid and ob ~= ARDOUR.ChanMapping.Invalid) then
			bufs:get_audio (ob):silence (n_samples, offset)
		end
	end

	-- Normalize gain (1 / channel-count)
	if ccnt > 1 then
		ARDOUR.DSP.apply_gain_to_buffer (mem, n_samples, 1 / ccnt)
	end

	-- if no channels were processed, feed silence.
	if ccnt == 0 then
		ARDOUR.DSP.memset (mem, 0, n_samples)
	end

	-- write data to the circular table
	if (write_ptr % ringsize + n_samples < ringsize) then
		ARDOUR.DSP.copy_vector (self:shmem ():to_float (SHMEM_AUDIO + write_ptr % ringsize), mem, n_samples)
	else
		local chunk = ringsize - write_ptr % ringsize
		ARDOUR.DSP.copy_vector (self:shmem ():to_float (SHMEM_AUDIO + write_ptr % ringsize), mem, chunk)
		ARDOUR.DSP.copy_vector (self:shmem ():to_float (SHMEM_AUDIO), cmem:to_float (chunk), n_samples - chunk)
	end
	self:shmem ():atomic_set_int (SHMEM_WRITEPTR, (write_ptr + n_samples) % ptr_wrap)

	-- emit QueueDraw every FPS
	-- TODO: call every FFT window-size worth of samples, at most every FPS
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
	local rate = self:shmem ():atomic_get_int (SHMEM_RATE)
	if not cmem then
		cmem = ARDOUR.DSP.DspShm (0)
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
		cmem:allocate (fft_size)
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
		line = 0
	end
	local ictx = img:context ()

	local bins = fft_size / 2 - 1 -- fft bin count
	local bpx = bins / w  -- bins per x-pixel (linear)
	local fpb = rate / fft_size -- freq-step per bin
	local f_e = rate / 2 / fpb -- log-scale exponent
	local f_b = w / math.log (fft_size / 2) -- inverse log-scale base
	local f_l = math.log (fft_size / rate) * f_b -- inverse logscale lower-bound

	local mem = cmem:to_float (0)

	local ringsize = 2 * rate
	local ptr_wrap = math.floor(2^50 / ringsize) * ringsize

	local write_ptr
	function read_space()
		write_ptr   = self:shmem ():atomic_get_int (SHMEM_WRITEPTR)
		local space = (write_ptr - read_ptr + ptr_wrap) % ptr_wrap
		if space > ringsize then
			-- the GUI lagged too much and unread data was overwritten
			-- jump to the oldest audio still present in the ringtable
			read_ptr = write_ptr - ringsize
			space = ringsize
		end
		return space
	end

	while (read_space() >= fft_size) do
		-- read one window from the circular table
		if (read_ptr % ringsize + fft_size < ringsize) then
			ARDOUR.DSP.copy_vector (mem, self:shmem ():to_float (SHMEM_AUDIO + read_ptr % ringsize), fft_size)
		else
			local chunk = ringsize - read_ptr % ringsize
			ARDOUR.DSP.copy_vector (mem, self:shmem ():to_float (SHMEM_AUDIO + read_ptr % ringsize), chunk)
			ARDOUR.DSP.copy_vector (cmem:to_float(chunk), self:shmem ():to_float (SHMEM_AUDIO), fft_size - chunk)
		end
		read_ptr = (read_ptr + fft_size) % ptr_wrap

		-- process one line
		fft:set_data_hann (mem, fft_size, 0)
		fft:execute ()

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
