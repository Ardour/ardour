ardour {
	["type"]    = "EditorAction",
	name        = "Setup ADM Render",
	author      = "Ardour Team",
	description = [[Set playback alignment to 512 samples to live render ADM]]
}

function factory () return function ()
	-- ignore input systemic latency, no additional pre-roll
	-- to fill buffers with input.
	for r in Session:get_tracks():iter() do
		r:input():disconnect_all (nil)
	end

	-- make surround-return announce additional latency to the next 512
	-- cycle boundary (and delay the output accordingly).
	Session:surround_master():surround_return():set_sync_and_align (true)

	-- Mixbus: enforce latency pre-roll to be >= 1505 (latency of Atmos renderer)
	-- this allows the Stem Export the Surround Bus.
	-- (Mixbus tracks are aligned to master-out, not surrround out)
	if Session:master_out().ch_post then
		Session:master_out():ch_post ():to_latent():set_user_latency (1505)
	end

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
