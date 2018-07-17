ardour {
	["type"] = "EditorAction",
	name = "Reset Mixer",
	author = "Ben Loftis, Nikolaus Gullotta",
	description = [[Resets key Mixer settings after user-prompt (warning: this cannot be undone)]]
}

function factory() return function()

	local dlg = {
		{ type = "label", align ="left", colspan="3", title = "Select the items to reset:" },
		{ type = "checkbox", key = "fader", default = true,  title = "Fader" },
		{ type = "checkbox", key = "mute",  default = true,  title = "Mute" },
		{ type = "checkbox", key = "trim",  default = true,  title = "Trim + Phase" },
		{ type = "checkbox", key = "plug",  default = true,  title = "Plug-ins" },
		{ type = "checkbox", key = "dest",  default = false, title = "Remove plug-ins instead of bypassing?" },
		{ type = "label", colspan="3", title = "" },
		{ type = "label", colspan="3", title = "Note that this is a script which can be user-edited to match your needs." },
		{ type = "label", colspan="3", title = "" },
	}

	local pref = LuaDialog.Dialog("Reset Mixer", dlg):run()
	if not(pref) then goto end_script end
    assert(pref, 'Dialog box was cancelled or is ' .. type(pref))

	Session:cancel_all_solo()
	-- loop over all tracks
	for t in Session:get_routes():iter() do
		if not t:is_monitor() and not t:is_auditioner() then
			--zero the fader and input trim
			if pref["fader"] then t:gain_control():set_value(1, 1) end
			if pref["trim"]  then
				t:trim_control():set_value(1, 1)
				t:phase_control():set_value(0, 1)
			end
			if pref["mute"]  then t:mute_control():set_value(0, 1) end
			if not(t:pan_azimuth_control():isnil()) then
				if pref["pan"] then
					t:pan_azimuth_control():set_value(0.5, 1)
				end
			end

			i = 0
			local proc = t:nth_processor (i)
			local queue = {}

			local protected = {
			"recorder", "latcomp-", "player",
			"Polarity", "Trim", "Fader",
			"meter-", "main outs", "Monitor",
			}

			repeat
				local name = proc:display_name()
				--check if processor is foreign to us
				protected_proc = false
				for _, v in pairs(protected) do
					if string.find(name, v) then
						--processor is not foreign to us
						protected_proc = true
					end
				end

				if not(protected_proc) and proc:display_to_user() then
					print(name)
					queue[#queue + 1] = proc
				end

				i = i + 1
				proc = t:nth_processor(i)
			until proc:isnil()

			for p = 1, #queue do
				if pref['plug'] then
					if not(pref["dest"]) then
						queue[p]:deactivate()
					else
						t:remove_processor(queue[p], nil, true)
					end
				end
			end
		end
	end
	::end_script::
end end
