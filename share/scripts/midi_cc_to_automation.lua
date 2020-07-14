ardour { ["type"] = "EditorAction", name = "MIDI CC to Plugin Automation",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Parse a given MIDI control changes (CC) from all selected MIDI regions and convert them into plugin parameter automation]]
}

function factory () return function ()
	-- find possible target parameters, collect them in a nested table
	--   [track-name] -> [plugin-name] -> [parameters]
	-- to allow selection in a dropdown menu
	local targets = {}
	local have_entries = false
	for r in Session:get_routes ():iter () do -- for every track/bus
		if r:is_monitor () or r:is_auditioner () then goto nextroute end -- skip special routes
		local i = 0
		while true do -- iterate over all plugins on the route
			local proc = r:nth_plugin (i)
			if proc:isnil () then break end
			local plug = proc:to_insert ():plugin (0) -- we know it's a plugin-insert (we asked for nth_plugin)
			local n = 0 -- count control-ports
			for j = 0, plug:parameter_count () - 1 do -- iterate over all plugin parameters
				if plug:parameter_is_control (j) then
					local label = plug:parameter_label (j)
					if plug:parameter_is_input (j) and label ~= "hidden" and label:sub (1,1) ~= "#" then
						local nn = n --local scope for return value function
						-- create table parents only if needed (if there's at least one parameter)
						if not targets [r:name ()] then targets [r:name ()] = {} end
						-- TODO handle ambiguity if there are 2 plugins with the same name on the same track
						if not targets [r:name ()][proc:display_name ()] then targets [r:name ()][proc:display_name ()] = {} end
						-- we need 2 return values: the plugin-instance and the parameter-id, so we use a table (associative array)
						-- however, we cannot directly use a table: the dropdown menu would expand it as another sub-menu.
						-- so we produce a function that will return the table.
						targets [r:name ()][proc:display_name ()][label] = function () return {["p"] = proc, ["n"] = nn} end
						have_entries = true
					end
					n = n + 1
				end
			end
			i = i + 1
		end
		::nextroute::
	end

	-- bail out if there are no parameters
	if not have_entries then
		LuaDialog.Message ("CC to Plugin Automation", "No Plugins found", LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run ()
		return
	end

	-- create a dialog, ask user which MIDI-CC to map and to what parameter
	local dialog_options = {
		{ type = "heading", title = "MIDI CC Source", align = "left" },
		{ type = "number", key = "channel", title = "Channel",  min = 1, max = 16, step = 1, digits = 0 },
		{ type = "number", key = "ccparam", title = "CC Parameter",  min = 0, max = 127, step = 1, digits = 0 },
		{ type = "heading", title = "Target Track and Plugin", align = "left"},
		{ type = "dropdown", key = "param", title = "Target Parameter", values = targets }
	}
	local rv = LuaDialog.Dialog ("Select Taget", dialog_options):run ()

	targets = nil -- drop references (the table holds shared-pointer references to all plugins)
	collectgarbage () -- and release the references immediately

	if not rv then return end -- user cancelled

	-- parse user response

	assert (type (rv["param"]) == "function")
	local midi_channel = rv["channel"] - 1 -- MIDI channel 0..15
	local cc_param = rv["ccparam"]
	local pp = rv["param"]() -- evaluate function, retrieve table {["p"] = proc, ["n"] = nn}
	local al, _, pd = ARDOUR.LuaAPI.plugin_automation (pp["p"], pp["n"])
	rv = nil -- drop references
	assert (not al:isnil ())
	assert (midi_channel >= 0 and midi_channel < 16)
	assert (cc_param >= 0 and cc_param < 128)

	-- all systems go
	local add_undo = false
	Session:begin_reversible_command ("CC to Automation")
	local before = al:get_state () -- save previous state (for undo)
	al:clear_list () -- clear target automation-list

	-- for all selected MIDI regions
	local sel = Editor:get_selection ()
	for r in sel.regions:regionlist ():iter () do
		local mr = r:to_midiregion ()
		if mr:isnil () then goto next end

		-- get list of MIDI-CC events for the given channel and parameter
		local ec = mr:control (Evoral.Parameter (ARDOUR.AutomationType.MidiCCAutomation, midi_channel, cc_param), false)
		if ec:isnil () then goto next end
		if ec:list ():events ():size () == 0 then goto next end

		-- MIDI events are timestamped in "bar-beat" units, we need to convert those
		-- using the tempo-map, relative to the region-start
		local bfc = ARDOUR.BeatsSamplesConverter (Session:tempo_map (), r:start ())

		-- iterate over CC-events
		for av in ec:list ():events ():iter () do
			-- re-scale event to target range
			local val = pd.lower + (pd.upper - pd.lower) * av.value / 127
			-- and add it to the target-parameter automation-list
			al:add (r:position () - r:start () + bfc:to (av.when), val, false, true)
			add_undo = true
		end
		::next::
	end

	-- save undo
	if add_undo then
		local after = al:get_state ()
		Session:add_command (al:memento_command (before, after))
		Session:commit_reversible_command (nil)
	else
		Session:abort_reversible_command ()
		LuaDialog.Message ("CC to Plugin Automation", "No data was converted. Was a MIDI-region with CC-automation selected? ", LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run ()
	end
end end

function icon (params) return function (ctx, width, height, fg)
	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil (height / 3) .. "px")
	txt:set_text ("CC\nPA")
	local tw, th = txt:get_pixel_size ()
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	txt:show_in_cairo_context (ctx)
end end
