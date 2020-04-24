ardour {
	["type"] = "EditorAction",
	name = "Create Drum Tracks",
	author = "PSmith",
	description = [[Creates eight new tracks with representative drum names and colors.]]
}

function factory () return function ()
		local names = {
			"Kick",
			"Snare",
			"Hat",
			"Fl Tom",
			"OH L",
			"OH R",
			"Room 1",
			"Room 2"
		}

		local color = 0xff8800ff  --orange

		local i = 1
		while names[i] do
			local tl = Session:new_audio_track (1, 2, nil, 1, names[i],
			                                    ARDOUR.PresentationInfo.max_order,
			                                    ARDOUR.TrackMode.Normal, true)

			for track in tl:iter () do
				track:presentation_info_ptr ():set_color (color)
			end

			i = i + 1
		end --foreach track

end end -- function factory


function icon (params) return function (ctx, width, height)
	local x = width * .5
	local y = height * .5
	local r = math.min (x, y) * .7
	ctx:save ()
	ctx:translate (x, y)
	ctx:scale (1, .5)
	ctx:translate (-x, -y)
	ctx:arc (x, y, r, 0, 2 * math.pi)
	ctx:set_source_rgba (.9, .9, 1, 1)
	ctx:fill ()
	ctx:arc (x, y, r, 0, math.pi)
	ctx:arc_negative (x, y * 1.6, r, math.pi, 0)
	ctx:set_source_rgba (.7, .7, .7, 1)
	ctx:fill ()
	ctx:restore ()

	ctx:set_source_rgba (.6, .4, .2, 1)
	ctx:translate (x, y)
	ctx:scale (.7, 1)
	ctx:translate (-x, -y)
	ctx:set_line_cap (Cairo.LineCap.Round)

	function drumstick (xp, lr)
		ctx:set_line_width (r * .3)
		ctx:move_to (x * xp, y)
		ctx:close_path ()
		ctx:stroke ()
		ctx:set_line_width (r * .2)
		ctx:move_to (x * xp, y)
		ctx:rel_line_to (lr * x, y)
		ctx:stroke ()
	end
	drumstick (1.2, 1.2)
	drumstick (0.7, -.5)
end end
