ardour { ["type"] = "EditorAction", name = "Reverse MIDI Events",
	license     = "MIT",
	author      = "Nil Geisweiller",
	description = [[Chronologically reverse MIDI notes of selected MIDI regions.  The positions of the MIDI regions are reversed as well, meaning regions at the end appear at the beginning and so on.  Individual notes are reversed so the ending of a note corresponds to its beginning.  Thus notes should be quantized for this effect to yield good results.  Note that only MIDI notes are reversed.  Other MIDI events such as CC or SYSEX are left unchanged.]]
}

function factory () return function ()
	-- Calculate the minimal position and the maximum length of the
	-- selection, ignoring non-MIDI region.
	local sel = Editor:get_selection ()
	local sel_position = Temporal.timepos_t.max (Temporal.TimeDomain.BeatTime)
	local sel_end = Temporal.timepos_t.zero ()
	for r in sel.regions:regionlist ():iter () do
		-- Skip non-MIDI region
		local mr = r:to_midiregion ()
		if mr:isnil () then goto continue1 end

		-- Update sel_position and sel_end
		if r:position () < sel_position then sel_position = r:position () end
		local r_end = r:position () + r:length ()
		if sel_end < r_end then sel_end = r_end end

		::continue1::
	end
	local sel_length = sel_end - sel_position

	-- Reverse the order of selected MIDI regions
	for r in sel.regions:regionlist ():iter () do
		-- Skip non-MIDI region
		local mr = r:to_midiregion ()
		if mr:isnil () then goto continue2 end

		-- Reverse region position
		local old_position = r:position ()
		local new_position = sel_position + sel_position + sel_length - old_position - r:length ()
		if new_position ~= old_position then r:set_position (new_position) end

		::continue2::
	end

	-- Reverse the content inside selected MIDI regions
	for r in sel.regions:regionlist ():iter () do
		-- Skip non-MIDI region
		local mr = r:to_midiregion ()
		if mr:isnil () then goto continue3 end

		-- Get start and length of MIDI region
		local rstart = mr:start ():beats ()
		local rlength = mr:length ():beats ()
		local rend = rstart + rlength

		-- Iterate over all notes of the MIDI region and reverse them
		local mm = mr:midi_source(0):model ()
		local midi_command = mm:new_note_diff_command ("Reverse MIDI Events")
		for note in ARDOUR.LuaAPI.note_list (mm):iter () do
			-- Skip notes that are not within the region visible range
			local old_time = note:time ()
			if old_time < rstart or rend < old_time then goto continue4 end

			-- Reverse if within the visible range
			local new_time = rstart + rend - old_time - note:length ()
			if new_time ~= old_time then
				local new_note = ARDOUR.LuaAPI.new_noteptr (note:channel (), new_time, note:length (), note:note (), note:velocity ())
				midi_command:remove (note)
				midi_command:add (new_note)
			end

			::continue4::
		end
		mm:apply_command (Session, midi_command)

		-- TODO: support other MIDI events
		::continue3::
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
