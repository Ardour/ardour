ardour {
	["type"]    = "dsp",
	name        = "Pong",
	category    = "Toy",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[A console classic for your console]]
}

-- return possible i/o configurations
function dsp_ioconfig ()
	-- -1, -1 = any number of channels as long as input and output count matches
	return { [1] = { audio_in = -1, audio_out = -1}, }
end

-- control port(s)
function dsp_params ()
	return
	{
		{ ["type"] = "input", name = "Bar", min = 0, max = 1, default = 0.5 },
		{ ["type"] = "input", name = "Reset", min = 0, max = 1, default = 0, toggled = true },
		{ ["type"] = "input", name = "Difficulty", min = 1, max = 10, default = 3},
	}
end


-- Game State (for this instance)
-- NOTE: these variables are for the DSP part (not shared with the GUI instance)
local sample_rate -- sample-rate
local fps -- audio samples per game-step
local game_time -- counts up to fps
local game_score
local ball_x, ball_y -- ball position [0..1]
local dx, dy -- current ball speed
local lost_sound -- audio-sample counter for game-over [0..3*fps]
local ping_sound -- audio-sample counter for ping-sound [0..fps]
local ping_phase -- ping note phase-difference per sample
local ping_pitch

function dsp_init (rate)
	-- allocate a "shared memory" area to transfer state to the GUI
	self:shmem ():allocate (3)
	self:shmem ():clear ()
	-- initialize some variables
	sample_rate = rate
	fps = rate / 25
	ping_pitch = 752 / rate
	ball_x = 0.5
	ball_y = 0
	dx = 0.00367
	dy = 0.01063
	game_score = 0
	game_time  = fps -- start the ball immediately (notify GUI)
	ping_sound = fps -- set to end of synth cycle
	lost_sound = 3 * fps
end

function queue_beep ()
	-- queue 'ping' sound (unless one is already playing to prevent clicks)
	if (ping_sound >= fps) then
		-- major scale, 2 octaves
		local scale = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23, 24 }
		local midi_note = 60 + scale[1 + math.floor (math.random () * 14)]
		ping_pitch = (440 / 32) * 2^((midi_note - 10.0) / 12.0) / sample_rate
		ping_sound = 0
		ping_phase = 0
	end
end

-- callback: process "n_samples" of audio
-- ins, outs are http://manual.ardour.org/lua-scripting/class_reference/#C:FloatArray
-- pointers to the audio buffers
function dsp_run (ins, outs, n_samples)
	local ctrl = CtrlPorts:array () -- get control port array (read/write)

	local changed = false -- flag to notify GUI on every game-step
	game_time = game_time + n_samples

	-- reset (allow to write automation from a given start-point)
	-- ctrl[2] corresponds to the  "Reset" input control
	if ctrl[2] > 0 then
		game_time = 0
		ball_x = 0.5
		ball_y = 0
		dx = 0.00367
		dy = 0.01063
		game_score = 0
	end

	-- simple game engine
	while game_time > fps and ctrl[2] <= 0 do
		changed = true
		game_time = game_time - fps

		-- move the ball
		ball_x = ball_x + dx * ctrl[3]
		ball_y = ball_y + dy * ctrl[3]

		-- reflect left/right
		if ball_x >= 1 or ball_x <= 0 then
			dx = -dx
			queue_beep ()
		end

		-- single player (reflect top) -- TODO "stereo" version, 2 ctrls :)
		if ball_y <= 0 then
			dy = - dy y = 0
			queue_beep ()
		end

		-- keep the ball in the field at all times
		if ball_x >= 1 then ball_x = 1 end
		if ball_x <= 0 then ball_x = 0 end

		-- bottom edge
		if ball_y > 1 then
			local bar = ctrl[1] -- get bar position
			if math.abs (bar - ball_x) < 0.1 then
				-- reflect the ball
				dy = - dy
				ball_y = 1.0
				dx = dx - 0.04 * (bar - ball_x)
				-- make sure it's moving (not stuck on borders)
				if math.abs (dx) < 0.0001 then dx = 0.0001 end
				game_score = game_score + 1
				queue_beep ()
			else
				-- game over, reset game
				lost_sound = 0 -- re-start noise
				ball_y = 0
				game_score = 0
				dx = 0.00367
			end
		end
	end

	-- forward audio if processing is not in-place
	for c = 1,#outs do
		-- check if output and input buffers for this channel are identical
		-- http://manual.ardour.org/lua-scripting/class_reference/#C:FloatArray
		if ins[c] ~= outs[c] then
			-- fast (accelerated) copy
			-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:DSP
			ARDOUR.DSP.copy_vector (outs[c], ins[c], n_samples)
		end
	end

	-- simple synth -- TODO Optimize
	if ping_sound < fps then
		-- cache audio data buffers for direct access, later
		local abufs = {}
		for c = 1,#outs do
			abufs[c] = outs[c]:array()
		end
		-- simple sine synth with a sine-envelope
		for s = 1, n_samples do
			ping_sound = ping_sound + 1
			ping_phase = ping_phase + ping_pitch
			local snd = 0.7 * math.sin(6.283185307 * ping_phase) * math.sin (3.141592 * ping_sound / fps)
			-- add synthesized sound to all channels
			for c = 1,#outs do
				abufs[c][s] = abufs[c][s] + snd
			end
			-- break out of the loop when the sound finished
			if ping_sound >= fps then goto ping_end end
		end
		::ping_end::
	end

	if lost_sound < 3 * fps then
		local abufs = {}
		for c = 1,#outs do
			abufs[c] = outs[c]:array()
		end
		for s = 1, n_samples do
			lost_sound = lost_sound + 1
			-- -12dBFS white noise
			local snd = 0.5 * (math.random () - 0.5)
			for c = 1,#outs do
				abufs[c][s] = abufs[c][s] + snd
			end
			if lost_sound >= 3 * fps then goto noise_end end
		end
		::noise_end::
	end

	if changed then
		-- notify the GUI
		local shmem = self:shmem () -- get the shared memory region
		local state = shmem:to_float (0):array () -- "cast" into lua-table
		-- update data..
		state[1] = ball_x
		state[2] = ball_y
		state[3] = game_score
		-- ..and wake up the UI
		self:queue_draw ()
	end
end


-------------------------------------------------------------------------------
--- inline display

local txt = nil -- cache font description (in GUI context)

function render_inline (ctx, w, max_h)
	local ctrl = CtrlPorts:array () -- control port array
	local shmem = self:shmem () -- shared memory region (game state from DSP)
	local state = shmem:to_float (0):array () -- cast to lua-table

	if (w > max_h) then
		h = max_h
	else
		h = w
	end

	-- prepare text rendering
	if not txt then
		-- allocate PangoLayout and set font
		--http://manual.ardour.org/lua-scripting/class_reference/#Cairo:PangoLayout
		txt = Cairo.PangoLayout (ctx, "Mono 10px")
	end

	-- ctx is-a http://manual.ardour.org/lua-scripting/class_reference/#Cairo:Context
	-- 2D vector graphics http://cairographics.org/

	-- clear background
	ctx:rectangle (0, 0, w, h)
	ctx:set_source_rgba (.2, .2, .2, 1.0)
	ctx:fill ()

	-- print the current score
	if (state[3] > 0) then
		txt:set_text (string.format ("%.0f", state[3]));
		local tw, th = txt:get_pixel_size ()
		ctx:set_source_rgba (1, 1, 1, 1.0)
		ctx:move_to (w - tw - 3, 3)
		txt:show_in_cairo_context (ctx)
	end

	-- prepare line and dot rendering
	ctx:set_line_cap (Cairo.LineCap.Round)
	ctx:set_line_width (3.0)
	ctx:set_source_rgba (.8, .8, .8, 1.0)

	-- display bar
	local bar_width = w * .1
	local bar_space = w - bar_width

	ctx:move_to (bar_space * ctrl[1], h - 3)
	ctx:rel_line_to (bar_width, 0)
	ctx:stroke ()

	-- display ball
	ctx:move_to (1 + state[1] * (w - 3), state[2] * (h - 5))
	ctx:close_path ()
	ctx:stroke ()

	return {w, h}
end
