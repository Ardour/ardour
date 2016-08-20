ardour {
	["type"]    = "dsp",
	name        = "a-Pong",
	category    = "Visualization",
	license     = "MIT",
	author      = "Ardour Lua Task Force",
	description = [[classic game of mixer pong]]
}

-- return possible i/o configurations
function dsp_ioconfig ()
	-- -1, -1 = any number of channels as long as input and output count matches
	return { [1] = { audio_in = -1, audio_out = -1}, }
end

function dsp_params ()
	return
	{
		{ ["type"] = "input", name = "Bar", min = 0, max = 1, default = 0.5 },
	}
end

local fps -- audio samples per game-step
local game_time -- counts up to fps
local ball_x, ball_y -- ball position [0..1]
local dx, dy -- current ball speed
local lost_sound -- audio-sample counter for game-over [0..3*fps]
local ping_sound -- audio-sample counter for ping-sound [0..fps]
local ping_phase -- ping note phase
local ping_pitch

function dsp_init (rate)
	self:shmem ():allocate (4)
	self:shmem ():clear ()
	fps = rate / 25
	ping_pitch = 752 / rate
	ball_x = 0.5
	ball_y = 0
	dx = 0.011
	dy = 0.0237
end

function dsp_configure (ins, outs)
	game_time  = fps -- start the ball immediately (notfiy GUI)
	ping_sound = fps -- set to end of synth cycle
	lost_sound = 3 * fps
end

function dsp_run (ins, outs, n_samples)
	local ctrl = CtrlPorts:array () -- get control port array (read/write)
	local shmem = self:shmem ()
	local state = shmem:to_float (0):array () -- "cast" into lua-table

	local changed = false -- flag to notify GUI on every game-step
	game_time = game_time + n_samples

	-- simple game engine
	while game_time > fps do
		changed = true
		game_time = game_time - fps

		-- move the ball
		ball_x = ball_x + dx
		ball_y = ball_y + dy

		-- reflect left/right
		if ball_x >= 1 or ball_x <= 0 then dx = -dx end

		-- single player (reflect top) -- TODO "stereo" version, 2 ctrls
		if ball_y <= 0 then dy = - dy end

		if ball_y > 1 then
			local bar = ctrl[1] -- get bar position
			if math.abs (bar - ball_x) < 0.1 then
				dy = - dy
				ball_y = 1.0
				dx = dx + 0.1 * (bar - ball_x)
				-- queue sound (unless it's playing)
				if (ping_sound >= fps) then
					ping_sound = 0
					ping_phase = 0
				end
			else
				-- game over
				lost_sound = 0
				ball_y = 0
				dx = 0.011
			end
		end
	end

	-- simple synth -- TODO Optimize
	if ping_sound < fps then
		local abufs = {}
		for c = 1,#outs do
			abufs[c] = outs[c]:array();
		end
		for s = 1, n_samples do
			ping_sound = ping_sound + 1
			ping_phase = ping_phase + ping_pitch
			local snd = 0.7 * math.sin(6.283185307 * ping_phase) * math.sin (3.141592 * ping_sound / fps)
			for c = 1,#outs do
				abufs[c][s] = abufs[c][s] + snd
			end
			if ping_sound >= fps then goto ping_end end
		end
		::ping_end::
	end

	if lost_sound < 3 * fps then
		local abufs = {}
		for c = 1,#outs do
			abufs[c] = outs[c]:array();
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
		state[1] = ball_x
		state[2] = ball_y
		self:queue_draw ()
	end
end


function render_inline (ctx, w, max_h)
	local ctrl = CtrlPorts:array () -- get control port array (read/write)
	local shmem = self:shmem () -- get shared memory region
	local state = shmem:to_float (0):array () -- "cast" into lua-table

	if (w > max_h) then
		h = max_h
	else
		h = w
	end

	-- clear background
	ctx:rectangle (0, 0, w, h)
	ctx:set_source_rgba (.2, .2, .2, 1.0)
	ctx:fill ()

	-- display bar
	local bar_width = w * .1
	local bar_space = w - bar_width

	ctx:set_line_cap (Cairo.LineCap.Round)
	ctx:set_source_rgba (.8, .8, .8, 1.0)
	ctx:set_line_width (3.0)
	ctx:move_to (bar_space * ctrl[1], h - 3)
	ctx:rel_line_to (bar_width, 0)
	ctx:stroke ()

	-- display ball
	ctx:move_to (state[1] * w, state[2] * (h - 5))
	ctx:close_path ()
	ctx:stroke ()

	return {w, h}
end
