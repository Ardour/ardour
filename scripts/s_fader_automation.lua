ardour { ["type"] = "Snippet", name = "Fader Automation" }

function factory () return function ()
	local playhead = Session:transport_sample ()
	local samplerate = Session:nominal_sample_rate ()

	-- get selected tracks
	rl = Editor:get_selection ().tracks:routelist ()

	-- prepare undo operation
	Session:begin_reversible_command ("Fancy Fade Out")
	local add_undo = false -- keep track if something has changed

	-- iterate over selected tracks
	for r in rl:iter () do
		local ac = r:amp ():gain_control () -- ARDOUR:AutomationControl
		local al = ac:alist () -- ARDOUR:AutomationList (state, high-level)

		-- set automation state to "Touch"
		ac:set_automation_state (ARDOUR.AutoState.Touch)

		-- query the value at the playhead position
		local g = al:eval (playhead)

		-- get state for undo
		local before = al:get_state ()

		-- delete all events after the playhead...
		al:truncate_end (playhead)

		-- ...and generate some new ones.
		for i=0,50 do
			-- use a sqrt fade-out (the shape is recognizable, and otherwise
			-- not be possible to achieve with existing ardour fade shapes)
			al:add (playhead + i * samplerate / 50,
			        g * (1 - math.sqrt (i / 50)),
			        false, true)
		end

		-- remove dense events
		al:thin (20)

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
