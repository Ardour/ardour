ardour { ["type"] = "EditorAction", name = "List Plugins",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[List and count plugins used in this session]]
}

function factory () return function ()
	local rv = "Plugins used in this session:\n<span face=\"mono\">CNT | TYPE | NAME</span>"
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
			if all_plugs[id] then cnt = all_plugs[id]['cnt'] end
			all_plugs[id] = { name = proc:name(), ["type"] = pi:type(), id = pp:unique_id(), cnt = (cnt + 1) }
			i = i + 1
		end
		::nextroute::
	end

	function plugintypestr (t)
		if (t == ARDOUR.PluginType.LADSPA) then  return "LADSPA" end
		if (t == ARDOUR.PluginType.LV2) then return "LV2" end
		if (t == ARDOUR.PluginType.AudioUnit) then return "AU" end
		if (t == ARDOUR.PluginType.Windows_VST) then return "VST" end
		if (t == ARDOUR.PluginType.LXVST) then return "VST" end
		if (t == ARDOUR.PluginType.MacVST) then return "VST" end
		if (t == ARDOUR.PluginType.Lua) then return "Lua" end
		return "??"
	end

	for k,v in pairs (all_plugs) do
		print (string.format ("%2d * %-6s %-30s (%s)", v['cnt'], plugintypestr(v['type']), v['name'], v['id']))
		rv = rv .. "\n" .. string.format ("%2d * %-6s %s", v['cnt'], plugintypestr(v['type']), v['name'])
	end

	LuaDialog.Message ("All Plugins", "<span face=\"mono\">" .. rv .. "</span>", LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run()
end end
