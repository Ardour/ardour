ardour { ["type"] = "Snippet", name = "Timecode" }

function factory () return function ()

	local samplerate = 48000 -- samples per second

	-- generic convert, explicitly provide Timecode (fps) and sample-rate
	-- http://manual.ardour.org/lua-scripting/class_reference/#Timecode.TimecodeFormat
	hh, mm, ss, ff = ARDOUR.LuaAPI.sample_to_timecode (Timecode.TimecodeFormat.TC25, samplerate, 1920)
	print (ARDOUR.LuaAPI.sample_to_timecode (Timecode.TimecodeFormat.TC25, samplerate, 1920))

	-- generic convert, explicitly provide Timecode (fps) and sample-rate
	local s = ARDOUR.LuaAPI.timecode_to_sample (Timecode.TimecodeFormat.TC25, samplerate, 10, 11, 12, 13)
	assert (25 * (10 * 3600 + 11 * 60 + 12 ) + 13 == s * 25 / samplerate)

	-- use session-settings: sample-rate and timecode format is taken from the
	-- current session. Note that the sample-rate includes pull-up/down
	print (Session:sample_to_timecode_lua (12345))
	print (Session:timecode_to_sample_lua (10, 11, 12, 13))

end end
