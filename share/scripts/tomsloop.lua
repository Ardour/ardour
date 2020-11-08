ardour { ["type"] = "EditorAction", name = "Tom's Loop",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Bounce the loop-range of all non muted audio tracks, paste at playhead]]
}

-- main method, every custom (i.e. non-ardour) method must be defined *inside* factory()
function factory (params) return function ()

-- when this script is called as an action, the output will be printed to the ardour log window
	function print_help()
		print("")
		print("---------------------------------------------------------------------")
		print("")
		print("Manual for \"Tom’s Loop\" Ardour Lua Script")
		print("")
		print("---------------------------------------------------------------------")
		print("---------------------------------------------------------------------")
		print("")
		print("Table of Contents")
		print("")
		print("1. The first test")
		print("2. Using mute and solo")
		print("3. Combining region clouds to a defined length")
		print("")
		print("Abstract: This script for Ardour (>=4.7 git) operates on the time")
		print("line. It allows to copy and combine specific portions within the loop")
		print("range to a later point on the time line with one single action")
		print("command. Everything that can be heard within the loop range is")
		print("considered for this process, namely non-muted regions on non-muted or")
		print("soloed tracks that are fully or partially inside the loop range. This")
		print("still sounds a bit abstract and will be more obvious with the")
		print("following example cases of use.")
		print("")
		print("For convenience, it’s recommended to bind the script to a keyboard")
		print("shortcut in order to quickly and easily access the \"Tom’s Loop\"")
		print("scripted action.")
		print("")
		print("-Open dialog \"Script Manager\" via menu Edit/Scripted Actions/Script")
		print("Manager")
		print("")
		print("-In tab \"Action Scripts\", select a line and press button \"Add/Set\"")
		print("")
		print("-In dialog \"Add Lua Action\", select \"Tom’s Loop\" from the drop down")
		print("menu and hit \"Add\"")
		print("")
		print("-In dialog \"Set Script Parameter\" just hit \"Add\" again")
		print("")
		print("-Close dialog \"Script Manager\"")
		print("")
		print("-Open dialog \"Bindings Editor\" via menu Window/Bindings Editor")
		print("")
		print("-In tab \"Editor\", expand \"Editor\", look for entry \"Tom’s loop\",")
		print("select it")
		print("")
		print("-Hit the keyboard shortcut to assign to this scripted action")
		print("")
		print("-Close dialog \"Key Bindings\"")
		print("")
		print("An alternative way to quickly access a scripted action is to enable")
		print("\"Action Script Button Visibility\" in \"Preferences/GUI\".")
		print("")
		print("---------------------------------------------------------------------")
		print("")
		print("1. The first test")
		print("")
		print("---------------------------------------------------------------------")
		print("")
		print("-Record a short sequence of audio input or import a wave file to a")
		print("track to get a region")
		print("")
		print("-Set a loop range inside that one region")
		print("")
		print("-Place the playhead after the loop range, possibly after the region,")
		print("non-rolling")
		print("")
		print("     _L====L_              V")
		print(" .____|____|____________.  |")
		print(" |R1__|_x__|____________|  |")
		print("")
		print("-Call \"Tom’s Loop\" via the previously created shortcut")
		print("")
		print("This results in a new region created at the playhead, with the length")
		print("of the loop range, containing audio of the original region. The")
		print("playhead moved to the end of this new region so that subsequent calls")
		print("to \"Tom’s Loop\" will result in a gap less series of regions.")
		print("")
		print("     _L====L_               --> V")
		print(" .____|____|____________.  .____|")
		print(" |R1__|_x__|____________|  |_x__|")
		print("")
		print("-Repeat calling \"Tom’s Loop\"")
		print("")
		print("This creates multiple copies of the loop range to line up one after")
		print("each other.")
		print("")
		print("     _L====L_                         --> V")
		print(" .____|____|____________.  .______________|")
		print(" |R1__|_x__|____________|  |_x__|_x__|_x__|")
		print("")
		print("-Set a different loop range and call \"Tom’s Loops\" again")
		print("")
		print("This will create a new region with the length of the new loop range")
		print("at the playhead.")
		print("")
		print("        _L=======L_                           --> V")
		print(" ._______|_______|______.  .______________________|")
		print(" |R1_____|_X_____|______|  |_x__|_x__|_x__|_X_____|")
		print("")
		print("By using \"Tom’s Loop\", the loop range - which can be easily set with")
		print("the handles - and the playhead it’s easy to create any sequence of")
		print("existing regions on the time line. This can be useful during the")
		print("arrangement phase where macro parts of the session are already")
		print("temporally layed out (in the loop) but not part of the final")
		print("arrangement yet. The process is non-destructive in a sense that the")
		print("existing regions layout in the current loop range won’t be touched or")
		print("replaced. The newly created regions are immediately visible on the")
		print("time line at the playhead position.")
		print("")
		print("")
		print("---------------------------------------------------------------------")
		print("")
		print("2. Using mute and solo")
		print("")
		print("---------------------------------------------------------------------")
		print("")
		print("Creating a sequence of regions like described above respects the")
		print("current mute and solo state of a track. Variations of the loop are")
		print("thus easy to create, further supporting the arrangement process.")
		print("")
		print("      _L====L_                         --> V")
		print("  .____|____|____________.  ._________.    |")
		print("  |R1__|_x__|____________|  |_x__|_x__|    |")
		print(" .__|R2|_y__|________|_.    |_y__|_________|")
		print(" |R3___|_z__|__________|         |_z__|_z__|")
		print("")
		print("")
		print("---------------------------------------------------------------------")
		print("")
		print("3. Combining region clouds to a defined length")
		print("")
		print("---------------------------------------------------------------------")
		print("")
		print("Multiple small regions say on a percussive track can be simplified")
		print("for later arrangement keeping the temporal relations by combining")
		print("them. Using \"Tom’s Loop\", the resulting regions will not only combine")
		print("the regions but also automatically extend or shrink the new regions")
		print("start and end point so that it is exactly of the wished length equal")
		print("to the loop range.")
		print("")
		print("_L======================L_                            --> V")
		print(" |   .____  .___.  _____|_______.  .______________________|")
		print(" |   |R1_|  |R2_|  |R3__|_______|  |______________________|")
		print("")
		print("See also: Lua Action Bounce+Replace Regions")
		print("")
		print("")
	end -- print_help()

	local n_paste  = 1
	assert (n_paste > 0)

	local proc     = ARDOUR.LuaAPI.nil_proc () -- bounce w/o processing
	local itt      = ARDOUR.InterThreadInfo () -- bounce progress info (unused)

	local loop     = Session:locations ():auto_loop_location ()
	local playhead = Session:transport_sample ()

	-- make sure we have a loop, and the playhead (edit point) is after it
	if not loop then
--		print_help();
		print ("Error: A Loop range must be set.")
		LuaDialog.Message ("Tom's Loop", "Error: A Loop range must be set.", LuaDialog.MessageType.Error, LuaDialog.ButtonType.Close):run ()
		goto errorout
	end
	assert (loop:start () < loop:_end ())
	if loop:_end () > playhead then
--		print_help();
		print ("Error: The Playhead (paste point) needs to be after the loop.")
		LuaDialog.Message ("Tom's Loop", "Error: The Playhead (paste point) needs to be after the loop.", LuaDialog.MessageType.Error, LuaDialog.ButtonType.Close):run ()
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
		local region = track:bounce_range (loop:start (), loop:_end (), itt, proc, false, "")
		playlist:add_region (region, playhead, n_paste, false, 0, 0, false)

		n_regions_created = n_regions_created + 1

		-- create a diff of the performed work, add it to the session's undo stack
		-- and check if it is not empty
		if not Session:add_stateful_diff_command (playlist:to_statefuldestructible ()):empty () then
			add_undo = true
		end

		::continue::
	end -- for all routes

	--advance playhead so it's just after the newly added regions
	if n_regions_created > 0 then
		Session:request_locate (playhead + loop:length() * n_paste, ARDOUR.LocateTransportDisposition.MustStop, ARDOUR.TransportRequestSource.TRS_UI)
	end

	-- all done, commit the combined Undo Operation
	if add_undo then
		-- the 'nil' Command here mean to use the collected diffs added above
		Session:commit_reversible_command (nil)
	else
		Session:abort_reversible_command ()
	end

	print ("bounced " .. n_regions_created .. " regions from loop range (" .. loop:length() ..  " samples) to playhead @ sample # " .. playhead)
	::errorout::
end -- end of anonymous action script function
end -- end of script factory


function icon (params) return function (ctx, width, height)
	local x = width * .5
	local y = height * .5
	local r = math.min (x, y)

	ctx:set_line_width (1)

	function stroke_outline ()
		ctx:set_source_rgba (0, 0, 0, 1)
		ctx:stroke_preserve ()
		ctx:set_source_rgba (1, 1, 1, 1)
		ctx:fill ()
	end

	ctx:rectangle (x - r * .6, y - r * .05, r * .6, r * .3)
	stroke_outline ()

	ctx:arc (x, y, r * .61, math.pi, 0.2 * math.pi)
	ctx:arc_negative (x, y, r * .35, 0.2 * math.pi, math.pi);
	stroke_outline ()

	function arc_arrow (rad, ang)
		return x - rad * math.sin (ang * 2.0 * math.pi), y - rad * math.cos (ang * 2.0 * math.pi)
	end

	ctx:move_to (arc_arrow (r * .36, .72))
	ctx:line_to (arc_arrow (r * .17, .72))
	ctx:line_to (arc_arrow (r * .56, .60))
	ctx:line_to (arc_arrow (r * .75, .72))
	ctx:line_to (arc_arrow (r * .62, .72))

	ctx:set_source_rgba (0, 0, 0, 1)
	ctx:stroke_preserve ()
	ctx:close_path ()
	ctx:set_source_rgba (1, 1, 1, 1)
	ctx:fill ()
end end
