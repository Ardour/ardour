ardour {
	["type"] = "EditorAction",
	name = "Auto-Number Audiobook Chapters",
	author = "Claude",
	description = [[
Automatically numbers all markers as chapters in sequential order.

This script will:
1. Find all markers in the session
2. Sort them by time position
3. Update their chapter_info with sequential chapter numbers
4. Optionally rename markers to "Chapter 1", "Chapter 2", etc.

This is useful when you've recorded your audiobook and placed
markers at chapter breaks, but need to organize them properly.
]],
	license = "GPLv2"
}

function factory (unused_params) return function ()

	local markers = {}

	-- Collect all markers (excluding xruns)
	for l in Session:locations():list():iter() do
		if l:is_mark() and not string.find(l:name(), "^xrun%d*$") then
			table.insert(markers, {
				location = l,
				position = l:start()
			})
		end
	end

	if #markers == 0 then
		ARDOUR.LuaAPI.messagebox("No Markers Found",
			"No markers found in the session.\n\n" ..
			"Add markers at chapter break points first, then\n" ..
			"run this script to number them automatically.")
		return
	end

	-- Sort markers by position
	table.sort(markers, function(a, b)
		return a.position < b.position
	end)

	-- Ask user for options
	local dialog = LuaDialog.Dialog("Auto-Number Chapters", {
		{
			type = "checkbox",
			key = "rename",
			title = "Rename markers",
			default = true,
			description = "Rename markers to 'Chapter N'"
		},
		{
			type = "entry",
			key = "prefix",
			title = "Chapter prefix",
			default = "Chapter"
		},
		{
			type = "number",
			key = "start",
			title = "Start numbering from",
			default = 1,
			min = 0,
			max = 1000
		},
		{
			type = "checkbox",
			key = "metadata",
			title = "Add chapter metadata",
			default = true,
			description = "Add chapter number to chapter_info"
		}
	})

	local rv = dialog:run()
	if not rv then
		return -- User cancelled
	end

	-- Number the chapters
	local chapter_num = rv.start or 1
	local updated_count = 0

	Session:begin_reversible_command("Auto-Number Chapters")

	for i, marker_data in ipairs(markers) do
		local loc = marker_data.location

		if rv.rename then
			local new_name = string.format("%s %d", rv.prefix or "Chapter", chapter_num)
			loc:set_name(new_name)
		end

		if rv.metadata then
			-- Access chapter_info and add chapter number
			-- Note: This adds to the chapter_info map
			if loc.chapter_info then
				loc.chapter_info["number"] = tostring(chapter_num)
				if not loc.chapter_info["title"] or loc.chapter_info["title"] == "" then
					loc.chapter_info["title"] = string.format("%s %d", rv.prefix or "Chapter", chapter_num)
				end
			end
		end

		chapter_num = chapter_num + 1
		updated_count = updated_count + 1
	end

	Session:commit_reversible_command()

	-- Show success message
	ARDOUR.LuaAPI.messagebox("Chapters Numbered Successfully",
		string.format("✓ Updated %d chapters\n\n", updated_count) ..
		string.format("Numbered from %d to %d\n\n", rv.start, chapter_num - 1) ..
		"You can now:\n" ..
		"• Edit individual chapter details in Locations window\n" ..
		"• Export as M4B with chapter metadata\n" ..
		"• Run ACX compliance check")

end end

function icon (params) return function (ctx, width, height, fg)
	ctx:set_line_width (1.5)
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))

	-- Draw numbered list icon
	local line_h = height / 4

	for i = 1, 3 do
		local y = i * line_h
		-- Number
		ctx:arc (width * 0.2, y, 2, 0, 2 * math.pi)
		ctx:fill ()
		-- Line
		ctx:move_to (width * 0.35, y)
		ctx:line_to (width * 0.85, y)
		ctx:stroke ()
	end
end end
