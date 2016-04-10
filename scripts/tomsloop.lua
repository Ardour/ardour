ardour { ["type"] = "Snippet", name = "Tom's Loop",
	license     = "MIT",
	author      = "Robin Gareus",
	email       = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[Bounce the loop-range of all non muted audio tracks, paste N times at playhead]]
}

-- unused ;  cfg parameter for ["type"] = "EditorAction"
function action_params ()
	return { ["times"]   = { title = "Number of copies to add", default = "1"}, }
end

function factory () return function ()
	-- get options
	local p = params or {}
	local npaste   = p["times"] or 1
	assert (npaste > 0)

	local proc     = ARDOUR.LuaAPI.nil_proc () -- bounce w/o processing
	local itt      = ARDOUR.InterThreadInfo () -- bounce progress info (unused)

	local loop     = Session:locations ():auto_loop_location ()
	local playhead = Session:transport_frame ()

	-- make sure we have a loop, and the playhead (edit point) is after it
	-- TODO: only print an error and return
	-- TODO: use the edit-point (not playhead) ? maybe.
	if not loop then
		print ("A Loop range must be set.")
		goto errorout
	end
	assert (loop:start () < loop:_end ())
	if loop:_end () >= playhead then
		print ("The Playhead (paste point) needs to be after the loop.")
		goto errorout
	end

	-- prepare undo operation
	Session:begin_reversible_command ("Tom's Loop")
	local add_undo = false -- keep track if something has changed

	for route in Session:get_tracks ():iter () do
		-- skip muted tracks
		if route:muted () then
			goto continue
		end
		-- test if bouncing is possible
		local track = route:to_track ()
		if not track:bounceable (proc, false) then
			goto continue
		end
		-- only audio tracks
		local playlist = track:playlist ()
		if playlist:data_type ():to_string () ~= "audio" then
			goto continue
		end

		-- check if there are any regions in the loop-range of this track
		local range = Evoral.Range (loop:start (), loop:_end ())
		if playlist:regions_with_start_within (range):empty ()
			and playlist:regions_with_end_within (range):empty () then
			goto continue
		end

		-- clear existing changes, prepare "diff" of state
		playlist:to_stateful ():clear_changes ()

		-- do the actual work
		local region = track:bounce_range (loop:start (), loop:_end (), itt, proc, false)
		playlist:add_region (region, playhead, npaste, false)

		-- create a diff of the performed work, add it to the session's undo stack
		-- and check if it is not empty
		if not Session:add_stateful_diff_command (playlist:to_statefuldestructible ()):empty () then
			add_undo = true
		end

		::continue::
	end
	-- all done, commit the combined Undo Operation
	if add_undo then
		-- the 'nil' Commend here mean to use the collected diffs added above
		Session:commit_reversible_command (nil)
	else
		Session:abort_reversible_command ()
	end
	::errorout::
end end
