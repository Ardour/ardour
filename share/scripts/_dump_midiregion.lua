ardour { ["type"] = "Snippet", name = "Dump MIDI Region" }

function factory () return function ()
	local sel = Editor:get_selection ()
	local tm = Temporal.TempoMap.read ()
	for r in sel.regions:regionlist ():iter () do
		local mr = r:to_midiregion ()
		if mr:isnil () then goto next end

		print (r:name (), "Pos:", r:position (), "Start:", r:start ())
		local nl = ARDOUR.LuaAPI.note_list (mr:model ())
		for n in nl:iter () do
			print (" Note @", n:time (), tm:sample_at_beats (n:time ()),
			ARDOUR.ParameterDescriptor.midi_note_name (n:note ()),
			"Vel:", n:velocity ())
		end
		print ("----")
		::next::
	end
end end
