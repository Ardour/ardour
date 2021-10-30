ardour { ["type"] = "Snippet", name = "Thin Fader Automation" }

-- --TODO--
-- For a fully fledged EditorAction this script should
-- offer a dropdown to select automation of all parameters
-- (not just the fader)
-- see scripts/midi_cc_to_automation.lua and
-- scripts/mixer_settings_store.lua
-- Thinning Area should also be a numeric-entry or slider

function factory () return function ()
	-- get selected tracks
	rl = Editor:get_selection ().tracks:routelist ()

	-- prepare undo operation
	Session:begin_reversible_command ("Thin Automation")
	local add_undo = false -- keep track if something has changed

	-- iterate over selected tracks
	for r in rl:iter () do

		-- get the Fader (aka "amp") control
		local ac = r:amp ():gain_control () -- ARDOUR:AutomationControl
		local al = ac:alist () -- ARDOUR:AutomationList

		-- get state for undo
		local before = al:get_state ()

		-- remove dense events
		al:thin (50) -- threashold of area below curve

		-- save undo
		local after = al:get_state ()
		Session:add_command (al:memento_command (before, after))
		add_undo = true

		::out::
	end

	-- all done, commit the combined Undo Operation
	if add_undo then
		-- the 'nil' Commend here mean to use the collected diffs added above
		Session:commit_reversible_command (nil)
	else
		Session:abort_reversible_command ()
	end
end end
