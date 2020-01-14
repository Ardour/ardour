ardour { ["type"] = "Snippet", name = "Tempo Map Dump" }

function factory () return function ()

	local tm = Session:tempo_map ()
	local ts = tm:tempo_section_at_sample (0)

	while true do
		print ("TS @", ts:sample(), " | ", ts:to_tempo():note_types_per_minute (), "..", ts:to_tempo():end_note_types_per_minute (), "bpm")
		ts = tm:next_tempo_section (ts)
		if not ts then break end
	end

end end
