ardour { ["type"] = "Snippet", name = "Evaluate Region Gain Curve" }
-- see also "Set Region Gain Curve" snippet

function factory () return function ()

	local sel = Editor:get_selection ()

	-- iterate over selected regions
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:RegionSelection
	for r in sel.regions:regionlist ():iter () do

		-- test if it's an audio region
		local ar = r:to_audioregion ();
		if ar:isnil () then
			goto next
		end

		-- get region-gain-curve is-a
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:AutomationList
		local al = ar:envelope ()

		-- evaluate at 3 seconds (interpolated value)
		print ("Value at 3sec", al:eval(Temporal.timepos_t(Session:nominal_sample_rate () * 3)))

		-- iterate over events in the Automation List
		for ev in al:events():iter() do
			-- each event is-a https://manual.ardour.org/lua-scripting/class_reference/#Evoral:ControlEvent
			-- when is-a https://manual.ardour.org/lua-scripting/class_reference/#Temporal:timepos_t
			print (ev.when, ev.value)
		end

		::next::
	end
end end
