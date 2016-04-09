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
	assert (loop)
	assert (loop:start () < loop:_end ())
	assert (loop:_end () < playhead)

	for route in Session:get_tracks ():iter () do
		-- skip muted tracks
		if route:muted () then
			goto continue
		end
		-- test if bouncing is possible
		local track = route:to_track ()
		if not track:bounceable (proc, false) then
			goto continue;
		end
		-- only audio tracks
		local playlist = track:playlist ()
		if playlist:data_type ():to_string() ~= "audio" then
			goto continue
		end

		-- check if there are any regions in the loop-range of this track
		local range = Evoral.Range (loop:start (), loop:_end ())
		if playlist:regions_with_start_within (range):empty () then
			goto continue
		end
		if playlist:regions_with_end_within (range):empty () then
			goto continue
		end

		-- all set
		--print (track:name ())

		-- do the actual work
		local region = track:bounce_range (loop:start (), loop:_end (), itt, proc, false)
		playlist:add_region(region, playhead, npaste, false)

		::continue::
	end
end end
