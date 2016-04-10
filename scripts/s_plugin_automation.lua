ardour { ["type"] = "Snippet", name = "plugin automation2" }

function factory () return function ()
	local playhead = Session:transport_frame ()
	local samplerate = Session:nominal_frame_rate ()

	local r = Session:route_by_remote_id(3)
	-- get AutomationControList, ControlList and ParameterDescriptor
	local acl, cl, pd = ARDOUR.LuaAPI.plugin_automation (r:nth_plugin (0), 0)

	if not acl:isnil() then
		print ("Parameter Range", pd.lower, pd.upper)
		print ("Current value", cl:eval(playhead))

		-- prepare undo operation
		Session:begin_reversible_command ("Automatix")
		local before = acl:get_state()

		-- remove future automation
		cl:truncate_end (playhead)

		-- add new data points after the playhead 1 sec min..max
		-- without guard-points, but with initial (..., false, true)
		for i=0,10 do
			cl:add (playhead + i * samplerate / 10,
				 pd.lower + math.sqrt (i / 10) * (pd.upper - pd.lower),
				 false, true)
		end

		-- save undo
		local after = acl:get_state()
		Session:add_command (acl:memento_command(before, after))
		Session:commit_reversible_command (nil)
	end
end end
