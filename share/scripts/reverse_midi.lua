ardour { ["type"] = "EditorAction", name = "Reverse MIDI events",
	license     = "MIT",
	author      = "Nil Geisweiller",
	description = [[Reverse MIDI events of selected MIDI regions, so that events at the end appear at the beginning and so on.  Reverse the order of MIDI regions as well, so that MIDI regions at the end appear at the beginning and so on.  Reverse individual notes as well, so the ending of a note corresponds to its beginning.  Thus, for this effect to yeld good results, the notes should rather be quantized.]]
}

function factory () return function ()
	print ("Reverse MIDI regions, man!")
	-- Reverse all selected MIDI regions
	local sel = Editor:get_selection ()
	for r in sel.regions:regionlist ():iter () do
		print ("Iterating through selected regions baby!")
		local mr = r:to_midiregion ()
		if mr:isnil () then goto next end

		print ("mr =", mr, ", mr:position () =", mr:position ())
		local mm = mr:midi_source(0):model ()
		print ("mm =", mm)
		-- Look for NotePtr in luabindings.cc
		
		-- -- get list of MIDI-CC events for the given channel and parameter
		-- local ec = mr:control (Evoral.Parameter (ARDOUR.AutomationType.MidiCCAutomation, midi_channel, cc_param), false)
		-- if ec:isnil () then goto next end
		-- if ec:list ():events ():size () == 0 then goto next end

		-- -- iterate over CC-events
		-- for av in ec:list ():events ():iter () do
		-- 	-- re-scale event to target range
		-- 	local val = pd.lower + (pd.upper - pd.lower) * av.value / 127
		-- 	-- and add it to the target-parameter automation-list
		-- 	al:add (r:position () - r:start () + av.when, val, false, true)
		-- 	add_undo = true
		-- end
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
