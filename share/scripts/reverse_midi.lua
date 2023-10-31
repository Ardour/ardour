ardour { ["type"] = "EditorAction", name = "Reverse MIDI Events",
	license     = "MIT",
	author      = "Nil Geisweiller",
	description = [[Reverse MIDI events of selected MIDI regions, so that events at the end appear at the beginning and so on.  Reverse the order of MIDI regions as well, so that MIDI regions at the end appear at the beginning and so on.  Reverse individual notes as well, so the ending of a note corresponds to its beginning.  Thus, for this effect to yeld good results, the notes should rather be quantized.]]
}

function factory () return function ()
	-- Reverse all selected MIDI regions
	local sel = Editor:get_selection ()
	for r in sel.regions:regionlist ():iter () do
		-- Get start and length of MIDI region
		local mr = r:to_midiregion ()
		if mr:isnil () then goto next end
		start = mr:start ():beats ()
		length = mr:length ():beats ()

		-- Iterate over all notes of the MIDI region and reverse them
		-- TODO: make sure it works for regions with hidden notes
		local mm = mr:midi_source(0):model ()
		local midi_command = mm:new_note_diff_command ("Reverse MIDI Events")
		for note in ARDOUR.LuaAPI.note_list (mm):iter () do
			local new_time = start + start + length - note:time () - note:length ()
			local new_note = ARDOUR.LuaAPI.new_noteptr (note:channel (), new_time, note:length (), note:note (), note:velocity ())
			midi_command:remove (note)
			midi_command:add (new_note)
		end
		mm:apply_command (Session, midi_command)

		-- TODO: support other MIDI events
		::next::
	end
end end

function icon (params) return function (ctx, width, height, fg)
	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil (height / 3) .. "px")
	txt:set_text ("Rev")
	local tw, th = txt:get_pixel_size ()
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	txt:show_in_cairo_context (ctx)
end end
