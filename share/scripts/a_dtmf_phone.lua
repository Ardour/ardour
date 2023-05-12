ardour {
	["type"]    = "dsp",
	name        = "ACE DTMF Phone",
	category    = "Instrument",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Ardour phone home]]
}

local r = 48000
local p1 = 0
local p2 = 0
local tau = 2 * math.pi
local active_note = -1
local ringsize = 8

function dsp_ioconfig ()
	return {
		{ midi_in = 1, audio_in = 0, audio_out = 1},
		{ midi_in = 1, audio_in = 0, audio_out = 2}
	}
end

function dsp_init (rate)
	r = rate
	-- allocate DSP -> GUI ringbuffer
	self:shmem():allocate (1 + ringsize)
	self:shmem():atomic_set_int (0, 0)
	local buffer = self:shmem():to_int(1):array()
	for i = 1, ringsize do
		buffer[i] = -1 -- empty slot
	end
end

-- https://en.wikipedia.org/wiki/Dual-tone_multi-frequency_signaling#Keypad
local dtmf = {
	["1"] = {1209, 697},
	["2"] = {1336, 697},
	["3"] = {1447, 697},
	["A"] = {1633, 697},

	["4"] = {1209, 770},
	["5"] = {1336, 770},
	["6"] = {1447, 770},
	["B"] = {1633, 770},

	["7"] = {1209, 852},
	["8"] = {1336, 852},
	["9"] = {1447, 852},
	["C"] = {1633, 852},

	["*"] = {1209, 941},
	["0"] = {1336, 941},
	["#"] = {1447, 941},
	["D"] = {1633, 941},
	[" "] = {0, 0},
}

local map = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "*", "#", "A", "B", "C", "D"}

function midi_note_to_pad (n)
	-- map [1..9 0 * #] to 12TET, ignore A..D
	-- start at C == 1
	return map [1 + n % 12]
end

function dsp_run (ins, outs, n_samples)
	-- clear output buffer
	local a = {}
	for s = 1, n_samples do a[s] = 0 end

	local function synth (s_start, s_end)
		if active_note == -1 then
			return
		end

		local f = dtmf[midi_note_to_pad (active_note)]
		if f[1] == 0 then
			return
		end

		local i1 = f[1] / r
		local i2 = f[2] / r

		for s = s_start,s_end do
			a[s] = 0.5 * (math.sin (p1 * tau) + math.sin (p2 * tau))
			p1 = p1 + i1
			p2 = p2 + i2
		end
	end

	local newdata = false
	local tme = 1

	for _,b in pairs (midiin) do
		local t = b["time"] -- t = [ 1 .. n_samples ]
		-- synth sound until event
		synth (tme, t)
		tme = t + 1
		-- process MIDI events (ignore MIDI channel)
		local d = b["data"] -- get midi-event
		if (#d == 3 and (d[1] & 240) == 144) then -- note on
			--if active_note == -1 then
				active_note = d[2]
				p1, p2 = 0, 0
				-- inform UI
				local pos = self:shmem():atomic_get_int (0)
				local buffer = self:shmem():to_int(1):array()
				buffer[1 + pos] = active_note
				pos = (pos + 1) % ringsize
				self:shmem():atomic_set_int (0, pos)
				newdata = true
			--end
		end
		if (#d == 3 and (d[1] & 240) == 128) then -- note off
			if active_note == d[2] then
				active_note = -1
			end
		end
		if (#d == 3 and (d[1] & 240) == 176) then -- CC
			if (d[2] == 120 or d[2] == 123) then -- panic
				active_note = -1
				-- clear UI
				self:shmem():atomic_set_int (0, 0)
				local buffer = self:shmem():to_int(1):array()
				for i = 1, ringsize do
					buffer[i] = -1 -- empty slot
				end
				newdata = true
			end
		end
	end

	-- synth rest of cycle
	synth (tme, n_samples)

	-- keep phase between 0..tau
	p1 = math.fmod (p1, 1)
	p2 = math.fmod (p2, 1)

	-- set all outputs
	for c = 1,#outs do
		outs[c]:set_table (a, n_samples)
	end

	if newdata then
		self:queue_draw ()
	end
end

-----------------
-- inline display
-----------------

local txt = nil -- a pango context
local vpadding = 2
local displayheight = 0
local linewidth = 0

function render_inline (ctx, displaywidth, max_h)
	local pos = self:shmem():atomic_get_int(0)
	local buffer = self:shmem():to_int(1):array()
	local str = ""

	for i = 1, ringsize do
		local p = (i + pos + ringsize - 1) % ringsize
		local n = buffer[1 + p]
		if n ~= -1 and " " ~= midi_note_to_pad (n) then
			str = str .. midi_note_to_pad (n)
		end
	end

	if not txt then
		txt = Cairo.PangoLayout (ctx, "Mono 10")
		-- compute the size of the display
		local siz = ""
		for i = 1, ringsize do
			siz = siz .. "0"
		end
		txt:set_text (siz)
		local lineheight
		linewidth, lineheight = txt:get_pixel_size()
		displayheight = math.min (2 * vpadding + lineheight, max_h)
	end

	-- clear background
	ctx:rectangle (0, 0, displaywidth, displayheight)
	ctx:set_source_rgba (.2, .2, .2, 1.0)
	ctx:fill ()

	-- show dialed number
	ctx:set_source_rgba (1.0, 1.0, 1.0, 1.0)
	txt:set_text (str)
	ctx:move_to (math.floor (.5 * (displaywidth - linewidth)), vpadding)
	txt:show_in_cairo_context (ctx)

	return {displaywidth, displayheight}
end
