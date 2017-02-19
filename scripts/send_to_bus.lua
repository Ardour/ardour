ardour { ["type"] = "EditorAction", name = "Send Tracks to Bus",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Create a Bus and add aux-sends from all selected tracks]]
}

function factory () return function ()
	-- find number of channels to use for the new bus, follow master-bus' inputs
	local chn = 2
	local mst = Session:master_out ();
	if not mst:isnil () then
		chn = mst:n_inputs ():n_audio ()
	end
	mst = nil -- explicitly drop reference

	local sel = Editor:get_selection () -- get selection
	local tracks = ARDOUR.RouteListPtr () -- create a new list

	-- find selected *tracks*, add to tracks list
	for r in sel.tracks:routelist ():iter () do
		if not r:to_track ():isnil () then
			tracks:push_back (r)
		end
	end

	if tracks:size () > 0 then
		local bus = Session:new_audio_route (chn, chn, nil, 1, "", ARDOUR.PresentationInfo.Flag.AudioBus, ARDOUR.PresentationInfo.max_order)
		if bus:size () > 0 then
			Session:add_internal_sends (bus:front (), ARDOUR.Placement.PostFader, tracks);
		end
	end
end end

function icon (params) return function (ctx, width, height, fg)
	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil (math.min (width, height) * .5) .. "px")
	txt:set_text ("\u{2192}B") -- "->B"
	local tw, th = txt:get_pixel_size ()
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	txt:show_in_cairo_context (ctx)
end end
