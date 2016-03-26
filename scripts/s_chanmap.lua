ardour { ["type"] = "Snippet", name = "plugin channel mapping" }

function factory () return function ()
	-- first track needs to be stereo and has a stereo plugin
	-- (x42-eq with spectrum display, per channel processing,
	--  and pre/post visualization is handy here)

	r = Session:route_by_remote_id(1)
	pi = r:nth_plugin(0):to_insert()

	pi:set_no_inplace (true)

	cm = ARDOUR.ChanMapping()
	--cm:set(ARDOUR.DataType("Audio"), 0, 0)
	cm:set(ARDOUR.DataType("Audio"), 1, 0)
	pi:set_input_map (0, cm)

	cm = ARDOUR.ChanMapping()
	--cm:set(ARDOUR.DataType("Audio"), 0, 0)
	cm:set(ARDOUR.DataType("Audio"), 1, 0)
	pi:set_output_map (0, cm)
end end
