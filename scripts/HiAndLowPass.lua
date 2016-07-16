ardour {
	["type"]    = "dsp",
	name        = "a-High and Low Pass Filter",
	category    = "Filter",
	license     = "GPLv2",
	author      = "Ardour Team",
	description = [[Example Ardour Lua DSP Plugin]]
}

function dsp_ioconfig ()
	return
	{
		-- allow any number of I/O as long as port-count matches
		{ audio_in = -1, audio_out = -1},
	}
end


function dsp_params ()
	return
	{
		{ ["type"] = "input", name = "High Pass Steepness", min = 0, max = 4, default = 1, enum = true, scalepoints =
			{
				["Off"] = 0,
				["12dB/oct"] = 1,
				["24dB/oct"] = 2,
				["36dB/oct"] = 3,
				["48dB/oct"] = 4,
			}
		},
		{ ["type"] = "input", name = "High Pass Cut off frequency", min =   5, max = 20000, default = 100, unit="Hz", logarithmic = true },
		{ ["type"] = "input", name = "High Pass Resonance",         min = 0.1, max = 6,     default = .707, logarithmic = true },

		{ ["type"] = "input", name = "Low Pass Steepness", min = 0, max = 4, default = 1, enum = true, scalepoints =
			{
				["Off"] = 0,
				["12dB/oct"] = 1,
				["24dB/oct"] = 2,
				["36dB/oct"] = 3,
				["48dB/oct"] = 4,
			}
		},
		{ ["type"] = "input", name = "Low Pass Cut off frequency",  min =  20, max = 20000, default = 18000, unit="Hz", logarithmic = true },
		{ ["type"] = "input", name = "Low Pass Resonance",          min = 0.1, max = 6,     default = .707, logarithmic = true },
	}
end

-- these globals are *not* shared between DSP and UI
local hp = {}  -- the biquad high-pass filter instances (DSP)
local lp = {}  -- the biquad high-pass filter instances (DSP)
local filt = nil -- the biquad filter instance (GUI, response)
local cur = {0, 0, 0, 0, 0, 0, 0} -- current parameters
local lpf = 0.03 -- parameter low-pass filter time-constant
local chn = 0 -- channel/filter count

local mem = nil -- memory x-fade buffer

function dsp_init (rate)
	-- allocate some mix-buffer
	mem = ARDOUR.DSP.DspShm (8192)

	-- create a table of objects to share with the GUI
	local tbl = {}
	tbl['samplerate'] = rate
	self:table ():set (tbl)

	-- interpolation time constant, 64fpp
	lpf = 15000 / rate
end

function dsp_configure (ins, outs)
	assert (ins:n_audio () == outs:n_audio ())
	local tbl = self:table ():get () -- get shared memory table

	chn = ins:n_audio ()
	cur = {0, 0, 0, 0, 0, 0}

	hp = {}
	lp = {}

	collectgarbage ()

	for c = 1, chn do
		hp[c] = {}
		lp[c] = {}
		-- initialize filters
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:DSP:Biquad
		for k = 1,4 do
			hp[c][k] = ARDOUR.DSP.Biquad (tbl['samplerate'])
			lp[c][k] = ARDOUR.DSP.Biquad (tbl['samplerate'])
		end
	end
end

-- helper functions for parameter interpolation
function param_changed (ctrl)
	for p = 1,6 do
		if ctrl[p] ~= cur[p] then
			return true
		end
	end
	return false
end

function low_pass_filter_param (old, new, limit)
	if math.abs (old - new) < limit  then
		return new
	else
		return old + lpf * (new - old)
	end
end

-- apply parameters, re-compute filter coefficients if needed
function apply_params (ctrl)
	if not param_changed (ctrl) then
		return
	end

	-- low-pass filter ctrl parameter values, smooth transition
	cur[1] = low_pass_filter_param (cur[1], ctrl[1], 0.05) -- HP order x-fade
	cur[2] = low_pass_filter_param (cur[2], ctrl[2], 1.0)  -- HP freq/Hz
	cur[3] = low_pass_filter_param (cur[3], ctrl[3], 0.01) -- HP quality
	cur[4] = low_pass_filter_param (cur[4], ctrl[4], 0.05) -- LP order x-fade
	cur[5] = low_pass_filter_param (cur[5], ctrl[5], 1.0)  -- LP freq/Hz
	cur[6] = low_pass_filter_param (cur[6], ctrl[6], 0.01) -- LP quality

	for c = 1, chn do
		for k = 1,4 do
			hp[c][k]:compute (ARDOUR.DSP.BiquadType.HighPass, cur[2], cur[3], 0)
			lp[c][k]:compute (ARDOUR.DSP.BiquadType.LowPass,  cur[5], cur[6], 0)
		end
	end
end


-- the actual DSP callback
function dsp_run (ins, outs, n_samples)
	assert (n_samples < 8192)
	assert (#ins == chn)

	local changed = false
	local siz = n_samples
	local off = 0

	-- if a parameter was changed, process at most 64 samples at a time
	-- and interpolate parameters until the current settings match
	-- the target values
	if param_changed (CtrlPorts:array ()) then
		changed = true
		siz = 64
	end

	while n_samples > 0 do
		if changed then apply_params (CtrlPorts:array ()) end
		if siz > n_samples then siz = n_samples end

		local ho = math.floor(cur[1])
		local lo = math.floor(cur[4])
		local hox = cur[1]
		local lox = cur[4]

		-- process all channels
		for c = 1, #ins do

			local xfade = hox - ho
			assert (xfade >= 0 and xfade < 1)

			ARDOUR.DSP.copy_vector (mem:to_float (off), ins[c]:offset (off), siz)

			-- initialize output
			if hox == 0 then
				-- high pass is disabled, just copy data.
				ARDOUR.DSP.copy_vector (outs[c]:offset (off), mem:to_float (off), siz)
			else
				-- clear output, The filter mixes into the output buffer
				ARDOUR.DSP.memset (outs[c]:offset (off), 0, siz)
			end

			-- high pass
			-- allways run all filters so that we can interplate as needed.
			for k = 1,4 do
				if xfade > 0 and k > ho and k <= ho + 1 then
					ARDOUR.DSP.mix_buffers_with_gain (outs[c]:offset (off), mem:to_float (off), siz, 1 - xfade)
				end

				hp[c][k]:run (mem:to_float (off), siz)

				if k == ho and xfade == 0 then
					ARDOUR.DSP.copy_vector (outs[c]:offset (off), mem:to_float (off), siz)
				elseif k > ho and k <= ho + 1 then
					ARDOUR.DSP.mix_buffers_with_gain (outs[c]:offset (off), mem:to_float (off), siz, xfade)
				end
			end

			-- low pass
			xfade = lox - lo
			assert (xfade >= 0 and xfade < 1)

			-- copy output of high-pass into "processing memory"
			ARDOUR.DSP.copy_vector (mem:to_float (off), outs[c]:offset (off), siz)

			if lox > 0 then
				-- clear output, Low-pass mixes interpolated data into output,
				-- in which case we just keep the output
				ARDOUR.DSP.memset (outs[c]:offset (off), 0, siz)
			end

			for k = 1,4 do
				if xfade > 0 and k > lo and k <= lo + 1 then
					ARDOUR.DSP.mix_buffers_with_gain (outs[c]:offset (off), mem:to_float (off), siz, 1 - xfade)
				end

				lp[c][k]:run (mem:to_float (off), siz)

				if k == lo and xfade == 0 then
					ARDOUR.DSP.copy_vector (outs[c]:offset (off), mem:to_float (off), siz)
				elseif k > lo and k <= lo + 1 then
					ARDOUR.DSP.mix_buffers_with_gain (outs[c]:offset (off), mem:to_float (off), siz, xfade)
				end
			end

		end

		n_samples = n_samples - siz
		off = off + siz
	end

	if changed then
		-- notify display
		self:queue_draw ()
	end
end


-------------------------------------------------------------------------------
--- inline display

function round (n)
	return math.floor (n + .5)
end

function freq_at_x (x, w)
	-- frequency in Hz at given x-axis pixel
	return 20 * 1000 ^ (x / w)
end

function x_at_freq (f, w)
	-- x-axis pixel for given frequency, power-scale
	return w * math.log (f / 20.0) / math.log (1000.0)
end

function db_to_y (db, h)
	-- y-axis gain mapping
	if db < -60 then db = -60 end
	if db >  12 then db =  12 end
	return -.5 + round (0.2 * h) - h * db / 60
end

function grid_db (ctx, w, h, db)
	-- draw horizontal grid line
	-- note that a cairo pixel at Y spans [Y - 0.5 to Y + 0.5]
	local y = -.5 + round (db_to_y (db, h))
	ctx:move_to (0, y)
	ctx:line_to (w, y)
	ctx:stroke ()
end

function grid_freq (ctx, w, h, f)
	-- draw vertical grid line
	local x = -.5 + round (x_at_freq (f, w))
	ctx:move_to (x, 0)
	ctx:line_to (x, h)
	ctx:stroke ()
end

function response (ho, lo, f)
	-- calculate transfer function response for given
	-- hi/po pass order at given frequency [Hz]
	local db = ho * filt['hp']:dB_at_freq (f)
	return db + lo * filt['lp']:dB_at_freq (f)
end

function render_inline (ctx, w, max_h)
	if not filt then
		local tbl = self:table ():get () -- get shared memory table
		-- instantiate filter (to calculate the transfer function's response)
		filt = {}
		filt['hp'] = ARDOUR.DSP.Biquad (tbl['samplerate'])
		filt['lp'] = ARDOUR.DSP.Biquad (tbl['samplerate'])
	end

	-- set filter coefficients if they have changed
	if param_changed (CtrlPorts:array ()) then
		local ctrl = CtrlPorts:array ()
		for k = 1,6 do cur[k] = ctrl[k] end
		filt['hp']:compute (ARDOUR.DSP.BiquadType.HighPass, cur[2], cur[3], 0)
		filt['lp']:compute (ARDOUR.DSP.BiquadType.LowPass,  cur[5], cur[6], 0)
	end

	-- calc height of inline display
	local h = 1 | math.ceil (w * 9 / 16) -- use 16:9 aspect, odd number of y pixels
	if (h > max_h) then h = max_h end -- but at most max-height

	-- ctx is a http://cairographics.org/ context
	-- http://manual.ardour.org/lua-scripting/class_reference/#Cairo:Context

	-- clear background
	ctx:rectangle (0, 0, w, h)
	ctx:set_source_rgba (.2, .2, .2, 1.0)
	ctx:fill ()
	ctx:rectangle (0, 0, w, h)
	ctx:clip ()

	-- set line width: 1px
	ctx:set_line_width (1.0)

	-- draw grid
	local dash3 = C.DoubleVector ()
	local dash2 = C.DoubleVector ()
	dash2:add ({1, 2})
	dash3:add ({1, 3})
	ctx:set_dash (dash2, 2) -- dotted line: 1 pixel 2 space
	ctx:set_source_rgba (.5, .5, .5, .8)
	grid_db (ctx, w, h, 0)
	ctx:set_dash (dash3, 2) -- dashed line: 1 pixel 3 space
	ctx:set_source_rgba (.5, .5, .5, .5)
	grid_db (ctx, w, h, -12)
	grid_db (ctx, w, h, -24)
	grid_db (ctx, w, h, -36)
	grid_freq (ctx, w, h, 100)
	grid_freq (ctx, w, h, 1000)
	grid_freq (ctx, w, h, 10000)
	ctx:unset_dash ()

	-- draw transfer function line
	local ho = math.floor(cur[1])
	local lo = math.floor(cur[4])

	ctx:set_source_rgba (.8, .8, .8, 1.0)
	ctx:move_to (-.5, db_to_y (response(ho, lo, freq_at_x (0, w)), h))
	for x = 1,w do
		local db = response(ho, lo, freq_at_x (x, w))
		ctx:line_to (-.5 + x, db_to_y (db, h))
	end
	-- stoke a line, keep the path
	ctx:stroke_preserve ()

	-- fill area to zero under the curve
	ctx:line_to (w, -.5 + round (db_to_y (0, h)))
	ctx:line_to (0, -.5 + round (db_to_y (0, h)))
	ctx:close_path ()
	ctx:set_source_rgba (.5, .5, .5, .5)
	ctx:fill ()

	return {w, h}
end
