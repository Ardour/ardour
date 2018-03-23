ardour {
	["type"] = "EditorAction",
	name = "Mixer Store",
	author = "Ardour Lua Taskforce",
	description = [[Stores the current Mixer state as a file that can be read and recalled arbitrarily.
	Supports: processor settings, grouping, mute, solo, gain, trim, pan and processor ordering, plus re-adding certain deleted plugins.]]
}

function factory() return function()

	function new_plugin(name)
		local plugin = nil
		for x = 0, 6 do
			plugin = ARDOUR.LuaAPI.new_plugin(Session, name, x, "")
			if not(plugin:isnil()) then break end
		end return plugin
	end

	function group_by_id(id)
		local group = nil
		local id  = tonumber(id)
		for g in Session:route_groups():iter() do
			local group_id = tonumber(g:to_stateful():id():to_s())
			if group_id == id then group = g end
		end return group
	end

	function route_group_interrogate(t)
		local group = false
		for g in Session:route_groups():iter() do
			for r in g:route_list():iter() do
				if r:name() == t:name() then group = g:to_stateful():id():to_s() end
			end
		end return group
	end

	local path = ARDOUR.LuaAPI.build_filename(Session:path(), "export", "params.lua")
	function mark()

		local file = io.open(path, "w")
		file:write("") --empty current file from last run
		file:close()

		local g_route_str, group_str = "", ""
		local i = 0
		for g in Session:route_groups():iter() do --@ToDo: Color, and other bools
			group_str = "instance = {group_id = " .. g:to_stateful():id():to_s() .. ", name = " .. "\"" .. g:name() .. "\"" .. ", routes = {"
			for t in g:route_list():iter() do
				g_route_str = g_route_str .."[".. i .."] = " .. t:to_stateful():id():to_s() .. ","
				i = i + 1
			end
			group_str = group_str .. g_route_str .. "}}"
		end

		if not(group_str == "") then --sometimes there are no groups in the session
			file = io.open(path, "a")
			file:write(group_str, "\r\n")
			file:close()
		end

		for r in Session:get_routes():iter() do
			if r:is_monitor () or r:is_auditioner () then goto nextroute end -- skip special routes

			local order = ARDOUR.ProcessorList()
			local x = 0
			repeat
				local proc = r:nth_processor(x)
				if not proc:isnil() then
					order:push_back(proc)
				end
				x = x + 1
			until proc:isnil()

			local route_str, proc_order_str, cache_str = "", "", ""
			local rid = r:to_stateful():id():to_s()
			local pan = r:pan_azimuth_control()
			if pan:isnil() then pan = false else pan = pan:get_value() end --sometimes a route doesn't have pan, like the master.

			local on = 0
			for p in order:iter() do
				local pid = p:to_stateful():id():to_s()
				if not(string.find(p:display_name(), "latcomp")) then
					proc_order_str = proc_order_str .. "[" .. on .. "] = " .. pid ..","
					cache_str = cache_str .. "[" .. pid .. "] = " .. "\"" .. p:display_name() .. "\"" ..","
				end
				on = on + 1
			end

			route_str = "instance = {route_id = " .. rid .. ", gain_control = " .. r:gain_control():get_value() .. ", trim_control = " .. r:trim_control():get_value() .. ", pan_control = " .. tostring(pan) .. ", muted = " .. tostring(r:muted()) .. ", soloed = " .. tostring(r:soloed()) .. ", order = {" .. proc_order_str .."}, cache = {" .. cache_str .. "}, group = " .. tostring(route_group_interrogate(r))  .. "}"
			file = io.open(path, "a")
			file:write(route_str, "\r\n")
			file:close()

			local i = 0
			while true do
				local params = {}
				local proc_str, params_str = "", ""
				local proc = r:nth_plugin (i)
				if proc:isnil () then break end
				local active = proc:active()
				local id = proc:to_stateful():id():to_s()
				local plug = proc:to_insert ():plugin (0)
				local n = 0 -- count control-ports
				for j = 0, plug:parameter_count () - 1 do -- iterate over all plugin parameters
					if plug:parameter_is_control (j) then
						local label = plug:parameter_label (j)
						if plug:parameter_is_input (j) and label ~= "hidden" and label:sub (1,1) ~= "#" then
							local _, _, pd = ARDOUR.LuaAPI.plugin_automation(proc, n)
							local val = ARDOUR.LuaAPI.get_processor_param(proc, j, true)
							--print(r:name(), "->", proc:display_name(), label, val)
							params[n] = val
						end
						n = n + 1
					end
				end
				i = i + 1
				for k, v in pairs(params) do
					params_str = params_str .. "[".. k .."] = " .. v .. ","
				end
				proc_str = "instance = {plugin_id = " .. id .. ", parameters = {" .. params_str .. "}, active = " .. tostring(active) .. "}"
				file = io.open(path, "a")
				file:write(proc_str, "\r\n")
				file:close()
			end
			::nextroute::
		end
	end
	local invalidate = {}
	function recall()
		local file = io.open(path, "r")
		assert(file, "File not found!")
		for l in file:lines() do
			--print(l)

			local plugin, route, group = false, false, false
			local f = load(l)
			f ()

			if instance["route_id"]  ~= nil then route = true end
			if instance["plugin_id"] ~= nil then plugin = true end
			if instance["group_id"]  ~= nil then group = true end

			if group then
				local g_id = instance["group_id"]
				local routes = instance["routes"]
				local name = instance["name"]
				local group = group_by_id(g_id)
				if group == nil then group = Session:new_route_group(name) end
				for k, v in pairs(routes) do
					local rt = Session:route_by_id(PBD.ID(v))
					if not(rt:isnil()) then group:add(rt) end
				end
			end

			if route then

				local old_order = ARDOUR.ProcessorList()
				local r_id = PBD.ID(instance["route_id"])
				local muted, soloed = instance["muted"], instance["soloed"]
				local order = instance["order"]
				local cache = instance["cache"]
				local group = instance["group"]
				local gc, tc, pc = instance["gain_control"], instance["trim_control"], instance["pan_control"]

				local rt = Session:route_by_id(r_id)
				if rt:isnil() then goto nextline end

				local cur_group_id = route_group_interrogate(rt)
				if not(group) and (cur_group_id) then
					local g = group_by_id(cur_group_id)
					if g then g:remove(rt) end
				end

				for k, v in pairs(order) do
					local proc = Session:processor_by_id(PBD.ID(v))
					if proc:isnil() then
						for id, name in pairs(cache) do
							if v == id then
								proc = new_plugin(name)
								if not(proc:isnil()) then
									rt:add_processor_by_index(proc, 0, nil, true)
									invalidate[v] = proc:to_stateful():id():to_s()
								end
							end
						end
					end
					if not(proc:isnil()) then old_order:push_back(proc) end
				end

				if muted  then rt:mute_control():set_value(1, 1) else rt:mute_control():set_value(0, 1) end
				if soloed then rt:solo_control():set_value(1, 1) else rt:solo_control():set_value(0, 1) end
				rt:gain_control():set_value(gc, 1)
				rt:trim_control():set_value(tc, 1)
				if pc ~= false then rt:pan_azimuth_control():set_value(pc, 1) end
				rt:reorder_processors(old_order, nil)
			end

			if plugin then
				local enable = {}
				local params = instance["parameters"]
				local p_id   = instance["plugin_id"]
				local act = instance["active"]

				for k, v in pairs(invalidate) do --invalidate any deleted plugin's id
					if p_id == k then
						p_id = v
					end
				end

				local proc = Session:processor_by_id(PBD.ID(p_id))
				if proc:isnil() then goto nextline end
				local plug = proc:to_insert():plugin(0)

				for k, v in pairs(params) do
					local label = plug:parameter_label(k)
					if string.find(label, "Assign") or string.find(label, "Enable") then --@ToDo: Check Plugin type == LADSPA or VST?
						enable[k] = v --queue any assignments/enables for after the initial parameter recalling to duck the 'in-on-change' feature
					end
					ARDOUR.LuaAPI.set_processor_param(proc, k, v)
				end

				for k, v in pairs(enable) do
					ARDOUR.LuaAPI.set_processor_param(proc, k, v)
				end
				if act then proc:activate() else proc:deactivate() end
			end
			::nextline::
		end
	end

	local dialog_options = {
		{ type = "label", colspan= 10, title = "" },
		{ type = "radio",  colspan= 10, key = "select", title = "", values ={ ["1. Mark"] = "mark", ["2. Recall"] = "recall" }, default = "1. Mark"},
		{ type = "label", colspan= 10, title = "" },
	}

	local rv = LuaDialog.Dialog("Mixer Store:", dialog_options):run()
	if rv then
		local c = rv["select"]
		if c == "mark" then mark() end
		if c == "recall" then recall() end
	end

end end
