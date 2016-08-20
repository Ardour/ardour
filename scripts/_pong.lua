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

local gametime
local fps
local ball_x, ball_y
local dx, dy
local pingsound
local lotsound
local pingnote

function dsp_init (rate)
	self:shmem ():allocate (4)
	self:shmem ():clear ()
	fps = rate / 25
	pingnote = 352 / rate
	ball_x = 0.5
	ball_y = 0
	dx = 0.011
	dy = 0.021
end

function dsp_configure (ins, outs)
	gametime = fps
	pingsound = fps
	lostsound = 3 * fps
end

function dsp_run (ins, outs, n_samples)
	local ctrl = CtrlPorts:array () -- get control port array (read/write)
	local shmem = self:shmem ()
	local state = shmem:to_float (0):array () -- "cast" into lua-table

	local changed = false
	gametime = gametime + n_samples

	-- simple game engine
	while gametime > fps do
		changed = true
		gametime = gametime - fps

		ball_x = ball_x + dx
		ball_y = ball_y + dy

		if ball_x >= 1 or ball_x <= 0 then dx = -dx end
		if ball_y <= 0 then dy = - dy end

		if ball_y > 1 then
			local bar = ctrl[1]
			if math.abs (bar - ball_x) < 0.1 then
				dy = - dy
				ball_y = 1.0
				dx = dx + 0.1 * (bar - ball_x)
				-- queue sound (unless it's playing)
				if (pingsound > fps) then
					pingsound = 0
				end
				phase = 0
			else
				-- game over
				lostsound = 0
				ball_y = 0
				dx = 0.011
			end
		end
	end

	-- simple synth -- TODO Optimize
	if pingsound <= fps then
		for s = 1, n_samples do
			pingsound = pingsound + 1
			if pingsound > fps then goto note_end end
			phase = phase + pingnote
			local snd = 0.7 * math.sin(6.283185307 * phase) * math.sin (3.141592 * pingsound / fps)
			for c = 1,#outs do
				-- don't copy this code, it's quick/dirty and not efficient
				outs[c]:array()[s] = outs[c]:array()[s] + snd
			end
			::note_end::
		end
	end

	if lostsound <= 3 * fps then
		for s = 1, n_samples do
			lostsound = lostsound + 1
			if lostsound > 3 * fps then goto noise_end end
			local snd = 0.5 * (math.random () - 0.5)
			for c = 1,#outs do
				-- don't copy this code, it's quick/dirty and not efficient
				outs[c]:array()[s] = outs[c]:array()[s] + snd
			end
			::noise_end::
		end
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
