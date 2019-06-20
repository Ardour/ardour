ardour {
	["type"] = "EditorAction",
	name = "Track Sort",
	author = "Ardour Lua Taskforce",
	description = [[Sort tracks alphabetically by name]]
}

function factory () return function ()

	function tsort (a, b)
		return a:name() < b:name()
	end

	local tracklist = {}
	for t in Session:get_tracks():iter() do
		table.insert(tracklist, t)
	end

	table.sort(tracklist, tsort)

	local pos = 1;
	for _, t in ipairs(tracklist) do
		t:set_presentation_order(pos)
		pos = pos + 1
	end

	tracklist = nil
	collectgarbage ()
end end
