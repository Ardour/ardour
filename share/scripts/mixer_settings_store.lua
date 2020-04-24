ardour {
	["type"]    = "EditorAction",
	name        = "Store Mixer Settings",
	author      = "Mixbus Team",
	description = [[
	Stores the current Mixer state as a file
	that can be read and recalled arbitrarily
	by it's companion script, Recall Mixer Settings.

	Supports: processor settings, grouping,
	mute, solo, gain, trim, pan and processor ordering,
	plus re-adding certain deleted plugins.
	]]
}

function factory () return function ()

	local user_cfg = ARDOUR.user_config_directory(-1)
	local local_path = ARDOUR.LuaAPI.build_filename(Session:path(), 'mixer_settings')
	local global_path = ARDOUR.LuaAPI.build_filename(user_cfg, 'mixer_settings')

	function exists(file)
		local ok, err, code = os.rename(file, file)
		if not ok then
			if code == 13 then -- Permission denied, but it exists
				return true
			end
		end return ok, err
	end

	function whoami()
		if not pcall(function() local first_check = Session:get_mixbus(0) end) then
			return "Ardour"
		else
			local second_check = Session:get_mixbus(11)
			if second_check:isnil() then
				return "Mixbus"
			else
				return "32C"
			end
		end
	end

	function isdir(path)
		return exists(path.."/")
	end

	function setup_paths()
		local global_ok, local_ok = false, false

		if not(isdir(global_path)) then
			global_ok, _, _ = os.execute('mkdir '.. global_path)
			if global_ok == 0 then
				global_ok = true
			end
		else
			global_ok = true
		end
		if not(isdir(local_path)) then
			local_ok, _, _ = os.execute('mkdir '.. local_path)
			if local_ok == 0 then
				local_ok = true
			end
		else
			local_ok = true
		end
		return global_ok, local_ok
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

	function empty_last_store(path)  --empty current file from last run
		local file = io.open(path, "w")
		--file:write(string.format("instance = { whoami = '%s' }", whoami())
		file:write("")
		file:close()
	end

	function mark_tracks(selected, path)

		empty_last_store(path)

		local route_string = [[instance = {
			route_id = %d,
			route_name = '%s',
			gain_control = %s,
			trim_control = %s,
			pan_control = %s,
			sends = {%s},
			muted = %s,
			soloed = %s,
			order = {%s},
			cache = {%s},
			group = %s,
			group_name = '%s',
			pi_order = %d
		}]]

		local group_string = [[instance = {
			group_id = %s,
			name = '%s',
			routes = {%s},
		}]]

		local processor_string = [[instance = {
			plugin_id = %d,
			type = %d,
			display_name = '%s',
			owned_by_route_name = '%s',
			owned_by_route_id = %d,
			parameters = {%s},
			active = %s,
		}]]

		local group_route_string = " [%d] = %s,"
		local proc_order_string  = " [%d] = %d,"
		local proc_cache_string  = " [%d] = {'%s', %d},"
		local params_string      = " [%d] = %s,"

		--ensure easy-to-read formatting doesn't make it through
		local route_string     = string.gsub(route_string, "[\n\t%s]", "")
		local group_string     = string.gsub(group_string, "[\n\t%s]", "")
		local processor_string = string.gsub(processor_string, "[\n\t%s]", "")

		local sel = Editor:get_selection ()
		local groups_to_write = {}
		local i = 0

		local tracks = Session:get_stripables()

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
			if r:is_monitor () or r:is_auditioner () or not(r:to_vca():isnil()) then goto nextroute end -- skip special routes
			r = r:to_route()
			if r:isnil() then goto nextroute end
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

			-- Get send information, if any.
			local send_string = ""
			local i = 0
			repeat
				local fmt = "{%s, %s, %s, %s}"
				string.gsub(fmt, "[\n\t]", "")
				local values = {}
				for j, ctrl in pairs({
					r:send_level_controllable(i),
					r:send_enable_controllable(i),
					r:send_pan_azimuth_controllable(i),
					r:send_pan_azimuth_enable_controllable(i),
				}) do
					if not(ctrl:isnil()) then
						values[#values + 1] = ctrl:get_value()
					else
						values[#values + 1] = "nil"
					end
				end
				send_string = send_string .. string.format(fmt, table.unpack(values))
				send_string = send_string .. ","
				i = i + 1
			until r:send_enable_controllable(i):isnil()

			print(send_string)

			local order_nmbr = 0
			local tmp_order_str, tmp_cache_str = "", ""
			for p in order:iter() do
				local ptype
				if not(p:to_insert():isnil()) then
					ptype = p:to_insert():plugin(0):get_info().type
				else
					ptype = 99
				end
				local pid = p:to_stateful():id():to_s()
				if not(string.find(p:display_name(), "latcomp")) then
					tmp_order_str = tmp_order_str .. string.format(proc_order_string, order_nmbr, pid)
					tmp_cache_str = tmp_cache_str .. string.format(proc_cache_string, pid, p:display_name(), ptype)
				end
				order_nmbr = order_nmbr + 1
			end

			local route_str = string.format(
					route_string,
					rid,
					r:name(),
					ARDOUR.LuaAPI.ascii_dtostr(r:gain_control():get_value()),
					ARDOUR.LuaAPI.ascii_dtostr(r:trim_control():get_value()),
					tostring(pan),
					send_string,
					r:muted(),
					r:soloed(),
					tmp_order_str,
					tmp_cache_str,
					route_groupid_interrogate(r),
					route_group,
					r:presentation_info_ptr():order()
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
				local ptype = proc:to_insert():plugin(0):get_info().type

				local n = 0
				for j = 0, plug:parameter_count() - 1 do -- Iterate over all plugin parameters
					if plug:parameter_is_control(j) then
						local label = plug:parameter_label(j)
						if plug:parameter_is_input(j) and label ~= "hidden" and label:sub(1,1) ~= "#" then
							local _, _, pd = ARDOUR.LuaAPI.plugin_automation(proc, n)
							local val = ARDOUR.LuaAPI.get_processor_param(proc, n, true)

							-- Clamp values at plugin max and min
							if val < pd.lower then
								val = pd.lower
							end

							if val > pd.upper then
								val = pd.upper
							end

							print(r:name(), "->", proc:display_name(), "(#".. n ..")",  label, val)
							params[n] = val
						end
						n = n + 1
					end
				end
				i = i + 1

				local tmp_params_str = ""
				for k, v in pairs(params) do
					tmp_params_str = tmp_params_str .. string.format(params_string, k, ARDOUR.LuaAPI.ascii_dtostr(v))
				end

				local proc_str = string.format(
						processor_string,
						id,
						ptype,
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

	local store_options = {
		{ type = "label",    col=0, colspan=1, align="right", title = "Name:" },
		{ type = "entry",    col=1, colspan=1, align="left" , key = "filename", default = Session:name(), title=""},
		{ type = "label",    col=0, colspan=1, align="right", title = "Location:" },
		{
			type = "radio",  col=1, colspan=3, align="left", key = "store-dir", title = "", values =
			{
				['Global (accessible from any session)'] = 1, ['Local (this session only)'] = 2
			},
			default = 'Locally (this session only)'
		},
		{ type = "hseparator", title="", col=0, colspan = 3},
		{ type = "label",    col=0, colspan=1, align="right", title = "Selected Tracks Only:" },
		{ type = "checkbox", col=1, colspan=1, align="left",  key = "selected", default = false, title = ""},
		--{ type = "label", col=0, colspan=2, align="left", title = ''},
		--{ type = "label", col=0, colspan=2, align="left", title = "Global Path: " .. global_path},
		--{ type = "label", col=0, colspan=2, align="left", title = "Local Path: "  .. local_path},
	}

	local global_ok, local_ok = setup_paths()

	if global_ok and local_ok then
		local rv = LuaDialog.Dialog("Store Mixer Settings:", store_options):run()

		if not(rv) then return end

		local filename = rv['filename']
		if rv['store-dir'] == 1 then
			local store_path = ARDOUR.LuaAPI.build_filename(global_path, string.format("%s-%s.lua", filename, whoami()))
			local selected = rv['selected']
			mark_tracks(selected, store_path)
		end

		if rv['store-dir'] == 2 then
			local store_path = ARDOUR.LuaAPI.build_filename(local_path, string.format("%s-%s.lua", filename, whoami()))
			print(store_path)
			local selected = rv['selected']
			mark_tracks(selected, store_path)
		end
	end

end end
