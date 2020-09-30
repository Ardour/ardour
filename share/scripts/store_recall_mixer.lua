ardour {
	["type"] = "EditorAction",
	name = "Mixer Store",
	author = "Ardour Team",
	description = [[
	Stores the current Mixer state as a file
	that can be read and recalled arbitrarily.
	Supports: processor settings, grouping,
	mute, solo, gain, trim, pan and processor ordering,
	plus re-adding certain deleted plugins.
	]]
}

function factory() return function()

	local invalidate = {}
	local path = ARDOUR.LuaAPI.build_filename(Session:path(), "export", "params.lua")

	function mismatch_dialog(mismatch_str, checkbox_str)
		--string.format("Track didn't match ID: %d, but did match track in session: %s", 999, 'track')
		local dialog = {
			{ type = "label", colspan = 5, title = mismatch_str },
			{ type = "checkbox", col=1, colspan = 1, key = "use", default = true, title = checkbox_str },
		}
		local mismatch_return = LuaDialog.Dialog("", dialog):run()
		if mismatch_return then
			return mismatch_return['use']
		else
			return false
		end
	end

	function get_processor_by_name(track, name)
		local i = 0
		local proc = track:nth_processor(i)
			repeat
				if(proc:display_name() == name) then
					return proc
				else
					i = i + 1
				end
				proc = track:nth_processor(i)
			until proc:isnil()
		end

	function new_plugin(name)
		for x = 0, 6 do
			local plugin = ARDOUR.LuaAPI.new_plugin(Session, name, x, "")
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

	function group_by_name(name)
		for g in Session:route_groups():iter() do
			if g:name() == name then return g end
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

	function mark_tracks(selected)

		empty_last_store()

		local route_string = [[instance = {
			 route_id = %d,
			 route_name = '%s',
			 gain_control = %f,
			 trim_control = %f,
			 pan_control = %s,
			 muted = %s,
			 soloed = %s,
			 order = {%s},
			 cache = {%s},
			 group = %s,
			 group_name = '%s'
		}]]

		local group_string = [[instance = {
			 group_id = %s,
			 name = '%s',
			 routes = {%s},
		}]]

		local processor_string = [[instance = {
			 plugin_id = %d,
			 display_name = '%s',
			 owned_by_route_name = '%s',
			 owned_by_route_id = %d,
			 parameters = {%s},
			 active = %s,
		}]]

		local group_route_string = " [%d] = %s,"
		local proc_order_string  = " [%d] = %d,"
		local proc_cache_string  = " [%d] = '%s',"
		local params_string      = " [%d] = %f,"

		--ensure easy-to-read formatting doesn't make it through
		local route_string     = string.gsub(route_string, "[\n\t]", "")
		local group_string     = string.gsub(group_string, "[\n\t]", "")
		local processor_string = string.gsub(processor_string, "[\n\t]", "")

		local sel = Editor:get_selection ()
		local groups_to_write = {}
		local i = 0

		local tracks = Session:get_routes()

		if selected then tracks = sel.tracks:routelist() end

		for r in tracks:iter() do
			local group = route_group_interrogate(r)
			if group then
				local already_there = false
				for _, v in pairs(groups_to_write) do
					if group == v then
						already_there = true
					end
				end
				if not(already_there) then
					groups_to_write[#groups_to_write + 1] = group
				end
			end
		end

		for _, g in pairs(groups_to_write) do
			local tmp_str = ""
			for t in g:route_list():iter() do
				tmp_str = tmp_str .. string.format(group_route_string, i, t:to_stateful():id():to_s())
				i = i + 1
			end
			local group_str = string.format(
				group_string,
				g:to_stateful():id():to_s(),
				g:name(),
				tmp_str
			)

			file = io.open(path, "a")
			file:write(group_str, "\r\n")
			file:close()
		end

		for r in tracks:iter() do
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

			local route_group = route_group_interrogate(r)
			if route_group then route_group = route_group:name() else route_group = "" end
			local rid = r:to_stateful():id():to_s()
			local pan = r:pan_azimuth_control()
			if pan:isnil() then pan = false else pan = pan:get_value() end --sometimes a route doesn't have pan, like the master.

			local order_nmbr = 0
			local tmp_order_str, tmp_cache_str = "", ""
			for p in order:iter() do
				local pid = p:to_stateful():id():to_s()
				if not(string.find(p:display_name(), "latcomp")) then
					tmp_order_str = tmp_order_str .. string.format(proc_order_string, order_nmbr, pid)
					tmp_cache_str = tmp_cache_str .. string.format(proc_cache_string, pid, p:display_name())
				end
				order_nmbr = order_nmbr + 1
			end

			local route_str = string.format(
					route_string,
					rid,
					r:name(),
					r:gain_control():get_value(),
					r:trim_control():get_value(),
					tostring(pan),
					r:muted(),
					r:soloed(),
					tmp_order_str,
					tmp_cache_str,
					route_groupid_interrogate(r),
					route_group
				)

			file = io.open(path, "a")
			file:write(route_str, "\n")
			file:close()

			local i = 0
			while true do
				local params = {}
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

				local tmp_params_str = ""
				for k, v in pairs(params) do
					tmp_params_str = tmp_params_str .. string.format(params_string, k, v)
				end

				local proc_str = string.format(
						processor_string,
						id,
						proc:display_name(),
						r:name(),
						r:to_stateful():id():to_s(),
						tmp_params_str,
						active
					)
				file = io.open(path, "a")
				file:write(proc_str, "\n")
				file:close()
			end
			::nextroute::
		end
	end

	function recall(debug, dry_run)
		local file = io.open(path, "r")
		assert(file, "File not found!")
		local bypass_routes = {}

		local i = 0
		for l in file:lines() do
			--print(i, l)

			local exec_line = dry_run["dothis-"..i]
			local skip_line = false
			if not(exec_line == nil) and not(exec_line) then
				skip_line = true
			end

			local plugin, route, group = false, false, false
			local f = load(l)

			if debug then
				print(i, string.sub(l, 0, 29), f)
			end

			if f then f() end

			if instance["route_id"]  then route = true end
			if instance["plugin_id"] then plugin = true end
			if instance["group_id"]  then group = true end

			if group then
				if skip_line then goto nextline end

				local g_id   = instance["group_id"]
				local routes = instance["routes"]
				local name   = instance["name"]
				local group  = group_by_id(g_id)
				if not(group) then
					local group = Session:new_route_group(name)
					for _, v in pairs(routes) do
						local rt = Session:route_by_id(PBD.ID(v))
						if rt:isnil() then rt = Session:route_by_name(name) end
						if not(rt:isnil()) then group:add(rt) end
					end
				end
			end

			if route then
				local substitution = tonumber(dry_run["destination-"..i])
				if skip_line or (substitution == 0) then
					bypass_routes[#bypass_routes + 1] = instance["route_id"]
					goto nextline
				end

				local old_order = ARDOUR.ProcessorList()
				local route_id = instance["route_id"]
				local r_id = PBD.ID(instance["route_id"])
				local muted, soloed = instance["muted"], instance["soloed"]
				local order = instance["order"]
				local cache = instance["cache"]
				local group = instance["group"]
				local group_name = instance["group_name"]
				local name  = instance["route_name"]
				local gc, tc, pc = instance["gain_control"], instance["trim_control"], instance["pan_control"]

				if not(substitution == instance["route_id"]) then
					print('SUBSTITUTION FOR: ', name, substitution, Session:route_by_id(PBD.ID(substitution)):name())
					--bypass_routes[#bypass_routes + 1] = route_id
					was_subbed = true
					r_id = PBD.ID(substitution)
				end

				local rt = Session:route_by_id(r_id)
				if rt:isnil() then rt = Session:route_by_name(name) end
				if rt:isnil() then goto nextline end

				local cur_group_id = route_groupid_interrogate(rt)
				if not(group) and (cur_group_id) then
					local g = group_by_id(cur_group_id)
					if g then g:remove(rt) end
				end

				local rt_group = group_by_name(group_name)
				if rt_group then rt_group:add(rt) end

				well_known = {'PRE', 'Trim', 'EQ', 'Comp', 'Fader', 'POST'}

				for k, v in pairs(order) do
					local proc = Session:processor_by_id(PBD.ID(1))
					if not(was_subbed) then
						proc = Session:processor_by_id(PBD.ID(v))
					end
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
				rt:reorder_processors(old_order, nil)
				if muted  then rt:mute_control():set_value(1, 1) else rt:mute_control():set_value(0, 1) end
				if soloed then rt:solo_control():set_value(1, 1) else rt:solo_control():set_value(0, 1) end
				rt:gain_control():set_value(gc, 1)
				rt:trim_control():set_value(tc, 1)
				if pc ~= false then rt:pan_azimuth_control():set_value(pc, 1) end
			end

			if plugin then
				if skip_line then goto nextline end

				--if the plugin is owned by a route
				--we decided not to use, skip it
				for _, v in pairs(bypass_routes) do
					if instance["owned_by_route_id"] == v then
						goto nextline
					end
				end

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
			i = i + 1

		end
	end

	function dry_run(debug)
		--returns a dialog-able table of
		--everything we do (logically)
		--in the recall function
		local route_values = {['----'] = "0"}
		for r in Session:get_routes():iter() do
			route_values[r:name()] =  r:to_stateful():id():to_s()
		end

		local i = 0
		local dry_table = {
			{type = "label", align = "left", key =  "col-0-title" , col = 0, colspan = 1, title = 'Source Settings:'},
			{type = "label", align = "left", key =  "col-0-title" , col = 1, colspan = 1, title = 'Actions:'},
			{type = "label", align = "left", key =  "col-2-title" , col = 2, colspan = 1, title = 'Destination:'},
			{type = "label", align = "left", key =  "col-2-title" , col = 3, colspan = 1, title = 'Do this?'},
		}
		local file = io.open(path, "r")
		assert(file, "File not found!")

		for l in file:lines() do
			local do_plugin, do_route, do_group = false, false, false
			local f = load(l)

			if debug then
				print(i, string.sub(l, 0, 29), f)
			end

			if f then f() end

			if instance["route_id"]  then do_route = true end
			if instance["plugin_id"] then do_plugin = true end
			if instance["group_id"]  then do_group = true end

			if do_group then
				local group_id   = instance["group_id"]
				local group_name = instance["name"]
				local dlg_title, action_title  = "", ""

				local group_ptr  = group_by_id(group_id)

				if not(group_ptr) then
					new_group = Session:new_route_group(group_name)
					dlg_title = string.format("Cannot Find: (Group) %s.", group_name, new_group:name())
					action_title = "will create and use settings"
				else
					dlg_title = string.format("Found by ID: (Group) %s.", group_ptr:name())
					action_title = "will use group settings"
				end
				table.insert(dry_table, {
					type = "label", align = "left", key =  "group-"..i , col = 0, colspan = 1, title = dlg_title
				})
				table.insert(dry_table, {
					type = "label", align = "left", key =  "group-"..i , col = 1, colspan = 1, title = action_title
				})
				table.insert(dry_table, {
					type = "checkbox", col=3, colspan = 1, key = "dothis-"..i, default = true, title = "line:"..i
				})
			end

			if do_route then
				local route_id   = instance["route_id"]
				local route_name = instance["route_name"]
				local dlg_title = ""

				local route_ptr = Session:route_by_id(PBD.ID(route_id))

				if route_ptr:isnil() then
					route_ptr = Session:route_by_name(route_name)
					if not(route_ptr:isnil()) then
						dlg_title = string.format("Found by Name: (Rotue) %s", route_ptr:name())
						action_title = "will use route settings"
					else
						dlg_title = string.format("Cannot Find: (Route) %s", route_name)
						action_title = "will be ignored"
					end
				else
					dlg_title = string.format("Found by ID: (Route) %s", route_ptr:name())
					action_title = "will use route settings"
				end
				if route_ptr:isnil() then name = route_name else name = route_ptr:name() end
				table.insert(dry_table, {
					type = "label", align = "left", key = "route-"..i , col = 0, colspan = 1, title = dlg_title
				})
				table.insert(dry_table, {
					type = "label", align = "left", key = "action-"..i , col = 1, colspan = 1, title = action_title
				})
				table.insert(dry_table, {
					type = "dropdown", align = "left", key = "destination-"..i, col = 2, colspan = 1, title = "", values = route_values, default = name or "----"
				})
				table.insert(dry_table, {
					type = "checkbox", col=3, colspan = 1, key = "dothis-"..i, default = true, title = "line"..i
				})
			end
			i = i + 1
		end
		return dry_table
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
				mark_tracks(srv['selected'])
			end
		end

		if choice == "recall" then
			local rrv = LuaDialog.Dialog("Mixer Store:", recall_options):run()
			if rrv then
				if rrv['file'] ~= path then path = rrv['file'] end
				--recall(true)
				local dry_return = LuaDialog.Dialog("Mixer Store:", dry_run(true)):run()
				if dry_return then recall(true, dry_return) end
			end
		end
	end
end end
