ardour { ["type"] = "EditorAction", name = "List Plugins",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[List and count plugins used in this session]]
}

function factory () return function ()
	local rv = "Plugins used in this session:\n\n"
	local all_plugs = {}

	for r in Session:get_routes ():iter () do
		if r:is_monitor () or r:is_auditioner () then goto nextroute end -- skip special routes
		local i = 0
		while true do
			local proc = r:nth_plugin (i)
			if proc:isnil () then break end
			local pi = proc:to_insert () -- we know it's a plugin-insert (we asked for nth_plugin)
			local pp = pi:plugin (0)
			local id = pi:type() .. "-" .. pp:unique_id()
			local cnt = 0
			local rns = {}
			if pi:is_channelstrip () then goto nextproc end
			if all_plugs[id] then
				cnt = all_plugs[id]['cnt']
				rns = all_plugs[id]['rns']
			end
			rns[#rns+1] = r:name ()
			all_plugs[id] = { name = proc:name(), ["type"] = pi:type(), id = pp:unique_id(), author = pp:get_info().creator, cnt = (cnt + 1), rns = rns }
			::nextproc::
			i = i + 1
		end
		::nextroute::
	end

	function plugintypestr (t)
		return ARDOUR.PluginType.name (t)
	end

	if next(all_plugs) == nil then
		rv = rv .. " -- NONE --"
	else
		rv = rv .. "<span face=\"mono\">CNT | TYPE | NAME</span>"
	end

	for k,v in pairs (all_plugs) do
		print (string.format ("%2d * %-6s %-30s (%s)", v['cnt'], plugintypestr(v['type']), v['name'], v['id']))
		for _,n in ipairs (v['rns']) do
			print ("   -", n)
		end
		rv = rv .. "\n<span face=\"mono\">" .. string.format ("%3d * %-6s %s (by %s)", v['cnt'], plugintypestr(v['type']), v['name'], v['author']) .. "</span>"
	end

	LuaDialog.Message ("All Plugins",rv , LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run()
end end

function icon (params) return function (ctx, width, height, fg)
	local wh = math.min (width, height)
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	ctx:rectangle (wh * .2, wh * .35, wh * .4, wh * .3)
	ctx:fill ()
	ctx:rectangle (wh * .65, wh * .35, wh * .1, wh * .3)
	ctx:fill ()
	ctx:set_line_join (Cairo.LineJoin.Bevel)
	ctx:set_line_width (.5)
	ctx:move_to (wh * 0.85, wh * .35)
	ctx:line_to (wh, wh * .5)
	ctx:line_to (wh * 0.85, wh * .65)
	ctx:close_path ()
	ctx:fill_preserve ()
	ctx:stroke ()

end end
