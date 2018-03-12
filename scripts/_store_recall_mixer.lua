ardour {
    ["type"] = "EditorAction",
    name = "Mixer Store",
    author = "Mixbus Lua Taskforce",
    description = [[]]
}

function factory() return function()

    local path = ARDOUR.LuaAPI.build_filename(Session:path(), "export", "params.lua")
	function mark()
		local file = io.open(path, "w")
		file:write("") --empty curent file from last run
		file:close()
		for r in Session:get_routes():iter() do
			if r:is_monitor () or r:is_auditioner () then goto nextroute end -- skip special routes
            local route_str = ""
            local rid = r:to_stateful():id():to_s()
            local pan = r:pan_azimuth_control()
            if pan:isnil() then pan = false else pan = pan:get_value() end --sometimes a route doesn't have pan, like the master.
            --print(r:gain_control():get_value(), r:trim_control():get_value(), pan)
            route_str = "instance = {route_id = " .. rid .. ", gain_control = " .. r:gain_control():get_value() .. ", trim_control = " .. r:trim_control():get_value() .. ", pan_control = " .. tostring(pan) .. "}"
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
							local _, _, pd = ARDOUR.LuaAPI.plugin_automation (proc, n)
							local val = ARDOUR.LuaAPI.get_processor_param(proc, j, true)
                            if not(val == pd.normal) then
                                params[n] = val
                            end
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
		local file = io.open(ARDOUR.LuaAPI.build_filename (Session:path(), "export", "params.lua"), "r")
        assert(file, "File not found!")
		for l in file:lines() do
            print(l)
            local plugin, route = false
			local f = load(l)
			f ()

            if instance["route_id"]  ~= nil then route = true end
            if instance["plugin_id"] ~= nil then plugin = true end

            if route then
                local rid = PBD.ID(instance["route_id"])
                local rt = Session:route_by_id(rid)
                if rt:isnil() then goto next end
                local gc, tc, pc, act = instance["gain_control"], instance["trim_control"], instance["pan_control"], instance["active"]
                rt:gain_control():set_value(gc, 1)
                rt:trim_control():set_value(tc, 1)
                if pc ~= false then rt:pan_azimuth_control():set_value(pc, 1) end
            end

            if plugin then
                local id = PBD.ID(instance["plugin_id"])
                local proc = Session:processor_by_id(id)
                if proc:isnil() then goto next end
                for k, v in pairs(instance["parameters"]) do
                    ARDOUR.LuaAPI.set_processor_param (proc, k, v)
                end
            end
		    instance = nil
            ::next::
		end
	end

	local dialog_options = {
        { type = "label", colspan= 10, title = "" },
        {type = "radio",  colspan= 10, key = "select", title = "", values ={ ["1. Mark"] = "mark", ["2. Recall"] = "recall" }, default = "1. Mark"},
        { type = "label", colspan= 10, title = "" },
    }

	local rv = LuaDialog.Dialog("Mixer Store:", dialog_options):run()
    assert(rv, 'Dialog box was cancelled or is ' .. type(rv))
    local c = rv["select"]
    if c == "mark" then mark() end
    if c == "recall" then recall() end

end end
