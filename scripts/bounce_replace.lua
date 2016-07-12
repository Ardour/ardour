ardour { ["type"] = "EditorAction", name = "Bounce+Replace Regions",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Bounce selected regions with processing and replace region]]
}

function factory (params) return function ()
	-- there is currently no direct way to find the track
	-- corresponding to a [selected] region
	function find_track_for_region (region_id)
		for route in Session:get_tracks():iter() do
			local track = route:to_track();
			local pl = track:playlist ()
			if not pl:region_by_id (region_id):isnil () then
				return track
			end
		end
		assert (0); -- can't happen, region must be in a playlist
	end

	-- get selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()

	-- prepare undo operation
	Session:begin_reversible_command ("Bounce+Replace Regions")
	local add_undo = false -- keep track if something has changed

	-- Iterate over Regions part of the selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:RegionSelection
	for r in sel.regions:regionlist ():iter () do
		-- each of the items 'r' is a
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Region

		local track = find_track_for_region (r:to_stateful():id())
		local playlist = track:playlist ()

		-- clear existing changes, prepare "diff" of state for undo
		playlist:to_stateful ():clear_changes ()

		-- bounce the region with processing
		local region = track:bounce_range (r:position (), r:position() + r:length (), ARDOUR.InterThreadInfo (), track:main_outs (), false);

		-- remove old region..
		playlist:remove_region (r);
		-- ..and add the newly bounced one
		playlist:add_region (region, r:position (), 1, false)

		-- create a diff of the performed work, add it to the session's undo stack
		-- and check if it is not empty
		if not Session:add_stateful_diff_command (playlist:to_statefuldestructible ()):empty () then
			add_undo = true
		end
	end

	-- all done, commit the combined Undo Operation
	if add_undo then
		-- the 'nil' Command here mean to use the collected diffs added above
		Session:commit_reversible_command (nil)
	else
		Session:abort_reversible_command ()
	end

end end
