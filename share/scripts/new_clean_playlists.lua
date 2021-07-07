ardour { ["type"] = "EditorAction", name = "New clean playlists",
	license     = "MIT",
	author      = "Mathieu Picot (Houston4444)",
	description = [[Copy the current playlist of selected tracks to a new playlist with audible regions only.]]
}

function factory (params) return function ()

	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()

	-- Track/Bus Selection -- iterate over all Editor-GUI selected tracks
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:TrackSelection
	for route in sel.tracks:routelist():iter() do

		-- each of the items 'route' is-a
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Route
		local track = route:to_track() -- see if it's a track
		if track:isnil() then
			-- if not, skip it
			goto next_route
		end
        
		local is_audio = false
		if track:data_type():to_string() == "audio" then
			is_audio = true
		end

		local main_pl_name = track:name() .. "._MAIN_"
		local main_pl_ex_name = track:name() .. "_ExMAIN_"

		-- find the playlist name track_name._MAIN_
		for pl in Session:playlists():playlists_for_track (track):iter() do
			if pl:name() == main_pl_name then
				pl:set_name(main_pl_ex_name)
				break
			end
		end

		-- copy current playlist, get new playlist and rename it
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Playlist
        track:use_copy_playlist()
		local playlist = track:playlist()
		playlist:set_name(track:name() .. "._MAIN_")

		-- we will stock here the values needed to modify/remove regions
		local regions_to_modify = {}

		for r in playlist:region_list():iter() do
			local r_front = r:position()
			local r_end = r_front + r:length()

			-- region can be finally splited into many segments
			-- 'segments' is a table containing segments with the form
			-- {{seg1_start, seg1_end}, {seg2_start, seg2_finish}...}
			-- segments will finally be regions
			local segments = {{r_front, r_end}}

			-- values used for audio tracks to prevent cut during an audible fade
			local r_no_cut_before = r_front
			local r_no_cut_after = r_end

			if is_audio then
				local ra = r:to_audioregion()

				if ra:fade_in_active() then
					r_no_cut_before = r_front + ra:fade_in_length() + 64
				end

				if ra:fade_out_active() then
					r_no_cut_after = r_end - ra:fade_out_length() - 64
				end
			end

			for rg in playlist:region_list():iter() do
				-- ignore regions equal or above this one
				if rg:layer() <= r:layer() then
					goto next_rg
				end

				-- get points between which lower regions can be cut
				local cut_point_left = rg:position()
				local cut_point_right = cut_point_left + rg:length()

				if is_audio then
					rga = rg:to_audioregion()

					cut_point_left  = cut_point_left + 64
					cut_point_right = cut_point_right - 64

					if rga:fade_in_active() then
						cut_point_left = cut_point_left + rga:fade_in_length()
					end

					if rga:fade_out_active() then
						cut_point_right = cut_point_right - rga:fade_out_length()
					end
				end

				-- ignore regions with no overlap with this one
				if cut_point_left >= segments[#segments][2]
						or cut_point_right <= segments[1][1] then
					goto next_rg
				end

				-- stock here the segments ids we should remove
				local remove_id_list = {}

				-- iterate segments
				for i, s in pairs(segments) do
					if cut_point_right <= s[1] or cut_point_left >= s[2] then
						-- rg:  ___      or      ____
						-- s :      ___      ____

						-- compared region doesn't overlap this segment
						goto next_segment

					elseif cut_point_left <= s[1] and cut_point_right >= s[2] then
						-- rg:  ________
						-- s :    ____
						
						-- compared region exceed both front and end
						-- this segment will be removed
						table.insert(remove_id_list, i)
						goto next_segment

					elseif cut_point_left <= s[1] then
						-- rg:  ____
						-- s :    _____

						if is_audio then
							-- audio segment is cut 64 samples before compared region fade out
							-- in the limit of the original segment
							-- it also do not cut during the region fades
							if cut_point_right > r_no_cut_before then
								s[1] = math.min(cut_point_right,
								                r_no_cut_after)
							end
						else
							s[1] = cut_point_right
						end

					elseif cut_point_right >= s[2] then
						-- rg:     _____
						-- s :  _____

						if is_audio then
							-- audio segment is cut 64 samples after compared region fade in
							-- in the limit of the original segment
							if cut_point_left < r_no_cut_after then
								s[2] = math.max(cut_point_left,
								                r_no_cut_before)
							end
						else
							s[2] = cut_point_left
						end

					else
						-- rg:      B___C
						-- s :  A___________D

						-- worst case, compared region rg is above r,
                        -- rg starts after and finish before r.

						point_b = cut_point_left
						point_c = cut_point_right

						if is_audio then
							point_b = math.max(point_b, r_no_cut_before)
							point_c = math.min(point_c, r_no_cut_after)

							if point_b > r_no_cut_after or point_c < r_no_cut_before then
								-- we must keep the fade integrity
								goto next_segment
							end
						end

						-- we can modify the table now because
						-- we break the iteration loop just after.
						-- add the new segment C->D
						table.insert(segments, i + 1, {point_c, s[2]})

						-- reduce the original segment (from A->D to A->B)
						s[2] = point_b
						break
					end
					::next_segment::
				end

				-- remove segments that need to be removed
				for i=1, #remove_id_list do
					table.remove(
						segments, remove_id_list[#remove_id_list +1 -i])
				end

				-- if all segments are removed, region will be deleted
				if segments[1] == nil then break end

				::next_rg::
			end

			-- security check
			-- prevent segments and gaps shorter than 128
			-- and overlap segments from the same region
			local last_end = -1
			local new_segments = {}
 
			for i, s in pairs(segments) do
				if s[2] - s[1] > 128 then
					if s[1] > last_end + 128 then
						table.insert(new_segments, s)
					else
						new_segments[#new_segments][2] = s[2]
					end
				end 
				last_end = s[2]
			end

			-- remember the regions we have to delete or modify
			table.insert(regions_to_modify, {r, new_segments})
        end

		-- Remove, split or cut needed regions
		for i=1, #regions_to_modify do
			r = regions_to_modify[i][1]
			segments = regions_to_modify[i][2]

			if #segments == 0 then
				playlist:remove_region(r)
			else
				for j=1, #segments do
					if j == #segments then
						-- last segment, cut directly this region
						rg = r
					else
						-- 2 segments or more for this region
						-- So we need to clone the region before to cut it
						rg = ARDOUR.RegionFactory.clone_region(r, true, false)
						playlist:add_region(rg, rg:position(), 1, false, 0, 0, false)
						
						while rg:layer() > r:layer()
						do
							rg:lower()
						end
					end

					-- finally cut the region if needed
					if segments[j][1] ~= rg:position() then
						rg:cut_front(segments[j][1], 0)
					end

					if segments[j][2] ~= rg:position() + rg:length() then
						rg:cut_end(segments[j][2], 0)
					end
				end
			end
		end
		::next_route::
	end
end end
