ardour {
	["type"]    = "dsp",
	name        = "ACE High/Low Pass Filter",
	category    = "Filter",
	license     = "GPLv2",
	author      = "Ardour Community",
	description = [[High and Low Pass Filter with de-zipped controls]]
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
		{ ["type"] = "input", name = "Enable", min = 0, max = 1, default = 1, bypass = true, toggled = true },
	}
end

-- these globals are *not* shared between DSP and UI
local hp = {}  -- the biquad high-pass filter instances (DSP)
local lp = {}  -- the biquad high-pass filter instances (DSP)
local filt = nil -- the biquad filter instance (GUI, response)
local cur = {0, 0, 0, 0, 0, 0, 1} -- current parameters
local lpf = 0.03 -- parameter low-pass filter time-constant
local chn = 0 -- channel/filter count
local lpf_chunk = 0 -- chunk size for audio processing when interpolating parameters
local max_freq = 20000

local mem = nil -- memory x-fade buffer

function dsp_init (rate)
	-- allocate some mix-buffer
	mem = ARDOUR.DSP.DspShm (8192)

	-- max allowed cut-off frequency
	max_freq = .499 * rate

	-- create a table of objects to share with the GUI
	local tbl = {}
	tbl['samplerate'] = rate
	tbl['max_freq'] = max_freq
	self:table ():set (tbl)


	-- Parameter smoothing: we want to filter out parameter changes that are
	-- faster than 15Hz, and interpolate between parameter values.
	-- For performance reasons, we want to ensure that two consecutive values
	-- of the interpolated "steepness" are less that 1 apart. By choosing the
	-- interpolation chunk size to be 64 in most cases, but 32 if the rate is
	-- strictly less than 22kHz (there's only 8kHz in standard rates), we can
	-- ensure that steepness interpolation will never change the parameter by
	-- more than ~0.86.
	lpf_chunk = 64
	if rate < 22000 then lpf_chunk = 32 end
	-- We apply a discrete version of the standard RC low-pass, with a cutoff
	-- frequency of 15Hz. For more information about the underlying math, see
	-- https://en.wikipedia.org/wiki/Low-pass_filter#Discrete-time_realization
	-- (here Δt is lpf_chunk / rate)
	local R = 2 * math.pi * lpf_chunk * 15 -- Hz
	lpf = R / (R + rate)
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

		-- A different Biquad is needed for each pass and channel because they
		-- remember the last two samples seen during the last call of Biquad:run().
		-- For continuity these have to come from the previous audio chunk of the
		-- same channel and pass and would be clobbered if the same Biquad was
		-- called several times by cycle.
		for k = 1,4 do
			hp[c][k] = ARDOUR.DSP.Biquad (tbl['samplerate'])
			lp[c][k] = ARDOUR.DSP.Biquad (tbl['samplerate'])
		end
	end
end

function santize_params (ctrl)
	-- don't allow manual cross-fades. enforce enums
	ctrl[1] = math.floor(ctrl[1] + .5)
	ctrl[4] = math.floor(ctrl[4] + .5)

	-- high pass, clamp range
	ctrl[2] = math.min (max_freq, math.max (5, ctrl[2]))
	ctrl[3] = math.min (6, math.max (0.1, ctrl[3]))

	-- low pass, clamp range
	ctrl[5] = math.min (max_freq, math.max (20, ctrl[5]))
	ctrl[6] = math.min (6, math.max (0.1, ctrl[6]))

	if ctrl[7] <= 0 then -- when disabled
		ctrl[1] = 0;
		ctrl[4] = 0;
	end

	return ctrl
end

-- helper functions for parameter interpolation
function param_changed (ctrl)
	for p = 1,7 do
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
	cur[7] = ctrl[7]

	for c = 1, chn do
		for k = 1,4 do
			hp[c][k]:compute (ARDOUR.DSP.BiquadType.HighPass, cur[2], cur[3], 0)
			lp[c][k]:compute (ARDOUR.DSP.BiquadType.LowPass,  cur[5], cur[6], 0)
		end
	end
end


-- the actual DSP callback
function dsp_run (ins, outs, n_samples)
	assert (n_samples <= 8192)
	assert (#ins == chn)
	local ctrl = santize_params (CtrlPorts:array ())

	local changed = false
	local siz = n_samples
	local off = 0

	-- if a parameter was changed, process at most lpf_chunk samples
	-- at a time and interpolate parameters until the current settings
	-- match the target values
	if param_changed (ctrl) then
		changed = true
		siz = lpf_chunk
	end

	if changed == false and cur[1] == 0 and cur[4] == 0 then
		-- fully bypassed
		for c = 1, #ins do
			if ins[c] ~= outs[c] then
				ARDOUR.DSP.copy_vector (outs[c]:offset (0), ins[c]:offset (0), n_samples)
			end
		end
		-- nothing left to do
		n_samples = 0
	end

		-- process all channels
	while n_samples > 0 do
		if changed then apply_params (ctrl) end
		if siz > n_samples then siz = n_samples end

		local ho = math.floor(cur[1])
		local lo = math.floor(cur[4])

		for c = 1, #ins do

			-- High Pass
			local xfade = cur[1] - ho

			-- prepare scratch memory
			ARDOUR.DSP.copy_vector (mem:to_float (off), ins[c]:offset (off), siz)

			-- run at least |ho| biquads...
			for k = 1,ho do
				hp[c][k]:run (mem:to_float (off), siz)
			end
			ARDOUR.DSP.copy_vector (outs[c]:offset (off), mem:to_float (off), siz)

			-- mix the output of |ho| biquads (with weight |1-xfade|)
			-- with the output of |ho+1| biquads (with weight |xfade|)
			if xfade > 0 then
				ARDOUR.DSP.apply_gain_to_buffer (outs[c]:offset (off), siz, 1 - xfade)
				hp[c][ho+1]:run (mem:to_float (off), siz)
				ARDOUR.DSP.mix_buffers_with_gain (outs[c]:offset (off), mem:to_float (off), siz, xfade)
				-- also run the next biquad because it needs to have the correct state
				-- in case it start affecting the next chunck of output. Higher order
				-- ones are guaranteed not to be needed for the next run because the
				-- interpolated order won't increase more than 0.86 in one step thanks
				-- to the choice of the value of |lpf|.
				if ho + 2 <= 4 then hp[c][ho+2]:run (mem:to_float (off), siz) end
			elseif ho + 1 <= 4 then
				-- run the next biquad in case it is used next chunk
				hp[c][ho+1]:run (mem:to_float (off), siz)
			end

			-- Low Pass
			xfade = cur[4] - lo

			-- prepare scratch memory (from high pass output)
			ARDOUR.DSP.copy_vector (mem:to_float (off), outs[c]:offset (off), siz)

			-- run at least |lo| biquads...
			for k = 1,lo do
				lp[c][k]:run (mem:to_float (off), siz)
			end
			ARDOUR.DSP.copy_vector (outs[c]:offset (off), mem:to_float (off), siz)

			-- mix the output of |lo| biquads (with weight |1-xfade|)
			-- with the output of |lo+1| biquads (with weight |xfade|)
			if xfade > 0 then
				ARDOUR.DSP.apply_gain_to_buffer (outs[c]:offset (off), siz, 1 - xfade)
				lp[c][lo+1]:run (mem:to_float (off), siz)
				ARDOUR.DSP.mix_buffers_with_gain (outs[c]:offset (off), mem:to_float (off), siz, xfade)
				-- also run the next biquad in case it start affecting the next
				-- chunck of output.
				if lo + 2 <= 4 then lp[c][lo+2]:run (mem:to_float (off), siz) end
			elseif lo + 1 <= 4 then
				-- run the next biquad in case it is used next chunk
				lp[c][lo+1]:run (mem:to_float (off), siz)
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
		max_freq   = tbl['max_freq']
	end

	local ctrl = santize_params (CtrlPorts:array ())
	-- set filter coefficients if they have changed
	if param_changed (ctrl) then
		for k = 1,7 do cur[k] = ctrl[k] end
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
