ardour { ["type"] = "EditorAction", name = "Search and Jump to Marker",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Jump to the first marker that matches a given name pattern]]
}

function factory () return function ()
	local keep = false

	::restart::

	local dlg = LuaDialog.Dialog ("Search and Jump to Marker",
		{
			{ type = "entry",    key = "marker", default = '',   title = "Marker Prefix" },
			{ type = "checkbox", key = "keep",   default = keep, title = "Keep Dialog Open" },
		})

	local rv = dlg:run()
	if not rv then return end

	keep = rv['keep']

	if (rv['marker'] == "") then
		if keep then goto restart end
		return
	end

	for l in Session:locations():list():iter() do
		if l:is_mark() and string.find (l:name(), "^" .. rv['marker'] .. ".*$") then
			Session:request_locate (l:start (), ARDOUR.LocateTransportDisposition.RollIfAppropriate, ARDOUR.TransportRequestSource.TRS_UI)
			if keep then goto restart end
			return
		end
	end

	LuaDialog.Message ("Jump to Marker", "No marker matching the given pattern was found.", LuaDialog.MessageType.Warning, LuaDialog.ButtonType.Close):run ()

	if keep then goto restart end
end end


function icon (params) return function (ctx, width, height, fg)
	local mh = height - 3.5;
	local m3 = width / 3;
	local m6 = width / 6;

	ctx:set_line_width (.5)

	-- compare to gtk2_ardour/marker.cc "Marker"
	ctx:set_source_rgba (.6, .6, .6, 1.0)
	ctx:move_to (width / 2 - m6, 2)
	ctx:rel_line_to (m3, 0)
	ctx:rel_line_to (0, mh * 0.4)
	ctx:rel_line_to (-m6, mh * 0.6)
	ctx:rel_line_to (-m6, -mh * 0.6)
	ctx:close_path ()
	ctx:fill_preserve ()
	ctx:set_source_rgba (.0, .0, .0, 1.0)
	ctx:stroke ()

	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil(math.min (width, height) * .5) .. "px")
	txt:set_text ("txt")
	local tw, th = txt:get_pixel_size ()
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	txt:show_in_cairo_context (ctx)
end end
