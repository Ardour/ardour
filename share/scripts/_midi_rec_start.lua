ardour {
	["type"]    = "session",
	name        = "MIDI Record Enable",
	category    = "Example", -- "Utility"
	license     = "MIT",
	author      = "Ardour Team",
	description = [[An example script to start recording on note-on.]]
}

function factory ()
	return function (n_samples)
		if Session:actively_recording() then return end -- when recording already, do nothing
		-- iterate over all MIDI ports
		_, t = Session:engine ():get_ports (ARDOUR.DataType.midi (), ARDOUR.PortList ())
		for p in t[2]:iter () do
			-- skip output ports
			if not p:receives_input () then goto next end
			local midiport = p:to_midiport ()
			-- and skip async event ports
			if midiport:isnil () then goto next end
			local mb = midiport:get_midi_buffer (n_samples) -- get the midi-data buffers
			local events = mb:table () -- copy event list into lua table
			for _,e in pairs (events) do -- iterate over all events in the midi-buffer
				if (e:buffer():array()[1] & 0xf0) == 0x90 then -- note on
					Session:maybe_enable_record (true) -- global record-enable from rt-context
					-- maybe-enable may fail if there are no tracks or step-entry is active
					-- roll transport if record-enable suceeded:
					if ARDOUR.Session.RecordState.Enabled == Session:record_status() then
						Session:request_roll (ARDOUR.TransportRequestSource.TRS_UI) -- ...and go.
					end
					return
				end
			end
			::next::
		end
	end
end
