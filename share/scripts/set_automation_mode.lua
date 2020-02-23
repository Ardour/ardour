ardour {
	["type"] = "EditorAction",
	name = "Engage Automation",
	author = "Ardour Team",
	description = [[Set automation mode of various controls (fader, trim, mute, pan), for all selected tracks.]]
}

function factory() return function()
	-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR.AutoState
	local auto_state = ARDOUR.AutoState.Touch
	local with_plugins = true

	-- Ask user which mode to use, and whether to include plugins
	local dialog_options = {
		{ type = "label", align="left", title = "Select automation state to apply all selected tracks:" },
		{
			type = "dropdown", key = "as", title="", values =
			{
				["Manual"] = ARDOUR.AutoState.Off,
				["Touch"] = ARDOUR.AutoState.Touch,
				["Write"] = ARDOUR.AutoState.Write,
				["Play"] = ARDOUR.AutoState.Play,
			},
			default = "Touch"
		},
		{ type = "checkbox", key = "plug", default = true, title = "Also set plugin controls" }
	}
	local rv = LuaDialog.Dialog ("Select Automation State", dialog_options):run()
	if not rv then return end
	auto_state = rv['as']
	with_plugins = rv['plug']

	-- helper function to check if given ARDOUR:AutomationControl exists
	function maybe_set_automation_state (ac)
		if not ac:isnil() then
			ac:set_automation_state (auto_state)
		end
	end

	-- helper function to iterate over all automatable parameters of a plugin
	function set_plugin_control_mode (pi)
		local pc = pi:plugin (0):parameter_count()
		for c = 0, pc do
			local ac = pi:to_automatable():automation_control (Evoral.Parameter (ARDOUR.AutomationType.PluginAutomation, 0, c), false)
			if not ac:isnil () then
				ac:set_automation_state (auto_state)
			end
		end
	end

	-- get selected tracks
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()

	if sel.tracks:routelist ():empty() then
		LuaDialog.Message ("Select Automation State", "No Tracks are selected.", LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run ()
		return
	end

	-- iterate over selected tracks
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:TrackSelection
	for r in sel.tracks:routelist ():iter () do
		-- r is-a  http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Route
		-- which interhits from ARDOUR:Stripable

		-- set route's direct control
		r:gain_control ():set_automation_state (auto_state)
		maybe_set_automation_state (r:trim_control ())
		maybe_set_automation_state (r:mute_control ())
		maybe_set_automation_state (r:pan_azimuth_control ())
		maybe_set_automation_state (r:pan_width_control ())

		-- for every plugin
		local i = 0
		while with_plugins do
			local proc = r:nth_plugin (i)
			if proc:isnil () then break end
			set_plugin_control_mode (proc:to_insert ())
			i = i + 1
		end

	end
end end
