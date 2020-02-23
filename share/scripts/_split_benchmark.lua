-- [disable CPU freq scaling for benchmark]
-- create a session
-- add 16 mono tracks
-- record 2-3 mins on each track starting at 00:00:00:00
-- rewind the playhead to 00:00:00:00
-- run this script in  Menu > Window. Scripting  10 times
ardour { ["type"] = "EditorAction", name = "Split Benchmark" }

function factory (params) return function ()

	function split_at (pos)
		local add_undo = false -- keep track if something has changed
		Session:begin_reversible_command ("Auto Region Split")
		for route in Session:get_tracks():iter() do
			local playlist = route:to_track():playlist ()
			playlist:to_stateful ():clear_changes ()
			for region in playlist:regions_at (pos):iter () do
				playlist:split_region (region, ARDOUR.MusicSample (pos, 0))
			end
			if not Session:add_stateful_diff_command (playlist:to_statefuldestructible ()):empty () then
				add_undo = true
			end
		end
		if add_undo then
			Session:commit_reversible_command (nil)
		else
			Session:abort_reversible_command ()
		end
	end

	function count_regions ()
		local total = 0
		for route in Session:get_tracks():iter() do
			total = total + route:to_track():playlist():region_list():size()
		end
		return total
	end

	for x = 1, 3 do
		local playhead = Session:transport_sample ()

		local step = Session:samples_per_timecode_frame()
		local n_steps = 20

		local t_start = ARDOUR.LuaAPI.monotonic_time ()
		for i = 1, n_steps do
			split_at (playhead + step * i)
		end
		local t_end = ARDOUR.LuaAPI.monotonic_time ()

		Session:request_locate((playhead + step * n_steps), ARDOUR.LocateTransportDisposition.MustStop, ARDOUR.TransportRequestSource.TRS_UI)
		print (count_regions (), (t_end - t_start) / 1000 / n_steps)
		collectgarbage ();
		ARDOUR.LuaAPI.usleep(500000)
	end


end end
