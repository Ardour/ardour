ardour {
	["type"]    = "EditorAction",
	name        = "Live Band Recording Session",
	description = [[
This template helps create the tracks for a typical pop/rock band.

You will be prompted to assemble your session from a list of track types.

Each track will be pre-assigned with a color.

Optionally, tracks may be assigned to sensible Groups ( vocals, guitars, drums )

Optionally, tracks may be assigned Gates and other plugins.
]]
}

function route_setup () return {} end

function factory () return function ()

    --prompt the user for the tracks they'd like to instantiate
	local dialog_options = {
		{ type = "heading", title = "Select the tracks you'd like\n to add to your session: " },

		{ type = "checkbox", key = "ldvox", default = false, title = "Lead Vocal" },

		{ type = "checkbox", key = "bass", default = false, title = "Bass" },

		{ type = "checkbox", key = "piano", default = false, title = "Piano" },
		{ type = "checkbox", key = "electric-piano", default = false, title = "Electric Piano" },
		{ type = "checkbox", key = "organ", default = false, title = "Organ" },

		{ type = "checkbox", key = "electric-guitar", default = false, title = "Electric Guitar" },
		{ type = "checkbox", key = "solo-guitar", default = false, title = "Lead Guitar" },
		{ type = "checkbox", key = "accoustic-guitar", default = false, title = "Acoustic Guitar" },

		{ type = "checkbox", key = "basic-kit", default = false, title = "Basic Drum Mics (Kick + Snare)" },
		{ type = "checkbox", key = "full-kit", default = false, title = "Full Drum Mics (Kick, Snare, HiHat, 3 Toms)" },
		{ type = "checkbox", key = "overkill-kit", default = false, title = "Overkill Drum Mics (Kick (2x), Snare(2x), HiHat, 3 Toms)" },

		{ type = "checkbox", key = "overhead-mono", default = false, title = "Drum OH (2 mono)" },
		{ type = "checkbox", key = "overhead-stereo", default = false, title = "Drum OH (Stereo)" },

		{ type = "checkbox", key = "room-mono", default = false, title = "Drum Room (Mono)" },
		{ type = "checkbox", key = "room-stereo", default = false, title = "Drum Room (Stereo)" },

		{ type = "checkbox", key = "bgvox", default = false, title = "Background Vocals (3x)" },

		{ type = "heading", title = "-------------------" },

		{ type = "checkbox", key = "group", default = false, title = "Group Track(s)?" },
		{ type = "checkbox", key = "gates", default = false, title = "Add Gate(s)?" },
		{ type = "checkbox", key = "char", default = false, title = "Add Character Plugin(s)?" },
	}

	local dlg = LuaDialog.Dialog ("Template Setup", dialog_options)
	local rv = dlg:run()
	if (not rv) then
		return
	end

	-- helper function to reference processors
	function processor(t, s) --takes a track (t) and a string (s) as arguments
		local i = 0
		local proc = t:nth_processor(i)
			repeat
				if ( proc:display_name() == s ) then
					return proc
				else
					i = i + 1
				end
				proc = t:nth_processor(i)
			until proc:isnil()
		end

	--INSTANTIATING MIDI TRACKS IS TOO DAMN HARD
	function create_midi_track(name, chan_count) -- call this function with a name argument and output channel count
		Session:new_midi_track(ARDOUR.ChanCount(ARDOUR.DataType ("midi"), 1),  ARDOUR.ChanCount(ARDOUR.DataType ("audio"), chan_count), true, ARDOUR.PluginInfo(), nil, nil, 1, name, 1, ARDOUR.TrackMode.Normal)
		return true
	end

	if rv['group'] then
		drum_group = Session:new_route_group("Drums")
		drum_group:set_rgba(0x425CADff)
		bass_group = Session:new_route_group("Bass")
		bass_group:set_rgba(0x1AE54Eff)
		guitar_group = Session:new_route_group("Guitars")
		guitar_group:set_rgba(0xB475CBff)
		key_group = Session:new_route_group("Keys")
		key_group:set_rgba(0xDA8032ff)
		vox_group = Session:new_route_group("Vox")
		vox_group:set_rgba(0xC54249ff)
	end

	local track_count = 0;
	if rv['basic-kit'] then
		local names = {"Kick", "Snare"}
		for i = 1, #names do
	    	local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				local gate = ARDOUR.LuaAPI.new_plugin(Session, "XT-EG Expander Gate (Mono)", ARDOUR.PluginType.LV2, "")
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then drum_group:add(track) end
				if rv['gates'] then track:add_processor_by_index(eg, 0, nil, true) end
			end
		end

		track_count = track_count+2
	end

	if rv['full-kit'] then
		local names = {"Kick", "Snare", "Hi-Hat", "Hi-tom", "Mid-tom", "Fl-tom"}
		for i = 1, #names do
			local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				local eg = ARDOUR.LuaAPI.new_plugin(Session, "XT-EG Expander Gate (Mono)", ARDOUR.PluginType.LV2, "")
				local tg = ARDOUR.LuaAPI.new_plugin(Session, "XT-TG Tom Gate (Mono)",      ARDOUR.PluginType.LV2, "")
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then drum_group:add(track) end
				if rv['gates'] then
					if string.find(track:name(), '-tom') then
						track:add_processor_by_index(tg, 0, nil, true)
					else
						track:add_processor_by_index(eg, 0, nil, true)
					end
				end
			end
		end

		track_count = track_count+6
	end

	if rv['overkill-kit'] then
		local names = {"Kick In", "Kick Out", "Snare Top", "Snare Btm", "Hi-Hat", "Hi-tom", "Mid-tom", "Fl-tom"}
		for i = 1, #names do
			local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				local eg = ARDOUR.LuaAPI.new_plugin(Session, "XT-EG Expander Gate (Mono)", ARDOUR.PluginType.LV2, "")
				local tg = ARDOUR.LuaAPI.new_plugin(Session, "XT-TG Tom Gate (Mono)",      ARDOUR.PluginType.LV2, "")
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then drum_group:add(track) end
				if rv['gates'] then
					if string.find(track:name(), '-tom') then
						track:add_processor_by_index(tg, 0, nil, true)
					else
						track:add_processor_by_index(eg, 0, nil, true)
					end
				end
			end
		end

		track_count = track_count+8
	end

	if rv['overhead-mono'] then
		local names = {"Drum OHL", "Drum OHR"}
		for i = 1, #names do
			local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then drum_group:add(track) end
			end
		end

		track_count = track_count+2
	end

	if rv['overhead-stereo'] then
		local names = {"Drum OH (st)"}
		for i = 1, #names do
			local tl = Session:new_audio_track (2, 2, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then drum_group:add(track) end
			end
		end

		track_count = track_count+2
	end

	if rv['room-mono'] then
		local names = {"Drum Room"}
		for i = 1, #names do
			local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then drum_group:add(track) end
			end
		end

		track_count = track_count+1
	end

	if rv['room-stereo'] then
		local names = {"Drum Room (st)"}
		for i = 1, #names do
			local tl = Session:new_audio_track (2, 2, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then drum_group:add(track) end
			end
		end

		track_count = track_count+2
	end

	if rv['bass'] then
		local names = {"Bass"}
		for i = 1, #names do
			local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				local bc = ARDOUR.LuaAPI.new_plugin(Session, "XT-BC Bass Character (Mono)", ARDOUR.PluginType.LV2, "")
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then bass_group:add(track) end
				if rv['char'] then track:add_processor_by_index(bc, 0, nil, true) end
			end
		end

		track_count = track_count+1
	end

	if rv['electric-guitar'] then
		local names = {"El Guitar"}
		for i = 1, #names do
			local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then guitar_group:add(track) end
			end
		end

		track_count = track_count+1
	end

	if rv['solo-guitar'] then
		local names = {"Ld Guitar"}
		for i = 1, #names do
			local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then guitar_group:add(track) end
			end
		end

		track_count = track_count+1
	end

	if rv['accoustic-guitar'] then
		local names = {"Ac Guitar"}
		for i = 1, #names do
			local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then guitar_group:add(track) end
			end
		end

		track_count = track_count+1
	end

	if rv['piano'] then
		local names = {"Piano"}
		for i = 1, #names do
			local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then key_group:add(track) end
			end
		end

		track_count = track_count+1
	end

	if rv['electric-piano'] then
		local names = {"E Piano"}
		for i = 1, #names do
			local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then key_group:add(track) end
			end
		end

		track_count = track_count+1
	end

	if rv['organ'] then
		local names = {"Organ"}
		for i = 1, #names do
			local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then key_group:add(track) end
			end
		end

		track_count = track_count+1
	end

	if rv['ldvox'] then
		local names = {"Vox"}
		for i = 1, #names do
			local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				local vc = ARDOUR.LuaAPI.new_plugin(Session, "XT-VC Vocal Character (Mono)", ARDOUR.PluginType.LV2, "")
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then vox_group:add(track) end
				if rv['char']  then track:add_processor_by_index(vc, 0, nil, true) end
			end
		end

		track_count = track_count+1
	end

	if rv['bgvox'] then
		local names = {"Bg. Vox 1", "Bg. Vox 2", "Bg. Vox 3"}
		for i = 1, #names do
			local tl = Session:new_audio_track (1, 1, nil, 1, names[i],  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
			for track in tl:iter() do
				--track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
				if rv['group'] then vox_group:add(track) end
			end
		end

		track_count = track_count+1
	end

    --determine the number of tracks we can record
	local e = Session:engine()
	local _, t = e:get_backend_ports ("", ARDOUR.DataType("audio"), ARDOUR.PortFlags.IsOutput | ARDOUR.PortFlags.IsPhysical, C.StringVector())  -- from the engine's POV readable/capture ports are "outputs"
	local num_inputs = t[4]:size();  -- table 't' holds argument references. t[4] is the C.StringVector (return value)

    --ToDo:  if track_count > num_inputs, we should warn the user to check their routing.

    --fit all tracks on the screen
    Editor:access_action("Editor","fit_all_tracks")

	Session:save_state("");
end end
