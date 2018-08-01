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
		{ type = "checkbox", key = "sends", default = true,  title = "Sends and inserts" },
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

			repeat

				if not(proc:to_ioprocessor():isnil()) then
					--check if processor is a send or insert
					if proc:to_ioprocessor():display_to_user() then
						queue[#queue + 1] = proc
					end
				end

				if not(proc:to_insert():isnil()) then
					--check if processor is foreign to us
					if not(proc:to_insert():is_channelstrip()) and proc:display_to_user() and not(proc:to_insert():is_nonbypassable()) then
						--if it is, queue it for later
						queue[#queue + 1] = proc
					end
				end

				i = i + 1
				proc = t:nth_processor(i)
			until proc:isnil()

			for p = 1, #queue do
				if pref['sends'] then
					if not(queue[p]:to_ioprocessor():isnil()) then
						if not(pref["dest"]) then
							queue[p]:deactivate()
						else
							t:remove_processor(queue[p], nil, true)
						end
					end
				end
				if pref['plug'] then
					print(queue[p]:display_name())
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
	collectgarbage()
end end
