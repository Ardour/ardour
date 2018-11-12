ardour { ["type"] = "Snippet", name = "Plugin automation" }

function factory () return function ()
	-- query playhead position and session sample-rate
	local playhead = Session:transport_sample ()
	local samplerate = Session:nominal_sample_rate ()

	-- get Track/Bus with RID 3
	local r = Session:get_remote_nth_route(3)
	-- make sure the track object exists
	assert (not r:isnil ())

	-- get AutomationList, ControlList and ParameterDescriptor
	-- of the first plugin's first parameter
	-- see http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:LuaAPI
	local al, cl, pd = ARDOUR.LuaAPI.plugin_automation (r:nth_plugin (0), 0)

	if not al:isnil () then
		print ("Parameter Range", pd.lower, pd.upper)
		print ("Current value", cl:eval (playhead))

		-- prepare undo operation
		Session:begin_reversible_command ("Automatix")
		-- remember current AutomationList state
		local before = al:get_state()

		-- remove future automation
		cl:truncate_end (playhead)

		-- add new data points after the playhead 1 sec, min..max
		-- without guard-points, but with initial (..., false, true)
		for i=0,10 do
			cl:add (playhead + i * samplerate / 10,
				 pd.lower + math.sqrt (i / 10) * (pd.upper - pd.lower),
				 false, true)
		end

		-- save undo
		local after = al:get_state()
		Session:add_command (al:memento_command(before, after))
		Session:commit_reversible_command (nil)
	end
end end
