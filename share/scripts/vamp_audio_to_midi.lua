ardour {
	["type"] = "EditorAction",
	name = "Polyphonic Audio to MIDI",
	license     = "MIT",
	author      = "Ardour Team",
description = [[
Analyze audio from the selected audio region to a selected MIDI region.

A MIDI region on the target track will have to be created first (use the pen tool).

This script uses the Polyphonic Transcription VAMP plugin from Queen Mary Univ, London.
The plugin works best at 44.1KHz input sample rate, and is tuned for piano and guitar music. Velocity is not estimated.
]]
}

function factory () return function ()
	local sel = Editor:get_selection ()
	local sr = Session:nominal_sample_rate ()
	local tm = Session:tempo_map ()
	local vamp = ARDOUR.LuaAPI.Vamp ("libardourvampplugins:qm-transcription", sr)
	local midi_region = nil
	local audio_regions = {}
	local start_time = Session:current_end_sample ()
	local end_time = Session:current_start_sample ()
	local max_pos = 0
	local cur_pos = 0
	for r in sel.regions:regionlist ():iter () do
		if r:to_midiregion():isnil() then
			local st = r:position()
			local ln = r:length()
			local et = st + ln
			if st < start_time then
				start_time = st
			end
			if et > end_time then
				end_time = et
			end
			table.insert(audio_regions, r)
			max_pos = max_pos + r:to_readable ():readable_length ()
		else
			midi_region = r:to_midiregion()
		end
	end

	if #audio_regions == 0 then
		LuaDialog.Message ("Polyphonic Audio to MIDI", "No source audio region(s) selected.\nAt least one audio-region to be analyzed need to be selected.", LuaDialog.MessageType.Error, LuaDialog.ButtonType.Close):run ()
		return
	end
	if not midi_region then
		LuaDialog.Message ("Polyphonic Audio to MIDI", "No target MIDI region selected.\nA MIDI region, ideally empty, and extending beyond the selected audio-region(s) needs to be selected.", LuaDialog.MessageType.Error, LuaDialog.ButtonType.Close):run ()
		return
	end

	midi_region:set_initial_position(start_time)
	midi_region:set_length(end_time - start_time, 0)

	local pdialog = LuaDialog.ProgressWindow ("Audio to MIDI", true)
	function progress (_, pos)
		return pdialog:progress ((cur_pos + pos) / max_pos, "Analyzing")
	end

	for i,ar in pairs(audio_regions) do
		local a_off = ar:position ()
		local b_off = midi_region:quarter_note () - midi_region:start_beats ()

		vamp:analyze (ar:to_readable (), 0, progress)

		if pdialog:canceled () then
			goto out
		end

		cur_pos = cur_pos + ar:to_readable ():readable_length ()
		pdialog:progress (cur_pos / max_pos, "Generating MIDI")

		local fl = vamp:plugin ():getRemainingFeatures ():at (0)
		if fl and fl:size() > 0 then
			local mm = midi_region:midi_source(0):model()
			local midi_command = mm:new_note_diff_command ("Audio2Midi")
			for f in fl:iter () do
				local ft = Vamp.RealTime.realTime2Frame (f.timestamp, sr)
				local fd = Vamp.RealTime.realTime2Frame (f.duration, sr)
				local fn = f.values:at (0)

				local bs = tm:exact_qn_at_sample (a_off + ft, 0)
				local be = tm:exact_qn_at_sample (a_off + ft + fd, 0)

				local pos = Evoral.Beats (bs - b_off)
				local len = Evoral.Beats (be - bs)
				local note = ARDOUR.LuaAPI.new_noteptr (1, pos, len, fn + 1, 0x7f)
				midi_command:add (note)
			end
			mm:apply_command (Session, midi_command)
		end
		-- reset the plugin (prepare for next iteration)
		vamp:reset ()
	end

	::out::
	pdialog:done ();
end end

function icon (params) return function (ctx, width, height, fg)
	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil(width * .7) .. "px")
	txt:set_text ("\u{2669}") -- quarter note symbol UTF8
	local tw, th = txt:get_pixel_size ()
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	txt:show_in_cairo_context (ctx)
end end
