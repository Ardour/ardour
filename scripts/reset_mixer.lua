ardour {
	["type"]    = "EditorAction",
	name        = "Reset Mixer",
	license     = "MIT",
	author      = "Ben Loftis, Nikolaus Gullotta, Maxime Lecoq",
	description = [[Resets key Mixer settings after user-prompt (warning: this cannot be undone)]]
}

function factory() return function()
	local sp_radio_buttons = {Bypass="bypass", Remove="remove", Nothing=false}
	local dlg = {
		{type="label", align="left", colspan="3", title="Please select below the items you want to reset:" },
		{type="label", align="left", colspan="3", title="(Warning: this cannot be undone!)\n" },

		{type="heading", align ="center", colspan="3", title = "Common Controls:" },
		{type="checkbox", key="fader", default=true, title="Fader"      },
		{type="checkbox", key="mute",  default=true, title="Mute"       },
		{type="checkbox", key="solo",  default=true, title="Solo"       },
		{type="checkbox", key="trim",  default=true, title="Trim"       },
		{type="checkbox", key="pan",   default=true, title="Pan (All)"  },
		{type="checkbox", key="phase", default=true, title="Phase"      },
		{type="checkbox", key="sends", default=true, title="Sends"      },
		{type="checkbox", key="eq",    default=true, title="EQ"         },
		{type="checkbox", key="comp",  default=true, title="Compressor" },

		{type="heading", align="center", colspan="3", title="Processors:" },
		{type="radio", key="plugins", title="Plug-ins",      values=sp_radio_buttons, default="Bypass" },
		{type="radio", key="io",      title="Sends/Inserts", values=sp_radio_buttons, default="Bypass" },

		{type="hseparator", title=""},

		{type="heading", align="center", colspan="3", title="Misc." },
		{type="checkbox", key="auto",    colspan="3", title = "Automation (switch to manual mode)" },
		{type="checkbox", key="groups",  colspan="3", title = "Groups"                             },
	}

	function reset(ctrl, disp, auto)
		local disp = disp or PBD.GroupControlDisposition.NoGroup

		if not(ctrl:isnil()) then
			local pd = ctrl:desc()
			ctrl:set_value(pd.normal, disp)

			if auto then
				ctrl:set_automation_state(auto)
			end
		end
	end

	function reset_eq_controls(route, disp, auto)
		if route:isnil() then
			return
		end

		local disp = disp or PBD.GroupControlDisposition.NoGroup

		reset(route:eq_enable_controllable(), disp, auto)

		local i = 0
		repeat
			for _,ctrl in pairs({
				route:eq_freq_controllable(i),
				route:eq_gain_controllable(i),
				route:eq_q_controllable(i),
			}) do
				reset(ctrl, disp, auto)
			end
			i = i + 1
		until route:eq_freq_controllable(i):isnil()
	end

	function reset_comp_controls(route, disp, auto)
		if route:isnil() then
			return
		end

		local disp = disp or PBD.GroupControlDisposition.NoGroup

		for _,ctrl in pairs({
			route:comp_enable_controllable(),
			route:comp_makeup_controllable(),
			route:comp_mode_controllable(),
			route:comp_speed_controllable(),
			route:comp_threshold_controllable(),
		}) do
			reset(ctrl, disp, auto)
		end
	end

	function reset_send_controls(route, disp, auto)
		if route:isnil() then
			return
		end

		local disp = disp or PBD.GroupControlDisposition.NoGroup

		local i = 0
		repeat
			for _,ctrl in pairs({
				route:send_level_controllable(i),
				route:send_enable_controllable(i),
				route:send_pan_azimuth_controllable(i),
				route:send_pan_azimuth_enable_controllable(i),
			}) do
				reset(ctrl, disp, auto)
			end
			i = i + 1
		until route:send_enable_controllable(i):isnil()
	end

	function reset_plugins(route, prefs)
		if route:isnil() then
			return
		end

		local i = 0
		repeat
			local proc = route:nth_processor(i)
			if not(proc:isnil()) then

				if prefs["plugins"] then
					local insert = proc:to_insert()
					if not(insert:isnil()) then
						if insert:is_channelstrip() or not(insert:display_to_user()) then
							ARDOUR.LuaAPI.reset_processor_to_default(insert)
						else
							if prefs["plugins"] == "remove" then
								route:remove_processor(proc, nil, true)
							else
								insert:deactivate()
							end
						end
					end
				end

				if prefs["io"] then
					local io_proc = proc:to_ioprocessor()
					if not(io_proc:isnil()) then
						if prefs["io"] == "remove" then
							route:remove_processor(proc, nil, true)
						else
							io_proc:deactivate()
						end
					end
				end
			end
			i = i + 1
		until proc:isnil()
	end

	local pref = LuaDialog.Dialog("Reset Mixer", dlg):run()

	if not(pref) then goto pass_script end
	assert(pref, "Dialog box was cancelled or is nil")

	for route in Session:get_routes():iter() do
		local disp = PBD.GroupControlDisposition.NoGroup
		local auto = nil

		if pref["auto"] then
			auto = ARDOUR.AutoState.Off
		end

		if pref["eq"]    then reset_eq_controls(route, disp, auto) end
		if pref["comp"]  then reset_comp_controls(route, disp, auto) end
		if pref["sends"] then reset_send_controls(route, disp, auto) end
		reset_plugins(route, pref)

		if pref["fader"] then
			reset(route:gain_control(), disp, auto)
		end

		if pref["phase"] then
			reset(route:phase_control(), disp, auto)
		end

		if pref["trim"] then
			reset(route:trim_control(), disp, auto)
		end

		if pref["mute"] then
			reset(route:mute_control(), disp, auto)
		end

		if pref["solo"] then
			reset(route:solo_control(), disp, auto)
		end

		if pref["pan"] then
			reset(route:pan_azimuth_control(), disp, auto)
			reset(route:pan_elevation_control(), disp, auto)
			reset(route:pan_frontback_control(), disp, auto)
			reset(route:pan_lfe_control(), disp, auto)
			reset(route:pan_width_control(), disp, auto)
		end
	end

	if pref["groups"] then
		for group in Session:route_groups():iter() do
			Session:remove_route_group(group)
		end
	end
	::pass_script::
	collectgarbage()
end end