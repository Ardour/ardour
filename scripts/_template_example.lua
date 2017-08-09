--
-- Session Template setup-hook
--
-- If a script named 'template.lua' exists in a session-template folder
-- the `template_load` function of the script is called after
-- creating the session from the template.
--
-- (e.g. ~/.config/ardour5/templates/Template-Name/template.lua
--

function template_load ()
	local e = Session:engine()
	-- from the engine's POV readable/capture ports are "outputs"
	local _, t = e:get_backend_ports ("", ARDOUR.DataType("audio"), ARDOUR.PortFlags.IsOutput | ARDOUR.PortFlags.IsPhysical, C.StringVector())
	-- table 't' holds argument references. t[4] is the C.StringVector (return value)
	local tracks = t[4]:size();

	local dialog_options = {
		{ type = "heading", title = "Customize Session: " .. Session:name () },
		{ type = "number", key = "tracks", title = "Create Tracks",  min = 0, max = 128, step = 1, digits = 0, default = tracks },
		{ type = "checkbox", key = "recarm", default = false, title = "Record Arm Tracks" },
	}

	local dlg = LuaDialog.Dialog ("Template Setup", dialog_options)
	local rv = dlg:run()
	if (not rv or rv['tracks'] == 0) then
		return
	end

	-- create tracks
	local tl = Session:new_audio_track (1, 2, nil, rv['tracks'], "",  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
	-- and optionally record-arm them
	if rv['recarm'] then
		for track in tl:iter() do
			track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
		end
	end

	Session:save_state("");
end
