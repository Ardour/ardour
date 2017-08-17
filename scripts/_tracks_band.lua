ardour {
	["type"]    = "TrackSetup",
	name        = "Live Band Tracks",
	description = [[
This template helps create the tracks for a typical pop/rock band.

You will be prompted to assemble your session from a list of track types.

Each track comes with its pre-assigned grouping, routing, EQ and plugins.
]]
}

function track_setup ()

	--prompt the user for the tracks they'd like to instantiate
	local dialog_options = {
		{ type = "heading", title = "Select the tracks you'd like\n to add to your session: " },

		{ type = "checkbox", key = "LeadVox", default = false, title = "Lead Vocal" },

		{ type = "checkbox", key = "Bass", default = false, title = "Bass" },

		{ type = "checkbox", key = "Piano", default = false, title = "Piano" },
		{ type = "checkbox", key = "E. Piano", default = false, title = "E. Piano" },
		{ type = "checkbox", key = "Organ", default = false, title = "Organ" },

		{ type = "checkbox", key = "ElecGuitar", default = false, title = "Electric Guitar" },
		{ type = "checkbox", key = "SoloGuitar", default = false, title = "Guitar Solo" },
		{ type = "checkbox", key = "AcousticGuitar", default = false, title = "Acoustic Guitar" },

		{ type = "checkbox", key = "basicDrums", default = false, title = "Basic Drum Mics (Kick + Snare)" },
		{ type = "checkbox", key = "fullDrums", default = false, title = "Full Drum Mics (Kick, Snare, HiHat, 3 Toms)" },
		{ type = "checkbox", key = "overDrums", default = false, title = "Overkill Drum Mics (Kick (2x), Snare(2x), HiHat, 3 Toms)" },

		{ type = "checkbox", key = "Drum O-Heads (2 mono)", default = false, title = "Drum O-Heads (2 mono)" },
		{ type = "checkbox", key = "Drum O-Heads (Stereo)", default = false, title = "Drum O-Heads (Stereo)" },

		{ type = "checkbox", key = "Room (Mono)", default = false, title = "Room (Mono)" },
		{ type = "checkbox", key = "Room (Stereo)", default = false, title = "Room (Stereo)" },

		{ type = "checkbox", key = "BGV", default = false, title = "Background Vocals (3x)" },
	}
	local dlg = LuaDialog.Dialog ("Template Setup", dialog_options)
	local rv = dlg:run()
	if (not rv) then
		return
	end

	local track_list = {}
	local channel_count = 0

	function add_track (io, name)
		local tl = Session:new_audio_track (io, io, nil, 1, name,  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
		for track in tl:iter() do
			table.insert (track_list, track)
			channel_count = channel_count + io
		end
	end

	-- for each selected item, create track(s), add plugins, etc
    
	if rv['Bass'] then
        add_track (1, "Bass")
		local trk = track_list.front()

		local color = 0xff8800ff  --orange
        trk:presentation_info_ptr ():set_color (color) 

	end

	if rv['Room (Stereo)'] then
		add_track (2, "Room (Stereo)")
	end

	-- TODO add others


	-- determine the number of physical inputs
	local e = Session:engine()
	-- from the engine's POV readable/capture ports are "outputs"
	local _, t = e:get_backend_ports ("", ARDOUR.DataType("audio"), ARDOUR.PortFlags.IsOutput | ARDOUR.PortFlags.IsPhysical, C.StringVector())
	-- table 't' holds argument references. t[4] is the C.StringVector (return value)
	local num_inputs = t[4]:size()

	if num_inputs < channel_count then
		-- warn the user if there are less physical inputs than created tracks
		LuaDialog.Message ("Session Creation", "Check your routing :)", LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run ()
	else

		-- otherwise record arm all created tracks
		for _, t in ipairs (track_list) do
			t:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
		end
	end

	--fit all tracks on the screen
	Editor:access_action("Editor","fit_all_tracks")

	Session:save_state("");
end
