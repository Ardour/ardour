ardour {
	["type"]    = "EditorAction",
	name        = "Send Raw MIDI from File",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Read raw binary midi (.syx) from a file and send it to a MIDI port]]
}

function factory () return function ()

	function portlist ()
		local rv = {}
		local a = Session:engine()
		local _, t = a:get_ports (ARDOUR.DataType("midi"), ARDOUR.PortList())
		for p in t[2]:iter() do
			local amp = p:to_asyncmidiport ()
			if amp:isnil() or not amp:sends_output() then goto continue end
			rv[amp:name()] = amp
			print (amp:name(), amp:sends_output())
			::continue::
		end
		return rv
	end

	local dialog_options = {
		{ type = "file", key = "file", title = "Select .syx MIDI file" },
		{ type = "dropdown", key = "port", title = "Target Port", values = portlist () }
	}

	local rv = LuaDialog.Dialog ("Select Taget", dialog_options):run ()
	dialog_options = nil -- drop references (ports, shared ptr)
	collectgarbage () -- and release the references immediately

	if not rv then return end -- user cancelled

	local f = io.open (rv["file"], "rb")

	if not f then
		LuaDialog.Message ("Raw MIDI Tx", "File Not Found", LuaDialog.MessageType.Error, LuaDialog.ButtonType.Close):run ()
		goto out
	end

	local size
	do  -- scope for 'local'
		size = f:seek("end") -- determine file size
		f:seek("set", 0)

		if size > 1048576 then
			local ok = LuaDialog.Message ("Raw MIDI Tx",
				string.format ("File is larger than 1MB.\nFile-size = %.1f kB\n\nContinue?", size / 1024),
				LuaDialog.MessageType.Question, LuaDialog.ButtonType.Yes_No):run ()
			if ok ~= LuaDialog.Response.Yes then
				f:close ()
				goto out
			end
		end
	end

	do -- scope for 'local'
		local midi_byte_count = 0
		local total_read = 0
		local message_count = 0
		local long_message = false

		-- prepare progress dialog
		local pdialog = LuaDialog.ProgressWindow ("Tx MIDI", true)
		pdialog:progress (0, "Transmitting");

		local async_midi_port = rv["port"] -- reference to port
		local parser = ARDOUR.RawMidiParser () -- construct a MIDI parser

		while true do
			if pdialog:canceled () then break end
			-- read file in 64byte chunks
			local bytes = f:read (64)
			if not bytes then break end
			total_read = total_read + #bytes

			-- parse MIDI data byte-by-byte
			for i = 1, #bytes do
				if parser:process_byte (bytes:byte (i)) then
					if parser:buffer_size () > 255 then
						long_message = true
						print ("WARNING -- single large message > 255, bytes: ", parser:buffer_size ())
					end
					-- parsed complete normalized MIDI message, send it
					async_midi_port:write (parser:midi_buffer (), parser:buffer_size (), 0)

					-- Physical MIDI is sent at 31.25kBaud.
					-- Every message is sent as 10bit message on the wire,
					-- so every MIDI byte needs 320usec.
					ARDOUR.LuaAPI.usleep (400 * parser:buffer_size ())

					-- count msgs and valid bytes sent
					midi_byte_count = midi_byte_count + parser:buffer_size ()
					message_count = message_count + 1
					if 0 == message_count % 10 then
						pdialog:progress (total_read / size, string.format ("Transmitting %.1f kB", midi_byte_count / 1024))
					end
					if pdialog:canceled () then break end
				end
			end
		end

		f:close ()
		-- hide modal progress dialog and destroy it
		pdialog:done ();
		print ("Sent", message_count, "messages,  bytes: ", midi_byte_count, " read:", total_read, "/", size)

		if long_message then
			LuaDialog.Message ("Raw MIDI Tx", "Dataset contained messages longer than 127 bytes. Which may or may not have been transmitted successfully.", LuaDialog.MessageType.Warning, LuaDialog.ButtonType.Close):run ()
		end
	end

	::out::
end end

function icon (params) return function (ctx, width, height, fg)
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil(math.min (width, height) * .45) .. "px")
	txt:set_text ("S")
	ctx:move_to (1, 1)
	txt:show_in_cairo_context (ctx)

	txt:set_text ("Y")
	local tw, th = txt:get_pixel_size ()
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	txt:show_in_cairo_context (ctx)

	txt:set_text ("X")
	tw, th = txt:get_pixel_size ()
	ctx:move_to ((width - tw - 1), (height - th -1))
	txt:show_in_cairo_context (ctx)
end end
