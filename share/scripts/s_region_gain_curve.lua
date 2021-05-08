ardour { ["type"] = "Snippet", name = "Set Region Gain Curve" }

function factory () return function ()
	-- get Editor GUI Selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()

	-- prepare undo operation
	Session:begin_reversible_command ("Lua Region Gain Curve")

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

		-- get state for undo
		local before = al:get_state ()

		-- delete all current events
		al:clear_list ()

		-- add some new ones
		for i=0,50 do
			al:add (i * r:length () / 50,
			        1 - math.sqrt (i / 50),
			        false, true)
		end

		-- remove dense events
		al:thin (20)

		-- save undo
		local after = al:get_state ()
		Session:add_command (al:memento_command (before, after))

		::next::
	end

	if not Session:abort_empty_reversible_command () then
		Session:commit_reversible_command (nil)
	end

end end
