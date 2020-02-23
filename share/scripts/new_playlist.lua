ardour {
	["type"]    = "EditorAction",
	name        = "New Playlist",
	license     = "MIT",
	author      = "Ardour Lua Taskforce",
	description = [[Prompts and builds a new playlist for every track in the session.]]
}

function factory () return function ()

	for r in Session:get_tracks():iter() do
		local rtav = Editor:rtav_from_route(r) -- lookup RTAV
		Editor:new_playlists(rtav:to_timeaxisview())
	end

collectgarbage()
end end
