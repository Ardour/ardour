ardour {
	["type"]    = "EditorHook",
	name        = "Tempo Map Audio",
	author      = "Ardour Team",
	description = "Time Stretch Audio when Tempo Map changed",
}

-- subscribe to signals
-- http://manual.ardour.org/lua-scripting/class_reference/#LuaSignal.LuaSignal
function signals ()
	s = LuaSignal.Set()
	s:add ({[LuaSignal.TempoMapChanged] = true})
	return s
end

-- create callback functions
function factory ()
	-- callback function which invoked when signal is emitted
	return function (signal, ref, ...)
		-- 'TempoMapChanged' passes 3 aruguments: old and current tempo-map and a boolean from_undo
		local tmo, tmn, from_undo = ...

		if from_undo or not Session:collected_undo_commands () then
			return
		end

		function find_track_for_region (region_id)
			for route in Session:get_tracks ():iter () do
				local track = route:to_track ()
				local pl = track:playlist ()
				if not pl:region_by_id (region_id):isnil () then
					return track
				end
			end
			return nil
		end

		local all_regions = ARDOUR.RegionFactory.regions()

		-- copy region-map
		local regions = {}
		for _, r in all_regions:iter() do
			local ar = r:to_audioregion ()
			if ar:isnil () then goto next end
			local track = find_track_for_region (r:to_stateful ():id ())
			if not track then goto next end
			regions[ar] = track
			::next::
		end

		for ar, track in pairs (regions) do
			print ("Processing Region: ", ar:name (), track:name ())

			-- create Rubberband stretcher
			local rb = ARDOUR.LuaAPI.Rubberband (ar, false)

			-- the rubberband-filter also implements the readable API.
			-- https://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:AudioReadable
			-- This allows to read from the master-source of the given audio-region.
			-- XXX but, it will not work for incremental tempo-map changes :(
			local max_pos = rb:readable ():readable_length ()

			-- the beat-map is a table holding audio-sample positions:
			-- [from] = to
			local beat_map = {}
			beat_map[0] = 0

			-- get position of region
			local region_pos = ar:position()

			local beats = tmo:quarters_at (region_pos)
			local sample = 0
			local beat = 0

			if beats:get_ticks () > 0 then
				beat = beats:get_beats () + 1
			else
				beat = beats:get_beats ()
			end

			local old_pos = tmo:sample_at_beats (beats)
			local new_pos = tmn:sample_at_beats (beats)
			print ("start", old_pos, new_pos)

			while sample < max_pos do
				local b = Temporal.Beats (beat, 0)
				sample = tmo:sample_at_beats (b) - old_pos
				local to = tmn:sample_at_beats (b) - new_pos
				beat_map[sample] = to
				print ("map ", sample, "to", to)
				beat = beat + 1
			end

			local old_end_beats = tmo:quarters_at (Temporal.timepos_t (region_pos:samples () + max_pos))
			local new_end =  tmn:sample_at_beats (old_end_beats)
			local stretch = (new_end - new_pos) / max_pos
			print ("stretch factor", stretch, new_pos, new_end, max_pos)

			-- configure rubberband stretch tool
			rb:set_strech_and_pitch (stretch, 1) -- no overall stretching, no pitch-shift
			rb:set_mapping (beat_map) -- apply beat-map from/to

			-- now stretch the region
			function rb_progress (_, pos)
			end

			local nar = rb:process (rb_progress)

			-- replace region
			if not nar:isnil () then
				print ("new audio region: ", nar:name (), nar:length ())
				local playlist = track:playlist ()
				playlist:to_stateful ():clear_changes () -- prepare undo
				playlist:remove_region (ar)
				playlist:add_region (nar, Temporal.timepos_t(new_pos), 1, false)
				Session:add_stateful_diff_command (playlist:to_statefuldestructible ())
			end
		end

	end
end
