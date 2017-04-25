ardour { ["type"] = "Snippet", name = "MIDI CC to Plugin Automation" }

function factory () return function ()
	-- find target parameters
	local targets = {}
	local have_entries = false
	for r in Session:get_routes():iter() do -- for every track/bus
		local i = 0;
		while 1 do -- iterate over all plugins on the route
			local proc = r:nth_plugin (i)
			if proc:isnil () then break end
			if not targets [r:name ()] then targets [r:name ()] = {} end
			targets [r:name ()][proc:display_name ()] = {};
			local plug = proc:to_insert ():plugin (0)
			local n = 0
			for j = 0, plug:parameter_count () - 1 do -- iterate over all plugin parameters
				if plug:parameter_is_control(j) then
					if plug:parameter_is_input(j) then
						local nn = n
						targets [r:name ()][proc:display_name ()][plug:parameter_label(j)] = function () return {["p"] = proc, ["n"] = nn} end
						have_entries = true
					end
					n = n + 1
				end
			end
			i = i + 1
		end
	end

	-- bail out if there are no parameters
	if not have_entries then
		LuaDialog.Message ("CC to Plugin", "No Plugins found", LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run()
		collectgarbage ()
		return
	end

	-- create a dialog, ask user which MIDI-CC to map and to what parameter
	local dialog_options = {
		{ type = "dropdown", key = "param", title = "Target Parameter", values = targets },
		{ type = "number", key = "channel", title = "Channel",  min = 1, max = 16, step = 1, digits = 0 },
		{ type = "number", key = "ccparam", title = "CC Parameter",  min = 0, max = 127, step = 1, digits = 0 }
	}
	local od = LuaDialog.Dialog ("Select Taget", dialog_options)
	local rv = od:run()

	if not rv then
		od = nil collectgarbage ()
		return;
	end

	-- parse user response

	local midi_channel = rv["channel"] - 1
	local cc_param = rv["ccparam"]
	local pp = rv["param"]()
	local al, _, pd = ARDOUR.LuaAPI.plugin_automation (pp["p"], pp["n"])
	od = nil collectgarbage ()
	assert (al)
	assert (midi_channel >= 0 and midi_channel < 16)
	assert (cc_param >= 0 and cc_param < 128)

	-- all systems go
	local add_undo = false
	Session:begin_reversible_command ("CC to Automation")
	local before = al:get_state()
	al:clear_list ()

	-- for all selected MIDI regions
	local sel = Editor:get_selection ()
	for r in sel.regions:regionlist ():iter () do
		local mr = r:to_midiregion ()
		if mr:isnil () then goto next end

		local bfc = ARDOUR.DoubleBeatsFramesConverter (Session:tempo_map (), r:position ())
		local ec = mr:control (Evoral.Parameter (ARDOUR.AutomationType.MidiCCAutomation, midi_channel, cc_param), false)
		if ec:isnil () then goto next end
		if ec:list ():events ():size() == 0 then goto next end

		for av in ec:list ():events ():iter () do
			local val = pd.lower + (pd.upper - pd.lower) * av.value / 127
			al:add (bfc:to (av.when), val, false, true)
			add_undo = true
		end
		::next::
	end

	-- save undo
	if add_undo then
		local after = al:get_state()
		Session:add_command (al:memento_command(before, after))
		Session:commit_reversible_command (nil)
	else
		Session:abort_reversible_command ()
	end
end end
