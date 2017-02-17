ardour { ["type"] = "EditorAction", name = "Marker Split",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Split regions on selected tracks at all locations markers]]
}

function icon (params) return function (ctx, w, h, fg)
	local mh = h - 3.5;
	local m3 = w / 3;
	local m6 = w / 6;
	ctx:set_source_rgba (.8, .8, .2, 1.0)
	ctx:move_to (w / 2 - m6, 2)
	ctx:rel_line_to (m3, 0)
	ctx:rel_line_to (0, mh * 0.4)
	ctx:rel_line_to (-m6, mh * 0.6)
	ctx:rel_line_to (-m6, -mh * 0.6)
	ctx:close_path ()
	ctx:fill ()
	-- TODO: better draw a left/right arrow <--|-->
	-- yet this is a handy text example, so..
	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. (h - 7) .. "px")
	ctx:set_source_rgba (1, 0, 0, 0.7)
	txt:set_text ("S")
	local tw, th = txt:get_pixel_size ()
	ctx:move_to (.5 * (w - tw), 1)
	txt:show_in_cairo_context (ctx)
end end

function factory (params) return function ()

	local loc = Session:locations () -- all marker locations

	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()

	-- prepare undo operation
	-- see http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Session
	Session:begin_reversible_command ("Auto Region Split")
	local add_undo = false -- keep track if something has changed

	-- Track/Bus Selection -- iterate over all Editor-GUI selected tracks
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:TrackSelection
	for r in sel.tracks:routelist ():iter () do
		-- each of the items 'r' is-a
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Route

		local track = r:to_track () -- see if it's a track
		if track:isnil () then
			-- if not, skip it
			goto continue
		end

		-- get track's playlist
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Playlist
		local playlist = track:playlist ()

		-- clear existing changes, prepare "diff" of state for undo
		playlist:to_stateful ():clear_changes ()

		-- iterate over all location markers
		for l in loc:list ():iter () do
			if l:is_mark() then
				-- get all regions on the given track's playlist (may be stacked)
				for reg in playlist:regions_at (l:start ()):iter () do
					playlist:split_region (reg, ARDOUR.MusicFrame (l:start(), 0))
					-- the above operation will invalidate the playlist's region list:
					-- split creates 2 new regions.
					--
					-- Hence this script does it the way it does: the inner-most loop
					-- is over playlist-regions.
				end
			end
		end

		-- collect undo data
		if not Session:add_stateful_diff_command (playlist:to_statefuldestructible ()):empty () then
			-- is something has changed, we need to save it at the end.
			add_undo = true
		end

		::continue::
	end

	-- all done, commit the combined Undo Operation
	if add_undo then
		-- the 'nil' Command here mean to use the collected diffs added above
		Session:commit_reversible_command (nil)
	else
		Session:abort_reversible_command ()
	end

end end
