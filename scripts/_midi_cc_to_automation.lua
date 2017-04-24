ardour { ["type"] = "Snippet", name = "MIDI CC to Plugin Automation" }

function factory () return function ()
	-- target automation lane: a plugin parameter on a track
	local tgt = Session:route_by_name('Audio') -- get track
	assert (not tgt:isnil ())
	-- get AutomationList, ControlList and ParameterDescriptor
	-- of the first plugin's 2nd parameter
	local al, _, pd = ARDOUR.LuaAPI.plugin_automation (tgt:nth_plugin (0), 1)

	-- Source MIDI CC parameter
	local midi_channel = 0 -- 0..15
	local cc_param = 0x56

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
