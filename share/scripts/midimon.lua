ardour {
	["type"]    = "dsp",
	name        = "ACE MIDI Monitor",
	category    = "Visualization",
	license     = "GPLv2",
	author      = "Ardour Community",
	description = [[Display recent MIDI events inline in the mixer strip]]
}

local maxevents = 20
local ringsize = maxevents * 3
local evlen = 3
local hpadding, vpadding = 4, 2

function dsp_ioconfig ()
	return { { midi_in = 1, midi_out = 1, audio_in = -1, audio_out = -1}, }
end

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
			min = 0, max = 1, default = 0, toggled = true },
		{ ["type"] = "input",
			name = "Numeric Notes",
			doc = "If enabled, note-events displayed numerically",
			min = 0, max = 1, default = 0, toggled = true },
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

function dsp_configure (ins, outs)
	n_out = outs
end

function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	local pos = self:shmem():atomic_get_int(0)
	local buffer = self:shmem():to_int(1):array()
	local newdata = false

	-- passthrough all data
	ARDOUR.DSP.process_map (bufs, n_out, in_map, out_map, n_samples, offset)

	-- then fill the event buffer
	local ib = in_map:get (ARDOUR.DataType ("midi"), 0) -- index of 1st midi input

	if ib ~= ARDOUR.ChanMapping.Invalid then
		local events = bufs:get_midi (ib):table () -- copy event list into a lua table

		-- iterate over all MIDI events
		for _, e in pairs (events) do
			local ev = e:buffer():array()
			pos = pos % ringsize + 1
			-- copy the data
			for j = 1, math.min(e:size(),evlen) do
				buffer[(pos-1)*evlen + j] = ev[j]
			end
			-- zero unused slots
			for j = e:size()+1, evlen do
				buffer[(pos-1)*evlen + j] = 0
			end
			newdata = true
		end
	end

	self:shmem():atomic_set_int(0, pos)

	if newdata then
		self:queue_draw ()
	end
end

local txt = nil -- a pango context
local cursize = 0
local hex = nil
local format_note = nil
local show_scm = nil

function format_note_name(b)
	return string.format ("%5s", ARDOUR.ParameterDescriptor.midi_note_name (b))
end

function format_note_num(b)
	return string.format (hex, b)
end


function show_midi(ctx, x, y, buffer, event)
	local base = (event - 1) * evlen
	if buffer[base+1] == -1 then return false end
	local evtype = buffer[base + 1] >> 4
	local channel = (buffer[base + 1] & 15) + 1 -- for System Common Messages this has no use
	if evtype == 8 then
		txt:set_text(string.format("%02u \u{2669}Off%s" .. hex, channel, format_note(buffer[base+2]), buffer[base+3]))
	elseif evtype == 9 then
		txt:set_text(string.format("%02u \u{2669}On %s" .. hex, channel, format_note(buffer[base+2]), buffer[base+3]))
	elseif evtype == 10 then
		txt:set_text(string.format("%02u \u{2669}KP %s" .. hex, channel, format_note(buffer[base+2]), buffer[base+3]))
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
		else
			return false
		end
	else
		return false
	end
	ctx:move_to (x, y)
	txt:show_in_cairo_context (ctx)
	return true
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

	if ctrl[3] > 0 then hex = " %02X" else hex = " %3u" end
	show_scm = ctrl[4]

	if ctrl[5] > 0 then
		format_note = format_note_num
	else
		format_note = format_note_name
	end

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

	-- color of latest event
	ctx:set_source_rgba (1.0, 1.0, 1.0, 1.0)

	-- print events
	for i = pos, 1, -1 do
		if y < 0 then break end
		if show_midi(ctx, x, y, buffer, i) then
			y = y - lineheight - vpadding
			ctx:set_source_rgba (.8, .8, .8, 1.0)
		end
	end
	for i = ringsize, pos+1, -1 do
		if y < 0 then break end
		if show_midi(ctx, x, y, buffer, i) then
			y = y - lineheight - vpadding
			ctx:set_source_rgba (.8, .8, .8, 1.0)
		end
	end

	return {displaywidth, displayheight}
end
