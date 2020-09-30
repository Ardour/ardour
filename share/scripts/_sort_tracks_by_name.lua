ardour {
	["type"] = "EditorAction",
	name = "Track Sort",
	author = "Ardour Team",
	description = [[Sort tracks alphabetically by name]]
}

function factory () return function ()

	-- sort compare function
	-- a,b here are http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Route
	-- return true if route "a" should be ordered before route "b"
	function tsort (a, b)
		return a:name() < b:name()
	end

	-- create a sortable list of tracks
	local tracklist = {}
	for t in Session:get_tracks():iter() do
		table.insert(tracklist, t)
	end

	-- sort the list using the compare function
	table.sort(tracklist, tsort)

	-- traverse the sorted list and assign "presentation-order" to each track
	local pos = 1;
	for _, t in ipairs(tracklist) do
		t:set_presentation_order(pos)
		pos = pos + 1
	end
end end
