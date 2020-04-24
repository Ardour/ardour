ardour {
	["type"]    = "EditorAction",
	name        = "Region Select/2",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Select every 2nd region on all selected tracks]]
}

-- select every 2nd region on all selected tracks
function factory () return function ()

	local sl = ArdourUI.SelectionList () -- empty selection list

	local sel = Editor:get_selection () -- get current selection
	-- for each selected track/bus..
	for route in sel.tracks:routelist ():iter () do
		-- consider only tracks
		local track = route:to_track ()
		if track:isnil() then
			goto continue
		end

		local skip = false;
		-- iterate over all regions of the given track
		for region in track:playlist():region_list():iter() do
			if skip then
				-- skip every 2nd region
				skip = false;
			else
				skip = true;
				-- get RegionView (GUI object to be selected)
				local rv = Editor:regionview_from_region (region)
				-- add it to the list of Objects to be selected
				sl:push_back (rv);
			end
		end
		::continue::
	end

	-- set/replace current selection in the editor
	Editor:set_selection (sl, ArdourUI.SelectionOp.Set);
end end

function icon (params) return function (ctx, width, height, fg)
	local wh = math.min (width, height) * .5
	ctx:translate (math.floor (width * .5 - wh), math.floor (height * .5 - wh))

	ctx:set_line_width (1)
	ctx:rectangle (wh * .25, wh * .75, wh * 1.5 , .5 * wh)
	ctx:set_source_rgba (0, 0, 0, 1)
	ctx:stroke_preserve ()
	ctx:set_source_rgba (.9, .9, .9, 1)
	ctx:fill ()

	ctx:set_source_rgba (1, 0, 0, 1)
	ctx:rectangle (.5 + math.ceil(wh * 0.25), .5 + math.ceil(wh * .75), math.floor(wh * .5) - 1, math.floor(.5 * wh) - 1)
	ctx:stroke_preserve ()

	ctx:rectangle (.5 + math.ceil(wh * 1.25), .5 + math.ceil(wh * .75), math.floor(wh * .5) - 1, math.floor(.5 * wh) - 1)
	ctx:stroke_preserve ()
end end
