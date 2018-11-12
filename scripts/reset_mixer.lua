ardour {
	["type"] = "EditorAction",
	name = "Reset Mixer",
	author = "Ben Loftis, Nikolaus Gullotta, Maxime Lecoq",
	description = [[Resets key Mixer settings after user-prompt (warning: this cannot be undone)]]
}

function factory() return function()
	
	local sp_radio_buttons = { Unreset="unreset", Bypass="bypass", Remove="remove" }

	local dlg = {
		{ type = "label", align ="left", colspan="3", title = "Please select below the items you want to reset:" },
		{ type = "label", align ="left", colspan="3", title = "(Warning: this cannot be undone!)\n" },

		{ type = "label", align ="left", colspan="3", title = "Levels:" },
		{ type = "checkbox", key = "fader", default = false,  title = "Fader" },
		{ type = "checkbox", key = "mute",  default = false,  title = "Mute" },
		{ type = "checkbox", key = "solo",  default = false,  title = "Solo" },
		{ type = "checkbox", key = "trim",  default = false,  title = "Trim + Phase" },

		{ type = "label", align ="left", colspan="3", title = "\nPan:" },
		{ type = "checkbox", key = "pan",  default = false,  title = "Pan" },
		{ type = "checkbox", key = "panwidth", default = false,  title = "Pan width" },

		{ type = "label", align ="left", colspan="3", title = "\nSignal processors:" },
		{ type = "radio", key = "sends", title = "Sends", values=sp_radio_buttons, default="Unreset" },
		{ type = "radio", key = "inserts", title = "Inserts", values=sp_radio_buttons, default="Unreset" },
		{ type = "radio", key = "plug-ins", title = "Plug-ins", values=sp_radio_buttons, default="Unreset" },

		{ type = "label", align ="left", colspan="3", title = "\nAutomation (switch to manual mode):" },
		{ type = "checkbox", key = "autogain", default = false,  title = "Gain" },
		{ type = "checkbox", key = "autopan", default = false,  title = "Pan" },
		{ type = "checkbox", key = "autopanwidth", default = false,  title = "Pan width" },

		{ type = "label", align ="left", colspan="3", title = "" },
	}

	local pref = LuaDialog.Dialog("Reset Mixer", dlg):run()
	
	if not(pref) then goto pass_script end
    assert(pref, 'Dialog box was cancelled or is ' .. type(pref))
	
	-- Manage signal processors state or removal according
	-- to the user prompt settings and log trace.
	function handle_processor(effect_type_name, track, proc)
		local action_name = pref[effect_type_name]
		local proc_name = proc:display_name()
		local track_name = track:name()
		local proc_handled = false
		
		if(action_name == "bypass") then
			if(proc:active()) then 
				proc:deactivate()
				proc_handled = true
			end
		elseif(action_name == "remove") then
			track:remove_processor(proc, nil, true)
			proc_handled = true
		end
		
		if(proc_handled) then print(action_name, effect_type_name, proc_name, "on track", track_name) end
	end
	
	-- solo
	-- (could be handled in track loop but it's simplier to do it on the session)
	if pref["solo"] then Session:cancel_all_solo() end
	
	-- loop over all tracks
	for t in Session:get_routes():iter() do
		
		if not t:is_monitor() and not t:is_auditioner() then
			
			-- automation first
			if pref["autogain"] then t:gain_control():set_automation_state(ARDOUR.AutoState.Off) end
			if pref["autopan"] then t:pan_azimuth_control():set_automation_state(ARDOUR.AutoState.Off) end
			if pref["autopanwidth"] then 
				local pwc = t:pan_width_control()
				if(not pwc:isnil()) then -- careful stereo track
					pwc:set_automation_state(ARDOUR.AutoState.Off)
				end
			end
			
			-- levels
			if pref["fader"] then t:gain_control():set_value(1, 1) end
			if pref["trim"]  then
				t:trim_control():set_value(1, 1)
				t:phase_control():set_value(0, 1)
			end
			if pref["mute"] then t:mute_control():set_value(0, 1) end
			
			-- pan
			if not(t:pan_azimuth_control():isnil()) then
				if pref["pan"] then t:pan_azimuth_control():set_value(0.5, 1) end
			end
			if not(t:pan_width_control():isnil()) then
				if pref["panwidth"] then t:pan_width_control():set_value(1, 1) end
			end

			-- signal processors management
			i = 0
			local proc = t:nth_processor (i)
			
			-- collect user procs
			repeat -- loop over the track procs

				-- send
				if not(proc:to_ioprocessor():isnil()) then
					--check if processor is a send or insert
					if proc:to_ioprocessor():display_to_user() then
						handle_processor("sends", t, proc)
					end
				end

				-- insert
				if not(proc:to_insert():isnil()) then
					--check if processor is foreign to us
					if not(proc:to_insert():display_to_user()) then
						handle_processor("inserts", t, proc)
					end
				end
				
				-- regular user plug-in
				if not(proc:to_plugininsert():isnil()) then
					handle_processor("plug-ins", t, proc)
				end

				-- prepare the next proc to be inspected
				i = i + 1
				proc = t:nth_processor(i)
				
			until proc:isnil() -- end repeat track procs
			
		end -- if monitor or auditioner
		
	end -- loop over all tracks
	::pass_script::
	collectgarbage()
end end