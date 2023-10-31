ardour { ["type"] = "EditorAction", name = "Reverse MIDI events",
	license     = "MIT",
	author      = "Nil Geisweiller",
	description = [[Reverse MIDI events of selected MIDI regions, so that events at the end appear at the beginning and so on.  Reverse the order of MIDI regions as well, so that MIDI regions at the end appear at the beginning and so on.  Reverse individual notes as well, so the ending of a note corresponds to its beginning.  Thus, for this effect to yeld good results, the notes should rather be quantized.]]
}

function factory () return function ()
	print ("Reverse MIDI regions")
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
		for note in ARDOUR.LuaAPI.note_list (mm):iter () do
			local new_time = start + start + length - note:time () - note:length ()
			print ("new_time =", new_time)
		end

		-- TODO: support other MIDI events
		::next::
	end

	-- -- save undo
	-- if add_undo then
	-- 	local after = al:get_state ()
	-- 	Session:add_command (al:memento_command (before, after))
	-- 	Session:commit_reversible_command (nil)
	-- else
	-- 	Session:abort_reversible_command ()
	-- 	LuaDialog.Message ("Reverse MIDI events", "No event was reversed, was any non-empty region selected?", LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run ()
	-- end
end end

function icon (params) return function (ctx, width, height, fg)
	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil (height / 3) .. "px")
	txt:set_text ("Rev")
	local tw, th = txt:get_pixel_size ()
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	txt:show_in_cairo_context (ctx)
end end
