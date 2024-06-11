ardour { ["type"] = "EditorAction", name = "Select Regions at the Playhead",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Select regions under the playhead on selected track(s)]]
}

function factory (params) return function ()

	local loc = Session:locations () -- all marker locations

	-- get the playhead postion
	local playhead = Temporal.timepos_t (Session:transport_sample ())

	local sl = ArdourUI.SelectionList () -- empty selection list

	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()

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

		for region in playlist:regions_at (playhead):iter () do
			local rv = Editor:regionview_from_region (region)
			sl:push_back (rv);
		end

		::continue::
	end

  -- set/replace current selection in the editor
  Editor:set_selection (sl, ArdourUI.SelectionOp.Set);
end end
