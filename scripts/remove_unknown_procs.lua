ardour { ["type"] = "EditorAction", name = "Remove Unknown Plugins",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Remove all unknown plugins/processors from all tracks and busses]]
}

function factory (params) return function ()
	-- iterate over all tracks and busses (aka routes)
	for route in Session:get_routes ():iter () do
		-- route is-a http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Route
		local i = 0;
		-- we need to iterate one-by one, removing a processor invalidates the list
		repeat
			proc = route:nth_processor (i)
			-- proc is a http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Processor
			-- try cast it to http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:UnknownProcessor
			if not proc:isnil () and not proc:to_unknownprocessor ():isnil () then
				route:remove_processor (proc, nil, true)
			else
				i = i + 1
			end
		until proc:isnil ()
	end
end end


function icon (params) return function (ctx, width, height, fg)
	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil (math.min (width, height) * .5) .. "px")
	txt:set_text ("Fx")
	local tw, th = txt:get_pixel_size ()
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	txt:layout_cairo_path (ctx)

	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	ctx:set_line_width (3)
	ctx:stroke_preserve ()

	ctx:set_source_rgba (.8, .2, .2, 1)
	ctx:set_line_width (2)
	ctx:stroke_preserve ()

	ctx:set_source_rgba (0, 0, 0, 1)
	ctx:fill ()
end end
