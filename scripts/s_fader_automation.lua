ardour { ["type"] = "Snippet", name = "fader automation" }

function factory () return function ()
	local playhead = Session:transport_frame ()
	local samplerate = Session:nominal_frame_rate ()

	-- get selected tracks
	rl = Editor:get_selection().tracks:routelist()
	-- prepare undo operation
	Session:begin_reversible_command ("Fancy Fade Out")
	local add_undo = false -- keep track if something has changed
	-- iterate over selected tracks
	for r in rl:iter() do
		local ac = r:amp():gain_control() -- ARDOUR:AutomationControl
		local acl = ac:alist() -- ARDOUR:AutomationControlList (state, high-level)
		local cl = acl:list()  -- Evoral:ControlList (actual events)

		ac:set_automation_state(ARDOUR.AutoState.Touch)

		if cl:isnil() then
			goto out
		end

		-- query the value at the playhead position
		local g = cl:eval(playhead)

		-- get state for undo
		local before = acl:get_state()

		-- delete all events after the playhead...
		cl:truncate_end (playhead)
		-- ...and generate some new ones.
		for i=0,50 do
			cl:add (playhead + i * samplerate / 50,
				 g * (1 - math.sqrt (i / 50)),
				 false, true)
		end
		-- remove dense events
		cl:thin(20)

		-- save undo
		local after = acl:get_state()
		Session:add_command (acl:memento_command(before, after))
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
