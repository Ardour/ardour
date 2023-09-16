ardour {  ["type"] = "EditorHook", name = "On Editor Selection Change" }

function signals ()
	-- call script function when editor selection changes
	return LuaSignal.Set():add ({[LuaSignal.SelectionChanged] = true})
end

-- output is printed to Window > Log

function factory () return function ()
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	-- the Ardour Selection can include multiple items
	-- (regions, tracks, ranges, markers, automation, midi-notes etc)
	local sel = Editor:get_selection ()

	--
	-- At the point of writing the following data items are available
	--

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

	-- Time Range selection, total span of all ranges (0, 0 if no time range is selected)
	if sel.time:start_sample () < sel.time:end_sample () then
		print ("Total Range:", sel.time:start_sample (), sel.time:end_sample ())
	end

	-- .. and the same in Temporal.timepos_t
	if sel.time:start_time () < sel.time:end_time () then
		print ("Total Range:", sel.time:start_time (), sel.time:end_time ())
	end

	-- Range selection, individual ranges.
	for ar in sel.time:iter () do
		-- each of the items is a
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:TimelineRange
		print ("Range:", ar.id, ar.start, ar._end)
	end

	-- Markers
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:MarkerSelection
	for m in sel.markers:iter () do
		-- each of the items is a
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOURUI::ArdourMarker
		print ("Marker:", m:name (), m:position(), m:_type())
	end

	----------------------------------------------------------
	-- The total time extents of all selected regions and ranges
	local ok, ext = Editor:get_selection_extents (Temporal.timepos_t(0), Temporal.timepos_t(0))
	if ok then
		print ("Selection Extents:", ext[1], ext[2])
	else
		print ("No region or range is selected")
	end

end end
