ardour {
	["type"]    = "TrackSetup",
	name        = "Add tracks",
	description = [[
This template creates audio tracks.

You will be prompted for:
... the number of tracks to add
... the name of the tracks ( default: Audio %d )
... whether they are mono or stereo (default mono)
... whether to record-arm the tracks (default: no)
]]
}

function session_setup ()
	local e = Session:engine()
	-- from the engine's POV readable/capture ports are "outputs"
	local _, t = e:get_backend_ports ("", ARDOUR.DataType("audio"), ARDOUR.PortFlags.IsOutput | ARDOUR.PortFlags.IsPhysical, C.StringVector())
	-- table 't' holds argument references. t[4] is the C.StringVector (return value)
	local tracks = t[4]:size();

	local dialog_options = {
		{ type = "number", key = "tracks", title = "Create Tracks",  min = 1, max = 128, step = 1, digits = 0, default = tracks },
		{ type = "checkbox", key = "recarm", default = false, title = "Record Arm Tracks" },
	}

	local dlg = LuaDialog.Dialog ("Template Setup", dialog_options)
	local rv = dlg:run()
	if (not rv or rv['tracks'] == 0) then
		return
	end

	-- create tracks
	local tl = Session:new_audio_track (1, 1, nil, rv['tracks'], "",  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
	-- and optionally record-arm them
	if rv['recarm'] then
		for track in tl:iter() do
			track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
		end
	end
end
