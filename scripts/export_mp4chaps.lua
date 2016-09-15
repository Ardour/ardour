ardour {
	["type"] = "EditorAction",
	name = "Export markers as mp4chaps",
	author = "Johannes Mueller",
	description = [[
Exports MP4chaps of all markers except xruns. The markers are stored in the
export directory of the session in mp4 chapter marks format. The filename
is mp4chaps.txt

Note that there's always a chapter mark "Intro" at 00:00:00.000 as some
players can't live without it. If there are no exportable markers, the file
is not created.

This is a bit more convenient than the export option, as one does not
have to wait for the export.
]],
	license = "GPLv2"
}

function factory (unused_params) return function ()

	fr = Session:frame_rate()
	chaps = {}

	for l in Session:locations():list():iter() do
		name = l:name()
		if not l:is_mark() or string.find(name, "^xrun%d*$") then
			goto next end

		t = l:start()
		h = math.floor(t / (3600*fr))
		r = t - (h*3600*fr)
		m = math.floor(r / (60*fr))
		r = r - m*60*fr
		s = math.floor(r / fr)
		r = r - s*fr
		ms = math.floor(r*1000/fr)
		table.insert(chaps, string.format("%02d:%02d:%02d.%03d %s\n", h, m, s, ms, name))
		::next::
	end

	if next(chaps) == nil then
		goto out end

	table.insert(chaps, "00:00:00.000 Intro\n")
	table.sort(chaps)

	file = io.open(ARDOUR.LuaAPI.build_filename (Session:path(), "export", "mp4chaps.txt"), "w")
	for i, line in ipairs(chaps) do
		file:write(line)
	end
	file:close()

	::out::
end end
