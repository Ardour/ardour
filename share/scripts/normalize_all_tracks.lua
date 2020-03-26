ardour { ["type"] = "EditorAction",
	name = "Normalize All Tracks",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Normalize all regions using a common gain-factor per track.]]
}

function factory () return function ()
	-- target values -- TODO: use a LuaDialog.Dialog and ask..
	local target_peak = -1 --dBFS
	local target_rms = -18 --dBFS/RMS

	-- prepare undo operation
	Session:begin_reversible_command ("Normalize Tracks")
	local add_undo = false -- keep track if something has changed

	-- loop over all tracks in the session
	for track in Session:get_tracks():iter() do
		local norm = 0 -- per track gain
		-- loop over all regions on track
		for r in track:to_track():playlist():region_list():iter() do
			-- test if it's an audio region
			local ar = r:to_audioregion ()
			if ar:isnil () then goto next end

			local peak = ar:maximum_amplitude (nil)
			local rms  = ar:rms (nil)
			-- check if region is silent
			if (peak > 0) then
				local f_rms = rms / 10 ^ (.05 * target_rms)
				local f_peak = peak / 10 ^ (.05 * target_peak)
				local tg = (f_peak > f_rms) and f_peak or f_rms  -- max (f_peak, f_rms)
				norm = (tg > norm) and tg or norm -- max (tg, norm)
			end
			::next::
		end

		-- apply same gain to all regions on track
		if norm > 0 then
			for r in track:to_track():playlist():region_list():iter() do
				local ar = r:to_audioregion ()
				if ar:isnil () then goto skip end
				ar:to_stateful ():clear_changes ()
				ar:set_scale_amplitude (1 / norm)
				add_undo = true
				::skip::
			end
		end
	end

	-- all done. now commit the combined undo operation
	if add_undo then
		-- the 'nil' command here means to use all collected diffs
		Session:commit_reversible_command (nil)
	else
		Session:abort_reversible_command ()
	end
end end
