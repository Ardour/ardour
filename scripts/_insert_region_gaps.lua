ardour {
	["type"]    = "EditorAction",
	name        = "Insert Gaps",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Insert gaps between all regions on selected tracks]]
}

function action_params ()
	return
	{
		["gap"] = { title = "Gap size (in sec)", default = "2" },
	}
end

function factory () return function ()
	-- get configuration
	local p = params or {}
	local gap = p["gap"] or 2
	if gap <= 0 then gap = 2 end

	local sel = Editor:get_selection () -- get current selection

	local add_undo = false -- keep track of changes
	Session:begin_reversible_command ("Insert Gaps")

	-- iterate over all selected tracks
	for route in sel.tracks:routelist ():iter () do
		local track = route:to_track ()
		if track:isnil () then goto continue end

		-- get track's playlist
		local playlist = track:playlist ()
		local offset = 0

		-- iterate over all regions in the playlist
		for region in playlist:region_list():iter() do

			-- preare for undo operation
			region:to_stateful ():clear_changes ()

			-- move region
			region:set_position (region:position() + offset, 0)
			offset = offset + Session:nominal_sample_rate () * gap

			-- create a diff of the performed work, add it to the session's undo stack
			-- and check if it is not empty
			if not Session:add_stateful_diff_command (region:to_statefuldestructible ()):empty () then
				add_undo = true
			end
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
