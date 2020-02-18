ardour {
	["type"] = "EditorAction",
	name = "Plot Process Graph",
	author = "Ardour Team",
	description = [[Export process graph to a graphviz file, and launch xdot]]
}

function factory () return function ()
	if Session:plot_process_graph ("/tmp/ardour_graph.gv") then
		os.forkexec ("/bin/sh", "-c", "xdot /tmp/ardour_graph.gv")
	end
end end

function icon (params) return function (ctx, width, height, fg)
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	local txt = Cairo.PangoLayout (ctx, "Sans ".. math.ceil(height / 3) .. "px")
	txt:set_alignment (Cairo.Alignment.Center);
	txt:set_width (width);
	txt:set_ellipsize (Cairo.EllipsizeMode.Middle);
	txt:set_text ("plot\ngrph")
	local tw, th = txt:get_pixel_size ()
	ctx:move_to (0, .5 * (height - th))
	txt:show_in_cairo_context (ctx)
end end
