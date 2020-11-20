ardour { ["type"] = "Snippet", name = "Editor Selection" }

function factory () return function ()
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	-- the Ardour Selection can include multiple items
	-- (regions, tracks, ranges, markers, automation, midi-notes etc)
	local sel = Editor:get_selection ()

	--
	-- At the point of writing the following data items are available
	--
	
	-- Range selection, total span of all ranges (0, 0 if no time range is selected)
	if sel.time:start () < sel.time:end_sample () then
		print ("Total Range:", sel.time:start (), sel.time:end_sample ())
	end

	-- Range selection, individual ranges.
	for ar in sel.time:iter () do
		-- each of the items is a
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:TimelineRange
		print ("Range:", ar.id, ar.start_time, ar._end_time)
	end

	-- Track/Bus Selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:TrackSelection
	for r in sel.tracks:routelist ():iter () do
		-- each of the items is a
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Route
		print ("Route:", r:name ())
	end

	-- Region selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:RegionSelection
	for r in sel.regions:regionlist ():iter () do
		-- each of the items is a
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Region
		print ("Region:", r:name ())
	end

	-- Markers
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:MarkerSelection
	-- Note: Marker selection is not cleared and currently (Ardour-4.7) points
	--       to the most recently selected marker.
	for m in sel.markers:iter () do
		-- each of the items is a
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOURUI::ArdourMarker
		print ("Marker:", m:name (), m:position(), m:_type())
	end

	----------------------------------------------------------
	-- The total time extents of all selected regions and ranges
	local ok, ext = Editor:get_selection_extents (0, 0)
	if ok then
		print ("Selection Extents:", ext[1], ext[2])
	else
		print ("No region or range is selected")
	end

end end
