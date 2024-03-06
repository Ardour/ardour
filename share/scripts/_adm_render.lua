ardour {
	["type"]    = "EditorAction",
	name        = "Setup ADM Render",
	author      = "Ardour Team",
	description = [[Set playback alignment to 512 samples to live render ADM]]
}

function factory () return function ()
	-- make surround-return announce additional latency to the next 512
	-- cycle boundary (and delay the output accordingly).
	Session:surround_master():surround_return():set_sync_and_align (true)
end end

function icon (params) return function (ctx, width, height, fg)
	local wh = math.min (width, height)
	ctx:set_line_width (1.5)
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	ctx:arc (0.5 * width - wh * 0.3, 0.5 * height, wh * .275, -0.5 * math.pi , 0.5 * math.pi)
	ctx:close_path ()
	ctx:stroke ()
	ctx:arc (0.5 * width + wh * 0.3, 0.5 * height, wh * .275, 0.5 * math.pi , 1.5 * math.pi)
	ctx:close_path ()
	ctx:stroke ()
end end
