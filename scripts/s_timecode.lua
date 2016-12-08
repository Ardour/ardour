ardour { ["type"] = "Snippet", name = "Timecode" }

function factory () return function ()

	-- generic convert, explicitly provide Timecode (fps) and sample-rate
	hh, mm, ss, ff = ARDOUR.LuaAPI.sample_to_timecode (Timecode.TimecodeFormat.TC25, 48000, 1920)
	print (ARDOUR.LuaAPI.sample_to_timecode (Timecode.TimecodeFormat.TC25, 48000, 1920))

	-- generic convert, explicitly provide Timecode (fps) and sample-rate
	local s = ARDOUR.LuaAPI.timecode_to_sample (Timecode.TimecodeFormat.TC25, 48000, 10, 11, 12, 13)
	assert (25 * (10 * 3600 + 11 * 60 + 12 ) + 13 == s * 25 / 48000)

	-- use session-settings
	print (Session:sample_to_timecode_lua (12345))
	print (Session:timecode_to_sample_lua (10, 11, 12, 13))

end end
