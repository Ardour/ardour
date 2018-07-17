ardour {
	["type"] = "EditorAction",
	name = "Mixer Store",
	author = "Ardour Lua Taskforce",
	description = [[Stores the current Mixer state as a file that can be read and recalled arbitrarily.
	Supports: processor settings, grouping, mute, solo, gain, trim, pan and processor ordering, plus re-adding certain deleted plugins.]]
}

function factory() return function()

	local invalidate = {}
	local path = ARDOUR.LuaAPI.build_filename(Session:path(), "export", "params.lua")

	function get_processor_by_name(track, name)
		local i = 0
		local proc = track:nth_processor(i)
			repeat
				if ( proc:display_name() == name ) then
					return proc
				else
					i = i + 1
				end
				proc = track:nth_processor(i)
			until proc:isnil()
		end

	function new_plugin(name)
		for x = 0, 6 do
			plugin = ARDOUR.LuaAPI.new_plugin(Session, name, x, "")
			if not(plugin:isnil()) then return plugin end
		end
	end

	function group_by_id(id)
		local id  = tonumber(id)
		for g in Session:route_groups():iter() do
			local group_id = tonumber(g:to_stateful():id():to_s())
			if group_id == id then return g end
		end
	end

	function route_groupid_interrogate(t)
		local group = false
		for g in Session:route_groups():iter() do
			for r in g:route_list():iter() do
				if r:name() == t:name() then group = g:to_stateful():id():to_s() end
			end
		end return group
	end

	function route_group_interrogate(t)
		for g in Session:route_groups():iter() do
			for r in g:route_list():iter() do
				if r:name() == t:name() then return g end
			end
		end
	end

	function empty_last_store()  --empty current file from last run
		local file = io.open(path, "w")
		file:write("")
		file:close()
	end

	function mark_selected_tracks()
		empty_last_store()

		local sel = Editor:get_selection ()
		local groups_to_write = {}
		local i = 0

		for r in sel.tracks:routelist():iter() do
			local group = route_group_interrogate(r)
			if group then groups_to_write[#groups_to_write + 1] = group end
		end

		for k, g in pairs(groups_to_write) do
			local g_route_str, group_str = "", ""
			group_str = "instance = {group_id = " .. g:to_stateful():id():to_s() .. ", name = " .. "\"" .. g:name() .. "\"" .. ", routes = {"
			for t in g:route_list():iter() do
				g_route_str = g_route_str .."[".. i .."] = " .. t:to_stateful():id():to_s() .. ","
				i = i + 1
			end
			group_str = group_str .. g_route_str .. "}}"
			if not(group_str == "") then --sometimes there are no groups in the session
				file = io.open(path, "a")
				file:write(group_str, "\r\n")
				file:close()
			end
		end

		for r in sel.tracks:routelist():iter() do
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

			route_str = "instance = {route_id = " .. rid .. ", route_name = " .. r:name() .. ", gain_control = " .. r:gain_control():get_value() .. ", trim_control = " .. r:trim_control():get_value() .. ", pan_control = " .. tostring(pan) .. ", muted = " .. tostring(r:muted()) .. ", soloed = " .. tostring(r:soloed()) .. ", order = {" .. proc_order_str .."}, cache = {" .. cache_str .. "}, group = " .. tostring(route_groupid_interrogate(r))  .. "}"
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

	function mark_all_tracks()
		empty_last_store()

		local i = 0
		for g in Session:route_groups():iter() do --@ToDo: Color, and other bools
			local g_route_str, group_str = "", ""
			group_str = "instance = {group_id = " .. g:to_stateful():id():to_s() .. ", name = " .. "\"" .. g:name() .. "\"" .. ", routes = {"
			for t in g:route_list():iter() do
				g_route_str = g_route_str .."[".. i .."] = " .. t:to_stateful():id():to_s() .. ","
				i = i + 1
			end
			group_str = group_str .. g_route_str .. "}}"
			if not(group_str == "") then --sometimes there are no groups in the session
				file = io.open(path, "a")
				file:write(group_str, "\r\n")
				file:close()
			end
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

			route_str = "instance = {route_id = " .. rid .. ", route_name = '" .. r:name() .. "', gain_control = " .. r:gain_control():get_value() .. ", trim_control = " .. r:trim_control():get_value() .. ", pan_control = " .. tostring(pan) .. ", muted = " .. tostring(r:muted()) .. ", soloed = " .. tostring(r:soloed()) .. ", order = {" .. proc_order_str .."}, cache = {" .. cache_str .. "}, group = " .. tostring(route_groupid_interrogate(r))  .. "}"
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

	function recall()
		local file = io.open(path, "r")
		assert(file, "File not found!")
		for l in file:lines() do
			--print(l)

			local plugin, route, group = false, false, false
			local f = load(l)
			f ()

			if instance["route_id"]  then route = true end
			if instance["plugin_id"] then plugin = true end
			if instance["group_id"]  then group = true end

			if group then
				local g_id   = instance["group_id"]
				local routes = instance["routes"]
				local name   = instance["name"]
				local group  = group_by_id(g_id)
				if not(group) then group = Session:new_route_group(name) end
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
				local name  = instance["route_name"]
				local gc, tc, pc = instance["gain_control"], instance["trim_control"], instance["pan_control"]

				local rt = Session:route_by_id(r_id)
				if rt:isnil() then rt = Session:route_by_name(name) end
				if rt:isnil() then goto nextline end

				local cur_group_id = route_groupid_interrogate(rt)
				if not(group) and (cur_group_id) then
					local g = group_by_id(cur_group_id)
					if g then g:remove(rt) end
				end

				well_known = {'PRE', 'Trim', 'EQ', 'Comp', 'Fader', 'POST'}

				for k, v in pairs(order) do
					local proc = Session:processor_by_id(PBD.ID(v))
					if proc:isnil() then
						for id, name in pairs(cache) do
							if v == id then
								proc = new_plugin(name)
								for _, control in pairs(well_known) do
									if name == control then
										proc = get_processor_by_name(rt, control)
										invalidate[v] = proc:to_stateful():id():to_s()
										goto nextproc
									end
								end
								if not(proc) then goto nextproc end
								if not(proc:isnil()) then
									rt:add_processor_by_index(proc, 0, nil, true)
									invalidate[v] = proc:to_stateful():id():to_s()
								end
							end
						end
					end
					::nextproc::
					if proc and not(proc:isnil()) then old_order:push_back(proc) end
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
				local act    = instance["active"]

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
		{ type = "label", colspan = 5, title = "" },
		{ type = "radio", col = 1, colspan = 7, key = "select", title = "", values ={ ["Store"] = "store", ["Recall"] = "recall" }, default = "Store"},
		{ type = "label", colspan = 5, title = "" },
	}

	local store_options = {
		{ type = "label", colspan = 5, title = "" },
		{ type = "checkbox", col=1, colspan = 1, key = "selected", default = false, title = "Selected tracks only"},
		{ type = "entry", col=2, colspan = 10, key = "filename", default = "params", title = "Store name" },
		{ type = "label", colspan = 5, title = "" },
	}

	local recall_options = {
		{ type = "label", colspan = 5, title = "" },
		{ type = "file", col =1, colspan = 10, key = "file", title = "Select a File",  path = ARDOUR.LuaAPI.build_filename(Session:path(), "export", "params.lua") },
		{ type = "label", colspan = 5, title = "" },
	}

	local rv = LuaDialog.Dialog("Mixer Store:", dialog_options):run()

	if rv then
		local choice = rv["select"]
		if choice == "store" then
			local srv = LuaDialog.Dialog("Mixer Store:", store_options):run()
			if srv then
				empty_last_store() --ensures that params.lua will exist for the recall dialog
				path = ARDOUR.LuaAPI.build_filename(Session:path(), "export", srv["filename"] .. ".lua")
				if srv['selected'] then
					mark_selected_tracks()
				else
					mark_all_tracks()
				end
			end
		end

		if choice == "recall" then
			local rrv = LuaDialog.Dialog("Mixer Store:", recall_options):run()
			if rrv then
				if rrv['file'] ~= path then path = rrv['file'] end
				recall()
			end
		end
	end
collectgarbage()
end end
