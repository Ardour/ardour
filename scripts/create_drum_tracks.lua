
ardour {
	["type"] = "EditorAction",
	name = "Create Drum Tracks",
	author = "PSmith",
	description = [[Creates 8 new tracks with representative names and colors.]]
}

names = {
"Kick",
"Snare",
"Hat",
"Fl Tom",
"OH L",
"OH R",
"Room 1",
"Room 2"
}

color = 0xff8800ff  --orange

    
function factory (params)
	return function ()

		local i = 1
		while names[i] do
			Session:new_audio_track(1,2,RouteGroup,1,names[i],i,ARDOUR.TrackMode.Normal)

			track = Session:route_by_name(names[i])
			if (not track:isnil()) then
				trkinfo = track:presentation_info_ptr ()	
				trkinfo:set_color (color)
			end

			i = i + 1
		end --foreach track

	end  --function

end --factory
