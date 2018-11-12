ardour { ["type"] = "EditorAction", name = "Delete xrun markers", author = "Ardour Team", description = [[Delete all xrun markers in the current session]] }

function factory () return function ()
	for l in Session:locations():list():iter() do
		if l:is_mark() and string.find (l:name(), "^xrun%d*$") then
			Session:locations():remove (l);
		end
	end
end end

function icon (params) return function (ctx, width, height, fg)
	local mh = height - 3.5;
	local m3 = width / 3;
	local m6 = width / 6;

	ctx:set_line_width (.5)

	ctx:set_source_rgba (.8, .2, .2, 1.0)
	ctx:move_to (width / 2 - m6, 2)
	ctx:rel_line_to (m3, 0)
	ctx:rel_line_to (0, mh * 0.4)
	ctx:rel_line_to (-m6, mh * 0.6)
	ctx:rel_line_to (-m6, -mh * 0.6)
	ctx:close_path ()
	ctx:fill_preserve ()
	ctx:set_source_rgba (.0, .0, .0, 1.0)
	ctx:stroke ()

	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	ctx:set_line_width (1)

	ctx:move_to (width * .2, height * .2)
	ctx:line_to (width * .8, height * .8)
	ctx:stroke ()

	ctx:move_to (width * .8, height * .2)
	ctx:line_to (width * .2, height * .8)
	ctx:stroke ()

end end
