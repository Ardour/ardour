ardour { ["type"] = "EditorAction", name = "Tom's Loop",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Bounce the loop-range of all non muted audio tracks, paste N times at playhead]]
}

function action_params ()
	return { ["times"]   = { title = "Number of copies to add", default = "1"}, }
end

function factory (params) return function ()
	-- get options
	local p = params or {}
	local n_paste  = tonumber (p["times"] or 1)
	assert (n_paste > 0)

	local proc     = ARDOUR.LuaAPI.nil_proc () -- bounce w/o processing
	local itt      = ARDOUR.InterThreadInfo () -- bounce progress info (unused)

	local loop     = Session:locations ():auto_loop_location ()
	local playhead = Session:transport_frame ()

	-- make sure we have a loop, and the playhead (edit point) is after it
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

	-- prefer solo'ed tracks
	local soloed_track_found = false
	for route in Session:get_tracks ():iter () do
		if route:soloed () then
			soloed_track_found = true
			break
		end
	end

	-- count regions that are bounced
	local n_regions_created = 0

	-- loop over all tracks in the session
	for route in Session:get_tracks ():iter () do
		if soloed_track_found then
			-- skip not soloed tracks
			if not route:soloed () then
				goto continue
			end
		end

		-- skip muted tracks (also applies to soloed + muted)
		if route:muted () then
			goto continue
		end

		-- at this point the track is either soloed (if at least one track is soloed)
		-- or not muted (if no track is soloed)

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

		-- check if there is at least one unmuted region in the loop-range
		local reg_unmuted_count = 0
		for reg in playlist:regions_touched (loop:start (), loop:_end ()):iter () do
			if not reg:muted() then
				reg_unmuted_count = reg_unmuted_count + 1
			end
		end

		if reg_unmuted_count < 1 then
			goto continue
		end

		-- clear existing changes, prepare "diff" of state for undo
		playlist:to_stateful ():clear_changes ()

		-- do the actual work
		local region = track:bounce_range (loop:start (), loop:_end (), itt, proc, false)
		playlist:add_region (region, playhead, n_paste, false)

		n_regions_created = n_regions_created + 1

		-- create a diff of the performed work, add it to the session's undo stack
		-- and check if it is not empty
		if not Session:add_stateful_diff_command (playlist:to_statefuldestructible ()):empty () then
			add_undo = true
		end

		::continue::
	end

	--advance playhead so it's just after the newly added regions
	if n_regions_created > 0 then
		Session:request_locate((playhead + loop:length() * n_paste),false)
	end

	-- all done, commit the combined Undo Operation
	if add_undo then
		-- the 'nil' Command here mean to use the collected diffs added above
		Session:commit_reversible_command (nil)
	else
		Session:abort_reversible_command ()
	end

	print ("bounced " .. n_regions_created .. " regions from loop range (" .. loop:length() ..  " frames) to playhead @ frame # " .. playhead)

	::errorout::
end end
