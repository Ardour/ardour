ardour {
	["type"]    = "dsp",
	name        = "Biquad Filter",
	category    = "Filter",
	license     = "MIT",
	author      = "Robin Gareus",
	email       = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[
	An Example DSP Plugin for processing audio, to
	be used with Ardour's Lua scripting facility.]]
}

function dsp_ioconfig ()
	return
	{
		{ audio_in = -1, audio_out = -1},
	}
end


function dsp_params ()
	return
	{
		{ ["type"] = "input", name = "Type", min = 0, max = 4, default = 0, enum = true, scalepoints =
			{
				["Peaking"]    = 0,
				["Low Shelf"]  = 1,
				["High Shelf"] = 2,
				["Low Pass"]   = 3,
				["High Pass"]  = 4,
			}
		},
		{ ["type"] = "input", name = "Gain", min = -20, max = 20,    default = 0,    unit="dB" },
		{ ["type"] = "input", name = "Freq", min =  20, max = 20000, default = 1000, unit="Hz", logarithmic = true },
		{ ["type"] = "input", name = "Q",    min = 0.1, max = 8,     default = .707, logarithmic = true },
	}
end

-- these globals are *not* shared between DSP and UI
local filt -- the filter instance
local cur = {0, 0, 0, 0} -- current settings

function dsp_init (rate)
	self:shmem ():allocate (1) -- shared mem to tell UI about samplerate
	local cfg = self:shmem ():to_int (0):array ()
	cfg[1] = rate
	filt = ARDOUR.DSP.Biquad (rate) -- initialize filter
end


-- apply parameters, re-compute filter coefficients if needed
function apply_params (ctrl)
	if ctrl[1] == cur[1] and ctrl[2] == cur[2] and ctrl[3] == cur[3] and ctrl[4] == cur[4] then
		return false
	end

	local ft
	if     ctrl[1] == 1 then
		ft = ARDOUR.DSP.BiQuadType.LowShelf
	elseif ctrl[1] == 2 then
		ft = ARDOUR.DSP.BiQuadType.HighShelf
	elseif ctrl[1] == 3 then
		ft = ARDOUR.DSP.BiQuadType.LowPass
	elseif ctrl[1] == 4 then
		ft = ARDOUR.DSP.BiQuadType.HighPass
	else
		ft = ARDOUR.DSP.BiQuadType.Peaking
	end

	-- TODO low-pass filter ctrl values, smooth transition
	filt:compute (ft, ctrl[3], ctrl[4], ctrl[2])
	cur[1] = ctrl[1]
	cur[2] = ctrl[2]
	cur[3] = ctrl[3]
	cur[4] = ctrl[4]
	return true
end


-- the actual DSP callback
function dsp_run (ins, outs, n_samples)
	if apply_params (CtrlPorts:array ()) then
		self:queue_draw ()
	end
	for c = 1,#ins do
		if ins[c]:sameinstance (outs[c]) then
			filt:run (ins[c], n_samples) -- in-place
		else
			ARDOUR.DSP.copy_vector (outs[c], ins[c], n_samples)
			filt:run (outs[c], n_samples)
		end
	end
end


-------------------------------------------------------------------------------
--- inline display

function round (n)
	return math.floor (n + .5)
end

function freq_at_x (x, w)
	return 20 * 1000 ^ (x / w)
end

function x_at_freq (f, w)
	return w * math.log (f / 20.0) / math.log (1000.0)
end

function db_to_y (db, h)
	if db < -20 then db = -20 end
	if db >  20 then db =  20 end
	return -.5 + 0.5 * h * (1 - db / 20)
end

function grid_db (ctx, w, h, db)
	local y = -.5 + round (db_to_y (db, h))
	ctx:move_to (0, y)
	ctx:line_to (w, y)
	ctx:stroke ()
end

function grid_freq (ctx, w, h, f)
	local x = -.5 + round (x_at_freq (f, w))
	ctx:move_to (x, 0)
	ctx:line_to (x, h)
	ctx:stroke ()
end

function render_inline (ctx, w, max_h)
	if not filt then
		-- instantiate filter (to calculate the transfer function)
		local shmem = self:shmem () -- get shared memory region
		local cfg = shmem:to_int (0):array () -- "cast" into lua-table
		filt = ARDOUR.DSP.Biquad (cfg[1])
	end

	apply_params (CtrlPorts:array ())

	-- calc height of inline display
	local h = math.ceil (w * 10 / 16) -- 16:10 aspect
	h = 2 * round (h / 2) -- even number of vertical px
	if (h > max_h) then
		h = max_h
	end

	-- clear background
	ctx:rectangle (0, 0, w, h)
	ctx:set_source_rgba (.2, .2, .2, 1.0)
	ctx:fill ()

	ctx:set_line_width (1.0)

	-- draw grid
	local dash3 = C.DoubleVector ()
	dash3:add ({1, 3})
	ctx:set_dash (dash3, 2)
	ctx:set_source_rgba (.5, .5, .5, .5)
	grid_db (ctx, w, h, 0)
	grid_db (ctx, w, h, 6)
	grid_db (ctx, w, h, 12)
	grid_db (ctx, w, h, 18)
	grid_db (ctx, w, h, -6)
	grid_db (ctx, w, h, -12)
	grid_db (ctx, w, h, -18)
	grid_freq (ctx, w, h, 100)
	grid_freq (ctx, w, h, 1000)
	grid_freq (ctx, w, h, 10000)
	ctx:unset_dash ()

	-- draw transfer function
	ctx:set_source_rgba (.8, .8, .8, 1.0)
	ctx:move_to (-.5, db_to_y (filt:dB_at_freq (freq_at_x (0, w)), h))
	for x = 1,w do
		local db = filt:dB_at_freq (freq_at_x (x, w))
		ctx:line_to (-.5 + x, db_to_y (db, h))
	end
	ctx:stroke_preserve ()

	-- fill area to zero under the curve
	ctx:line_to (w, -.5 + h * .5)
	ctx:line_to (0, -.5 + h * .5)
	ctx:close_path ()
	ctx:set_source_rgba (.5, .5, .5, .5)
	ctx:fill ()

	return {w, h}
end
