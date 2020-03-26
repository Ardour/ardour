ardour {
	["type"]    = "session",
	name        = "Rewrite Midi",
	license     = "MIT",
	author      = "Ardour Lua Task Force",
	description = [[An example session script preprocesses midi buffers.]]
}

function factory ()
	-- this function is called in every process cycle, before processing
	return function (n_samples)
		_, t = Session:engine ():get_ports (ARDOUR.DataType.midi (), ARDOUR.PortList ())
		for p in t[2]:iter () do
			if not p:receives_input () then goto next end

			if not p:name () == "MIDI/midi_in 1" then goto next end

			midiport = p:to_midiport ()
			assert (not midiport:isnil ())
			mb = midiport:get_midi_buffer (n_samples);

			events = mb:table() -- copy event list into lua table
			mb:silence (n_samples, 0); -- clear existing buffer

			for _,e in pairs (events) do
				-- e is-a http://manual.ardour.org/lua-scripting/class_reference/#Evoral:MidiEvent
				e:set_channel (2)
				mb:push_event (e)
			end

			::next::
		end
	end
end
