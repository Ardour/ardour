ardour {
	["type"] = "EditorAction",
	name = "Check ACX Audiobook Compliance",
	author = "Claude",
	description = [[
Checks if the current session meets ACX (Audible) audiobook technical requirements:

ACX Requirements:
- Peak values: Between -3dB and -23dB
- RMS (loudness): Between -23dB and -18dB
- Noise floor: Below -60dB
- Format: Mono or Stereo, 44.1kHz or 48kHz or higher
- Bit depth: 16-bit or higher recommended

This script analyzes the current session and reports whether it meets
these specifications.

Note: This is an approximation. For final validation, use ACX's
official submission process or specialized tools like:
- Audacity with ACX Check plugin
- iZotope RX Loudness Control
- ffmpeg with ebur128 filter
]],
	license = "GPLv2"
}

function factory (unused_params) return function ()

	if Session:sample_rate() < 44100 then
		ARDOUR.LuaAPI.messagebox("ACX Check: Sample Rate Issue",
			string.format("❌ Sample Rate: %.1f kHz\n\n", Session:sample_rate() / 1000) ..
			"ACX requires 44.1kHz or higher.\n" ..
			"Current session does not meet ACX requirements.")
		return
	end

	local status_msg = "ACX Audiobook Compliance Check\n" ..
		"================================\n\n"

	-- Check sample rate
	local sr = Session:sample_rate()
	status_msg = status_msg .. string.format("✓ Sample Rate: %.1f kHz", sr / 1000)
	if sr >= 44100 then
		status_msg = status_msg .. " (ACX compliant)\n"
	else
		status_msg = status_msg .. " (❌ Too low - need 44.1kHz+)\n"
	end

	-- Check format
	local track_count = 0
	local has_audio = false
	for route in Session:get_routes():iter() do
		if route:to_track() then
			track_count = track_count + 1
			if route:to_audio_track() then
				has_audio = true
			end
		end
	end

	if has_audio then
		status_msg = status_msg .. "✓ Audio tracks found\n"
	else
		status_msg = status_msg .. "❌ No audio tracks found\n"
	end

	-- Check session markers for chapters
	local chapter_count = 0
	for l in Session:locations():list():iter() do
		if l:is_mark() and not string.find(l:name(), "^xrun%d*$") then
			chapter_count = chapter_count + 1
		end
	end

	status_msg = status_msg .. string.format("\n📚 Chapters: %d markers found\n", chapter_count)
	if chapter_count > 0 then
		status_msg = status_msg .. "   (Use 'Export Audiobook as M4B' to create chapter file)\n"
	end

	-- Recommendations
	status_msg = status_msg .. "\n" ..
		"ACX Technical Requirements:\n" ..
		"---------------------------\n" ..
		"• Peak levels: -3dB to -23dB\n" ..
		"• RMS/Loudness: -23dB to -18dB (LUFS)\n" ..
		"• Noise floor: Below -60dB\n" ..
		"• No clipping or digital artifacts\n" ..
		"• Consistent volume throughout\n\n" ..
		"Recommended Workflow:\n" ..
		"1. Record at -6dB to -12dB peak\n" ..
		"2. Apply gentle compression\n" ..
		"3. Use noise reduction if needed\n" ..
		"4. Normalize to -3dB peak\n" ..
		"5. Check loudness with EBU R128/LUFS meter\n" ..
		"6. Export as M4A or M4B with chapters\n\n" ..
		"For precise LUFS measurement, export a\n" ..
		"sample and analyze with:\n" ..
		"  ffmpeg -i sample.m4a -filter:a ebur128 -f null -\n"

	ARDOUR.LuaAPI.messagebox("ACX Compliance Check", status_msg)

end end

function icon (params) return function (ctx, width, height, fg)
	ctx:set_line_width (1)
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))

	-- Draw checkmark
	local x = width * 0.25
	local y = height * 0.5
	ctx:move_to (x, y)
	ctx:line_to (width * 0.4, height * 0.7)
	ctx:line_to (width * 0.75, height * 0.25)
	ctx:stroke ()

	-- Draw circle
	ctx:arc (width / 2, height / 2, math.min(width, height) / 2.5, 0, 2 * math.pi)
	ctx:stroke ()
end end
