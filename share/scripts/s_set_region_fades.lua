ardour { ["type"] = "Snippet", name = "Set Region Fades" }

function factory () return function ()

	-- ensure sure that fades are used and visible
	-- (Session > Properties > Fades)
	assert (Session:cfg():get_use_region_fades())
	assert (Session:cfg():get_show_region_fades())

	-- get Editor selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Editor
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()
	-- query the session's currane sample-rate
	local sr = Session:nominal_sample_rate ()

	-- iterate over Regions that part of the selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:RegionSelection
	for r in sel.regions:regionlist ():iter () do
		-- each of the items 'r' is-a
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Region

		-- test if it is an audio region
		local ar = r:to_audioregion ()
		if ar:isnil () then goto next end

		-- fade in/out for 500 msec, or half the region-length, whatever is shorter
		local fadelen = .5 * sr
		if fadelen > r:length () / 2 then
			fadelen = r:length () / 2
		end

		-- https://manual.ardour.org/lua-scripting/class_reference/#ARDOUR.FadeShape
		ar:set_fade_in_shape (ARDOUR.FadeShape.FadeConstantPower)
		ar:set_fade_in_length (fadelen)
		ar:set_fade_in_active (true)

		ar:set_fade_out_shape (ARDOUR.FadeShape.FadeConstantPower)
		ar:set_fade_out_length (fadelen)
		ar:set_fade_out_active (true)

		::next::
	end
end end
