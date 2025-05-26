ardour { ["type"] = "Snippet", name = "Add all audio file Sources to a new track" }

function factory () return function ()

	-- default track output channel count (= master bus input count)
	local n_chan_out  = 2
	if not Session:master_out():isnil() then
		n_chan_out = Session:master_out():n_inputs ():n_audio ()
	end

	-- create a stereo track
	local newtracks = Session:new_audio_track (2, n_chan_out, nil, 1, "All Audio Sources", ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal, true)
	-- and get playlist of the new track
	-- https://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Playlist
	local playlist = newtracks:front():playlist()

	-- the start of the session, first source is added there
	local position = Temporal.timepos_t(0)

	-- For each Source (AudioFileSource, MidiFileSource) Ardour
	-- creates a "whole file" Region to represent the Source.
	--
	-- So we get all regions for this session
	-- https://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:RegionFactory
	local rl = ARDOUR.RegionFactory.regions()

	-- ...and iterate over them
	for _, r in rl:iter() do
		-- only look for "sources", which are represented by "whole file regions"
		-- and filter by audio-regions
		if r:whole_file() and not r:to_audioregion():isnil() then
			print (r:name())
			-- add region to the track's playlist, this creates a copy of the region:
			playlist:add_region (r, position, 1, false)
			position = position + r:length ()
		end
	end

end end
