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
		local region = track:bounce_range (r:position (), r:position() + r:length (), ARDOUR.InterThreadInfo (), track:main_outs (), false, "");

		-- remove old region..
		playlist:remove_region (r);
		-- ..and add the newly bounced one
		playlist:add_region (region, r:position (), 1, false, 0, 0, false)

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

function icon (params) return function (ctx, width, height, fg)
	local wh = math.min (width, height) * .5
	local ar = wh * .2

	ctx:set_line_width (1)
	function stroke_outline (c)
		ctx:set_source_rgba (0, 0, 0, 1)
		ctx:stroke_preserve ()
		ctx:set_source_rgba (c, c, c, 1)
		ctx:fill ()
	end

	ctx:translate (math.floor (width * .5 - wh), math.floor (height * .5 - wh))
	ctx:rectangle (wh - wh * .6, wh - .7 * wh, wh * 1.2, .5 * wh)
	stroke_outline (.7)

	ctx:rectangle (wh - wh * .6, wh + .1 * wh, wh * 1.2, .5 * wh)
	stroke_outline (.9)

	-- arrow
	ctx:set_source_rgba (0, 1, 0, 1)
	ctx:set_line_width (ar * .7)

	ctx:move_to (wh, wh - .5 * wh)
	ctx:rel_line_to (0, wh)
	ctx:stroke ()

	ctx:move_to (wh, wh + .5 * wh)
	ctx:rel_line_to (-ar, -ar)
	ctx:rel_line_to (2 * ar, 0)
	ctx:close_path ()
	ctx:stroke ()



end end
