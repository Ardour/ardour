ardour {
	["type"] = "EditorAction",
	name = "Export Audiobook as M4B with Chapters",
	author = "Claude (based on export_mp4chaps.lua by Johannes Mueller)",
	description = [[
Exports the current session as an M4B audiobook file with embedded chapter markers.

This script:
1. Exports all markers with chapter metadata as MP4 chapter marks
2. Creates a chapter file compatible with ffmpeg/mp4box
3. Provides instructions for creating the final M4B file

Chapter metadata (title, description, author) from the Location chapter_info
will be used if available, otherwise the marker name is used.

The chapter file is saved in the export directory as audiobook_chapters.txt

To create the final M4B file, you need ffmpeg installed:
  ffmpeg -i input.m4a -i audiobook_chapters.txt -map 0:a -map 1 -c copy -metadata:s:v title="Chapter List" output.m4b

Note: You must first export your session as M4A (AAC format) using Ardour's
normal export function, then run this script to generate the chapter file.
]],
	license = "GPLv2"
}

function factory (unused_params) return function ()

	local fr = Session:sample_rate()
	local chaps = {}
	local chapter_count = 0

	-- Iterate through all locations
	for l in Session:locations():list():iter() do
		local name = l:name()

		-- Skip xrun markers and non-marker locations
		if not l:is_mark() or string.find(name, "^xrun%d*$") then
			goto next
		end

		-- Calculate time position
		local t = l:start() - Session:current_start_sample()
		local h = math.floor(t / (3600*fr))
		local r = t - (h*3600*fr)
		local m = math.floor(r / (60*fr))
		r = r - m*60*fr
		local s = math.floor(r / fr)
		r = r - s*fr
		local ms = math.floor(r*1000/fr)

		-- Get chapter metadata if available
		local chapter_title = name
		local chapter_desc = ""

		if l.chapter_info then
			if l.chapter_info["title"] and l.chapter_info["title"] ~= "" then
				chapter_title = l.chapter_info["title"]
			end
			if l.chapter_info["description"] and l.chapter_info["description"] ~= "" then
				chapter_desc = " - " .. l.chapter_info["description"]
			end
		end

		-- Format: HH:MM:SS.mmm Title
		table.insert(chaps, string.format("%02d:%02d:%02d.%03d %s%s\n",
			h, m, s, ms, chapter_title, chapter_desc))
		chapter_count = chapter_count + 1

		::next::
	end

	if next(chaps) == nil then
		ARDOUR.LuaAPI.messagebox("No Chapter Markers",
			"No chapter markers found in session.\n\n" ..
			"Please add markers (Locations) to define your audiobook chapters.\n" ..
			"Use the 'Chapter' checkbox in the Locations window to add chapter metadata.")
		goto out
	end

	-- Always add intro chapter at start
	table.insert(chaps, "00:00:00.000 Intro\n")
	table.sort(chaps)

	-- Write chapter file
	local export_dir = ARDOUR.LuaAPI.build_filename(Session:path(), "export")
	local chapter_file = ARDOUR.LuaAPI.build_filename(export_dir, "audiobook_chapters.txt")

	file = io.open(chapter_file, "w")
	if not file then
		ARDOUR.LuaAPI.messagebox("Error",
			"Could not create chapter file at:\n" .. chapter_file)
		goto out
	end

	file:write(";FFMETADATA1\n")
	for i, line in ipairs(chaps) do
		file:write(line)
	end
	file:close()

	-- Show success message with instructions
	local session_name = Session:name()
	ARDOUR.LuaAPI.messagebox("Audiobook Chapters Exported",
		string.format("✓ Successfully created chapter file with %d chapters\n\n", chapter_count) ..
		"File: " .. chapter_file .. "\n\n" ..
		"Next steps to create M4B:\n" ..
		"1. Export your session as M4A (AAC) format\n" ..
		"2. Run this command in terminal:\n\n" ..
		"   cd " .. export_dir .. "\n" ..
		"   ffmpeg -i " .. session_name .. ".m4a \\\n" ..
		"          -i audiobook_chapters.txt \\\n" ..
		"          -map 0:a -map_metadata 1 \\\n" ..
		"          -c copy \\\n" ..
		"          -metadata title=\"" .. session_name .. "\" \\\n" ..
		"          " .. session_name .. ".m4b\n\n" ..
		"(Requires ffmpeg to be installed)")

	::out::
end end

function icon (params) return function (ctx, width, height, fg)
	local mh = height - 3.5;
	local m3 = width / 3;
	local m6 = width / 6;

	ctx:set_line_width (.5)
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))

	-- Draw book icon
	ctx:move_to (width / 2 - m6, 2)
	ctx:rel_line_to (m3, 0)
	ctx:rel_line_to (0, mh * 0.4)
	ctx:rel_line_to (-m6, mh * 0.6)
	ctx:rel_line_to (-m6, -mh * 0.6)
	ctx:close_path ()
	ctx:stroke ()

	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil(math.min (width, height) * .45) .. "px")
	txt:set_text ("M4B")
	local tw, th = txt:get_pixel_size ()
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	txt:show_in_cairo_context (ctx)
end end
