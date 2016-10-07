ardour { ["type"] = "EditorAction", name = "Vamp Audio to MIDI" }

function factory () return function ()
	local sel = Editor:get_selection ()
	local sr = Session:nominal_frame_rate ()
	local tm = Session:tempo_map ()
	local vamp = ARDOUR.LuaAPI.Vamp ("libardourvampplugins:qm-transcription", sr)

	local ar = nil
	local mr = nil
	for r in sel.regions:regionlist ():iter () do
		if r:to_midiregion():isnil() then
			ar = r
		else
			mr = r:to_midiregion()
		end
	end
	-- analyze audio from selected audio region to selected midi region
	assert (ar and mr)

	vamp:analyze (ar:to_readable (), 0, nil)
	local fl = vamp:plugin ():getRemainingFeatures ():at (0)
	if fl and fl:size() > 0 then
		local mm = mr:midi_source(0):model()
		local midi_command = mm:new_note_diff_command ("Audio2Midi")
		for f in fl:iter () do
			local ft = Vamp.RealTime.realTime2Frame (f.timestamp, sr)
			local fd = Vamp.RealTime.realTime2Frame (f.duration, sr)
			local fn = f.values:at (0)

			local bs = tm:exact_beat_at_frame (ft, 0)
			local be = tm:exact_beat_at_frame (ft + fd, 0)

			local pos = Evoral.Beats (bs)
			local len = Evoral.Beats (be - bs)

			local note = ARDOUR.LuaAPI.new_noteptr (1, pos, len, fn + 1, 0x7f)
			midi_command:add (note)

		end
		mm:apply_command (Session, midi_command)
	end
end end
