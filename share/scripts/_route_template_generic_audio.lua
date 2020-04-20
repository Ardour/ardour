ardour {
	["type"]    = "EditorAction",
	name        = "Generic Audio Track",
	description = [[Add Audio tracks, by default as many as there are physical inputs]]
}

-- If a route_setup function is present in an Editor Action Script
-- the script is also listed in the "add track/bus" dialog as meta-template
--
-- The function is expected to return a Lua table. The table may be empty.
function route_setup ()
	local e = Session:engine()
	local _, t = e:get_backend_ports ("", ARDOUR.DataType("audio"), ARDOUR.PortFlags.IsOutput | ARDOUR.PortFlags.IsPhysical, C.StringVector())
	return
	{
		-- keys control which AddRouteDialog controls are made sensitive.
		-- The following keys accept a default value to pre-seed the dialog.
		['how_many'] = t[4]:size(),
		['name'] = 'Audio',
		['channels'] = 2,
		['track_mode'] = ARDOUR.TrackMode.Normal,
		['strict_io'] = true,
		-- these keys just need to be set (to something other than nil)
		-- in order to set the control sensitives
		['insert_at'] = ARDOUR.PresentationInfo.max_order,
		['group'] = false, -- return value will be a RouteGroup*
		['instrument'] = nil, -- return value will be a PluginInfoPtr
	}
end

-- The Script can be used as EditorAction in which case it *could*
-- optionally provide instantiation parmaters..
--[[
function action_params ()
	return
	{
		['how_many'] = { title = "How Many tracks to add", default = "1" },
		["name"]     = { title = "Track Name Prefix", default = "Audio" },
	}
end
--]]


function factory (p)
	-- when used from the AddRouteDialog (or w/action_params)
	if type (p) == 'table' and p['how_many'] ~= nil then
		return function ()
			-- When called from the AddRouteDialog, 'p' will be a table with
			-- keys as described in route_setup() above.
			local name      = p["name"] or 'Audio'
			local how_many  = p["how_many"] or 1
			local channels  = p["channels"] or 1
			local insert_at = p["insert_at"] or ARDOUR.PresentationInfo.max_order;
			local group     = p["group"] or nil
			local mode      = p["track_mode"] or ARDOUR.TrackMode.Normal
			local strict_io = p["strict_io"] or false
			local chan_out  = 0

			if ARDOUR.config():get_output_auto_connect() == ARDOUR.AutoConnectOption.AutoConnectMaster then
				if not Session:master_out():isnil() then
					chan_out = Session:master_out():n_inputs ():n_audio ()
				end
			end

			if chan_out == 0 then
				chan_out = channels;
			end

			local tl = Session:new_audio_track (channels, chan_out, group, how_many, name, insert_at, mode, true)

			if strict_io then
				for t in tl:iter() do
					t:set_strict_io (true)
				end
			end
		end
	end
	-- when used as standalone (no action parameters): interactive
	return function ()
		local e = Session:engine()
		local _, t = e:get_backend_ports ("", ARDOUR.DataType("audio"), ARDOUR.PortFlags.IsOutput | ARDOUR.PortFlags.IsPhysical, C.StringVector())
		local tracks = t[4]:size();

		local dialog_options = {
			{ type = "number",   key = "tracks", title = "Create Tracks",  min = 1, max = 128, step = 1, digits = 0, default = tracks },
			{ type = "entry",    key = "name",   default = 'Audio', title = "Name Prefix" },
			{ type = "checkbox", key = "stereo", default = false, title = "Stereo" },
			{ type = "checkbox", key = "recarm", default = false, title = "Record Arm Tracks" },
		}

		local dlg = LuaDialog.Dialog ("Create Audio Tracks", dialog_options)
		local rv = dlg:run()
		if (not rv or rv['tracks'] == 0) then
			return
		end

		local chan_in = stereo and 2 or 1
		local chan_out  = 0

		if ARDOUR.config():get_output_auto_connect() == ARDOUR.AutoConnectOption.AutoConnectMaster then
			if not Session:master_out():isnil() then
				chan_out = Session:master_out():n_inputs ():n_audio ()
			end
		end

		if chan_out == 0 then
			chan_out = chan_in;
		end

		-- create tracks
		local tl = Session:new_audio_track (chan_in, chan_out, nil, rv['tracks'], "", ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)
		-- and optionally record-arm them
		if rv['recarm'] then
			for track in tl:iter() do
				track:rec_enable_control ():set_value (1, PBD.GroupControlDisposition.NoGroup)
			end
		end
	end
end
