ardour {
	["type"]    = "dsp",
	name        = "MIDI Monitor",
	category    = "Visualization",
	license     = "GPLv2",
	author      = "Ardour Team",
	description = [[Display recent MIDI events inline in the mixer strip]]
}

local maxevents = 20
local ringsize = maxevents * 3
local evlen = 3
local hpadding, vpadding = 4, 2

function dsp_ioconfig ()
	return { { audio_in = 0, audio_out = 0}, }
end

function dsp_has_midi_input () return true end
function dsp_has_midi_output () return true end

function dsp_params ()
	return
	{
		{ ["type"] = "input",
			name = "Font size",
			doc = "Text size used by the monitor to display midi events",
			min = 4, max = 12, default = 7, integer = true },
		{ ["type"] = "input",
			name = "Line count",
			doc = "How many events will be shown at most",
			min = 1, max = maxevents, default = 6, integer = true },
		{ ["type"] = "input",
			name = "Hexadecimal",
			doc = "If enabled, values will be printed in hexadecimal notation",
			min = 0, max = 1, default = 0, toggled = true },
		{ ["type"] = "input",
			name = "System messages",
			doc = "If enabled, the monitor will show System Control and Real-Time messages",
			min = 0, max = 1, default = 0, toggled = true }
	}
end

function dsp_init (rate)
	-- create a shmem space to hold latest midi events
	-- a int representing the index of the last event, and
	-- a C-table as storage for events.
	self:shmem():allocate(1 + ringsize*evlen)
	self:shmem():atomic_set_int(0, 1)
	local buffer = self:shmem():to_int(1):array()
	for i = 1, ringsize*evlen do
		buffer[i] = -1 -- sentinel for empty slot
	end
end

function dsp_run (_, _, n_samples)
	assert (type(midiin) == "table")
	assert (type(midiout) == "table")

	local pos = self:shmem():atomic_get_int(0)
	local buffer = self:shmem():to_int(1):array()

	-- passthrough midi data, and fill the event buffer
	for i, d in pairs(midiin) do
		local ev = d["data"]
		midiout[i] = { time = d["time"], data = ev }
		pos = pos % ringsize + 1
		for j = 1, math.min(#ev,evlen) do
			buffer[(pos-1)*evlen + j] = ev[j]
		end
		for j = #ev+1, evlen do
			buffer[(pos-1)*evlen + j] = 0
		end
	end

	self:shmem():atomic_set_int(0, pos)

	self:queue_draw ()
end

local txt = nil -- a pango context
local cursize = 0
local hex = nil
local show_scm = nil

function show_midi(ctx, x, y, buffer, event)
	local base = (event - 1) * evlen
	if buffer[base+1] == -1 then return end
	local evtype = buffer[base + 1] >> 4
	local channel = (buffer[base + 1] & 15) + 1 -- for System Common Messages this has no use
	if evtype == 8 then
		txt:set_text(string.format("%02u \u{2669}Off" .. hex .. hex, channel, buffer[base+2], buffer[base+3]))
	elseif evtype == 9 then
		txt:set_text(string.format("%02u \u{2669}On " .. hex .. hex, channel, buffer[base+2], buffer[base+3]))
	elseif evtype == 10 then
		txt:set_text(string.format("%02u \u{2669}KP " .. hex .. hex, channel, buffer[base+2], buffer[base+3]))
	elseif evtype == 11 then
		txt:set_text(string.format("%02u CC  " .. hex .. hex, channel, buffer[base+2], buffer[base+3]))
	elseif evtype == 12 then
		txt:set_text(string.format("%02u PRG " .. hex, channel, buffer[base+2]))
	elseif evtype == 13 then
		txt:set_text(string.format("%02u  KP " .. hex, channel, buffer[base+2]))
	elseif evtype == 14 then
		txt:set_text(string.format("%02u PBnd" .. hex, channel, buffer[base+2] | buffer[base+3] << 7))
	elseif show_scm > 0 then -- System Common Message
		local message = buffer[base + 1] & 15
		if message == 0 then
			txt:set_text("-- SysEx")
		elseif message == 1 then
			txt:set_text(string.format("-- Time Code" .. hex, buffer[base+2]))
		elseif message == 2 then
			txt:set_text(string.format("-- Song Pos" .. hex, buffer[base+2] | buffer[base+3] << 7))
		elseif message == 3 then
			txt:set_text(string.format("-- Select Song" .. hex, buffer[base+2]))
		elseif message == 6 then
			txt:set_text("-- Tune Rq")
		elseif message == 8 then
			txt:set_text("-- Timing")
		elseif message == 10 then
			txt:set_text("-- Start")
		elseif message == 11 then
			txt:set_text("-- Continue")
		elseif message == 12 then
			txt:set_text("-- Stop")
		elseif message == 14 then
			txt:set_text("-- Active")
		elseif message == 15 then
			txt:set_text("-- Reset")
		end
	end
	ctx:move_to (x, y)
	txt:show_in_cairo_context (ctx)
end

function render_inline (ctx, displaywidth, max_h)
	local ctrl = CtrlPorts:array ()
	local pos = self:shmem():atomic_get_int(0)
	local buffer = self:shmem():to_int(1):array()
	local count = ctrl[2]

	if not txt or cursize ~= ctrl[1] then
		cursize = math.floor(ctrl[1])
		txt = Cairo.PangoLayout (ctx, "Mono " .. cursize)
	end

	if ctrl[3] > 0 then hex = " %2X" else hex = " %3u" end
	show_scm = ctrl[4]

	-- compute the size of the display
	txt:set_text("0")
	local _, lineheight = txt:get_pixel_size()
	local displayheight = math.min(vpadding + (lineheight + vpadding) * count, max_h)

	-- compute starting position (pango anchors text at north-west corner)
	local x, y = hpadding, displayheight - lineheight - vpadding

	-- clear background
	ctx:rectangle (0, 0, displaywidth, displayheight)
	ctx:set_source_rgba (.2, .2, .2, 1.0)
	ctx:fill ()

	-- print latest event
	ctx:set_source_rgba (1.0, 1.0, 1.0, 1.0)
	show_midi(ctx, x, y, buffer, pos)
	y = y - lineheight - vpadding

	-- and remaining events
	ctx:set_source_rgba (.8, .8, .8, 1.0)
	for i = pos-1, 1, -1 do
		if y < 0 then break end
		show_midi(ctx, x, y, buffer, i)
		y = y - lineheight - vpadding
	end
	for i = ringsize, pos+1, -1 do
		if y < 0 then break end
		show_midi(ctx, x, y, buffer, i)
		y = y - lineheight - vpadding
	end

	return {displaywidth, displayheight}
end
