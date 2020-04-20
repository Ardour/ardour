ardour {
	["type"]    = "SessionInit",
	name        = "Recording Session",
	description = [[Add as many mono tracks to the new session as there are physical audio inputs and optionally record-arm them.]]
}

---- For use with templates: Session Template setup-hook
--
-- If a script named 'template.lua' exists in a session-template folder
-- the function produced by the 'factory' function of the script is called
-- once after creating the session from the template.
--
-- (e.g. ~/.config/ardour5/templates/Template-Name/template.lua)
--
--
---- For use as meta-session (specic session-setup scripts)
--
-- Every Lua script in the script-folder of type "SessionInit"
-- is listed as implicit template in the new-session dialog.
-- The function produced by the scripts `factory` function is called
-- once after creating a new, empty session.
--
---- For use as meta-session (general purpose Actions)
--
-- In some cases normal action scripts can also serve as session-setup
-- To include those ActionScripts in the template-list the script needs
-- to implement an additional function
--      function session_setup () return true end;
-- The script's factory will be called without any parameters

function factory () return function ()
	local e = Session:engine()
	-- from the engine's POV readable/capture ports are "outputs"
	local _, t = e:get_backend_ports ("", ARDOUR.DataType("audio"), ARDOUR.PortFlags.IsOutput | ARDOUR.PortFlags.IsPhysical, C.StringVector())
	-- table 't' holds argument references. t[4] is the C.StringVector (return value)
	local tracks = t[4]:size();

	local dialog_options = {
		{ type = "heading", title = "Customize Session: " .. Session:name () },
		{ type = "number", key = "tracks", title = "Create Tracks",  min = 1, max = 128, step = 1, digits = 0, default = tracks },
		{ type = "checkbox", key = "recarm", default = false, title = "Record Arm Tracks" },
	}

	local dlg = LuaDialog.Dialog ("Template Setup", dialog_options)
	local rv = dlg:run()
	if (not rv or rv['tracks'] == 0) then
		return
	end

	-- create tracks
	local tl = Session:new_audio_track (1, 2, nil, rv['tracks'], "",  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal, true)
	-- and optionally record-arm them
	if rv['recarm'] then
		for track in tl:iter() do
			track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
		end
	end

	Session:save_state("");
end end
