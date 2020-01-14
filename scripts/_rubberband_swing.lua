ardour {
	["type"] = "EditorAction",
	name     = "Swing It (Rubberband)",
	license  = "MIT",
	author   = "Ardour Team",
description = [[
Create a 'swing feel' in selected regions.

Analyze beat-position from the selected audio regions,
then time-stretch the audio and move 8th notes back in
time while keeping 1/4 note beats in place.

(This script also servers as example for both VAMP
analysis as well as Rubberband region stretching.)

Kudos to Chris Cannam.
]]
}

function factory () return function ()

	-- helper function --
	-- there is currently no direct way to find the track
	-- corresponding to a [selected] region
	function find_track_for_region (region_id)
		for route in Session:get_tracks ():iter () do
			local track = route:to_track ()
			local pl = track:playlist ()
			if not pl:region_by_id (region_id):isnil () then
				return track
			end
		end
		assert (0) -- can't happen, region must be in a playlist
	end

	-- get Editor selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Editor
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()

	-- Instantiate the QM BarBeat Tracker
	-- see http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:LuaAPI:Vamp
	-- http://vamp-plugins.org/plugin-doc/qm-vamp-plugins.html#qm-barbeattracker
	local vamp = ARDOUR.LuaAPI.Vamp ("libardourvampplugins:qm-barbeattracker", Session:nominal_sample_rate ())

	-- prepare undo operation
	Session:begin_reversible_command ("Rubberband Regions")
	local add_undo = false -- keep track if something has changed

	-- for each selected region
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:RegionSelection
	for r in sel.regions:regionlist ():iter () do
		-- "r" is-a http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Region

		-- test if it's an audio region
		local ar = r:to_audioregion ()
		if ar:isnil () then
			goto next
		end

		-- create Rubberband stretcher
		local rb = ARDOUR.LuaAPI.Rubberband (ar, false)

		-- the rubberband-filter also implements the readable API to read from
		-- the master-source of the given audio-region (ignoring any prior time-stretch
		-- or pitch-shiting).
		-- https://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Readable
		local max_pos = rb:readable ():readable_length ()

		-- prepare table to hold analysis results
		-- the beat-map is a table holding audio-sample positions:
		-- [from] = to
		local beat_map = {}
		local prev_beat = 0

		local pdialog = LuaDialog.ProgressWindow ("Rubberband", true)

		-- callback to handle Vamp-Plugin analysis results
		function vamp_callback (_, pos)
			return pdialog:progress (pos / max_pos, "Analyzing")
		end
		function rb_progress (_, pos)
			return pdialog:progress (pos / max_pos, "Stretching")
		end

		-- run VAMP plugin, analyze the first channel of the audio-region
		vamp:analyze (rb:readable (), 0, vamp_callback)
		-- getRemainingFeatures returns a http://manual.ardour.org/lua-scripting/class_reference/#Vamp:Plugin:FeatureSet
		-- get the first output. here: Beats, estimated beat locations & beat-number
		-- "fl" is-a http://manual.ardour.org/lua-scripting/class_reference/#Vamp:Plugin:FeatureList
		local fl = vamp:plugin ():getRemainingFeatures ():at (0)
		local beatcount = 0
		-- iterate over returned features
		for f in fl:iter () do
			-- "f" is-a  http://manual.ardour.org/lua-scripting/class_reference/#Vamp:Plugin:Feature
			local fn = Vamp.RealTime.realTime2Frame (f.timestamp, Session:nominal_sample_rate ())
			beat_map[fn] = fn -- keep beats (1/4 notes) unchanged
			if prev_beat > 0 then
				-- move the half beats (1/8th) back
				local diff = (fn - prev_beat) / 2
				beat_map[fn - diff] = fn - diff + diff / 3 -- moderate swing 2:1 (triplet)
				--beat_map[fn - diff] = fn - diff + diff / 2 -- hard swing 3:1 (dotted 8th)
				beatcount = beatcount + 1
			end
			prev_beat = fn
		end
		-- reset the plugin (prepare for next iteration)
		vamp:reset ()

		if pdialog:canceled () then goto out end

		-- skip regions shorter than a bar
		if beatcount < 8 then
			pdialog:done ()
			goto next
		end

		-- now stretch the region
		rb:set_strech_and_pitch (1, 1)
		rb:set_mapping (beat_map)

		local nar = rb:process (rb_progress)

		if pdialog:canceled () then goto out end

		-- hide modal progress dialog and destroy it
		pdialog:done ()
		pdialog = nil

		-- replace region
		if not nar:isnil () then
			print ("new audio region: ", nar:name (), nar:length ())
			local track = find_track_for_region (r:to_stateful ():id ())
			local playlist = track:playlist ()
			playlist:to_stateful ():clear_changes () -- prepare undo
			playlist:remove_region (r)
			playlist:add_region (nar, r:position (), 1, false, 0, 0, false)
			-- create a diff of the performed work, add it to the session's undo stack
			-- and check if it is not empty
			if not Session:add_stateful_diff_command (playlist:to_statefuldestructible ()):empty () then
				add_undo = true
			end
		end

		::next::
	end

	::out::

	-- all done, commit the combined Undo Operation
	if add_undo then
		-- the 'nil' Command here mean to use the collected diffs added above
		Session:commit_reversible_command (nil)
	else
		Session:abort_reversible_command ()
	end

	vamp = nil
	collectgarbage ()
end end
