ardour {
	["type"]    = "EditorAction",
	name        = "New Playlists",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Prompts and builds a new playlist for every track in the session. Beware the operation cannot be cancelled.]]
}

function factory () return function ()
	for r in Session:get_tracks():iter() do
		local rtav = Editor:rtav_from_route(r) -- lookup RTAV
		Editor:new_playlists(rtav:to_timeaxisview())
	end
end end
