ardour {
	["type"]    = "session",
	name        = "Rewrite Midi",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[An example session script preprocesses midi buffers.]]
}

function factory ()
	-- this function is called in every process cycle, before processing
	return function (n_samples)
		_, t = Session:engine ():get_ports (ARDOUR.DataType.midi (), ARDOUR.PortList ())
		for p in t[2]:iter () do
			if not p:receives_input () then goto next end

			-- search-filter port
			if not p:name () == "MIDITrackName/midi_in 1" then
				goto next -- skip
			end

			-- ensure that the port is-a https://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:MidiPort
			assert (not p:to_midiport ():isnil ())

			-- https://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:MidiBuffer
			local mb = p:to_midiport ():get_midi_buffer (n_samples)
			-- When an I/O port is connected (source -> sink), the
			-- buffer is shared. The MidiBuffer is in fact the buffer
			-- from the backend. Changing events in this buffer affects
			-- all destinations that use this buffer as source.

			local events = mb:table() -- *copy* event list to a Lua table
			mb:silence (n_samples, 0) -- clear existing buffer

			-- now iterate over events that were in the buffer
			for _,e in pairs (events) do
				-- e is-a http://manual.ardour.org/lua-scripting/class_reference/#Evoral:Event
				if e:size () == 3 then
					-- found a 3 byte event
					local buffer = e:buffer():array()
					local ev_type = buffer[1] >> 4 -- get MIDI event type (upper 4 bits)
					if ev_type == 8 or ev_type == 9 then -- note on or note off event
						buffer[1] = (buffer[1] & 0xf0) + 2 -- change the MIDI channel to "2"
						buffer[3] = buffer[3] >> 1 -- reduce the velocity by half
					end
				end
				-- finally place the event back into the Port's MIDI buffers
				mb:push_event (e)
			end

			::next::
		end
	end
end
