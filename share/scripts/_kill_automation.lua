ardour {
  ["type"]    = "EditorAction",
  name        = "Clear All Automation",
  license     = "MIT",
  author      = "Robin Gareus",
  description = [[Clear all automation]]
}

function factory() return function()

	Session:begin_reversible_command ("Clear All Automation")

	function reset_automatable (auto)
		local params = auto:all_automatable_params ()
		for param in params:iter() do
			local ac = auto:automation_control (param, false) -- ARDOUR:AutomationControl
			ac:set_automation_state (ARDOUR.AutoState.Off)
			local al = ac:alist () -- ARDOUR:AutomationList
			local before = al:get_state ()
			al:clear_list () -- delete all events
			local after = al:get_state ()
			Session:add_command (al:memento_command (before, after))
		end
	end

	for route in Session:get_routes():iter() do
		reset_automatable (route:to_automatable ())
		local i = 0
		repeat
			local proc = route:nth_processor(i)
			if not proc:isnil() and proc:display_to_user () then
				reset_automatable (proc:to_automatable ())
			end
			i = i + 1
		until proc:isnil()
	end

	if not Session:abort_empty_reversible_command () then
		Session:commit_reversible_command (nil)
	end

end end
