ardour {
	["type"]    = "EditorAction",
	name        = "Record Now",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Engage Global Record-Arm and Play (record - roll)]]
}

function factory () return function ()
	Editor:access_action ("Transport", "record-roll")
end end

function icon (params) return function (ctx, width, height)
	local x = width * .5
	local y = height * .5
	local r = math.min (x, y) * .45
	local p = math.min (x, y) * .65

	ctx:move_to (x - p, y - p)
	ctx:line_to (x - p, y + p)
	ctx:line_to (x, y)
	ctx:close_path ()
	ctx:set_source_rgba (.3, .9, .3, 1.)
	ctx:fill_preserve ()
	ctx:set_source_rgba (0, 0, 0, .8)
	ctx:set_line_width (1)
	ctx:stroke ()

	ctx:arc (x + r, y, r, 0, 2 * math.pi)
	ctx:set_source_rgba (.9, .3, .3, 1.0)
	ctx:fill_preserve ()
	ctx:set_source_rgba (0, 0, 0, .8)
	ctx:set_line_width (1)
	ctx:stroke ()
end end
