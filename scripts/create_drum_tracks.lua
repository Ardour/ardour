ardour {
	["type"] = "EditorAction",
	name = "Create Drum Tracks",
	author = "PSmith",
	description = [[Creates 8 new tracks with representative names and colors.]]
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
			                                    ARDOUR.TrackMode.Normal)

			for track in tl:iter () do
				track:presentation_info_ptr ():set_color (color)
			end

			i = i + 1
		end --foreach track

end end -- function factory
