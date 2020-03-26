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

	local fr = Session:sample_rate()
	local chaps = {}

	for l in Session:locations():list():iter() do
		local name = l:name()
		if not l:is_mark() or string.find(name, "^xrun%d*$") then
			goto next end

		local t = l:start() - Session:current_start_sample()
		local h = math.floor(t / (3600*fr))
		local r = t - (h*3600*fr)
		local m = math.floor(r / (60*fr))
		r = r - m*60*fr
		local s = math.floor(r / fr)
		r = r - s*fr
		local ms = math.floor(r*1000/fr)
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

function icon (params) return function (ctx, width, height, fg)
	local mh = height - 3.5;
	local m3 = width / 3;
	local m6 = width / 6;

	ctx:set_line_width (.5)
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))

	ctx:move_to (width / 2 - m6, 2)
	ctx:rel_line_to (m3, 0)
	ctx:rel_line_to (0, mh * 0.4)
	ctx:rel_line_to (-m6, mh * 0.6)
	ctx:rel_line_to (-m6, -mh * 0.6)
	ctx:close_path ()
	ctx:stroke ()

	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil(math.min (width, height) * .5) .. "px")
	txt:set_text ("MP4")
	local tw, th = txt:get_pixel_size ()
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	txt:show_in_cairo_context (ctx)
end end
