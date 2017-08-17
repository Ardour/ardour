ardour {
	["type"]    = "TrackSetup",
	name        = "Route Test",
	description = [[ FOR TESTING AND PROTOTYING ONLY ]]
}

-- DON'T COUNT ON THIS TO REMAIN AS IS.
-- This may turn into a factory method, re-usable as ActionScript.
function session_setup ()
	Session:new_audio_track (1, 1, nil, 1, "Hello",  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
end
