ardour { ["type"] = "Snippet", name = "Dump Playlists" }

function factory () return function ()

	print ("Number of playlists:", Session:playlists():n_playlists())

	print ()
	print ("Used playlists:")
	for p in Session:playlists():get_used():iter() do
		print ("-", p:name(), p:n_regions())
	end

	print ()
	print ("Unused playlists:")
	for p in Session:playlists():get_unused():iter() do
		print ("-", p:name(), p:n_regions())
	end

	print ()
	print ("Playlists by Track:")
	for r in Session:get_tracks():iter() do
		print ("*", r:name())
		for p in Session:playlists():playlists_for_track (r:to_track()):iter() do
			if (p == r:to_track():playlist()) then
				print (" >-", p:name(), p:n_regions())
			else
				print ("  -", p:name(), p:n_regions())
			end
		end
	end
end end
