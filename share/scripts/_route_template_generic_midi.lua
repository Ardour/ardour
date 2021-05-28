ardour {
	["type"]    = "EditorAction",
	name        = "Generic MIDI Track",
	description = [[Example]]
}

-- If a route_setup function is present in an Editor Action Script
-- the script is also listed in the "add track/bus" dialog as meta-template
--
-- The function is expected to return a Lua table. The table may be empty.
function route_setup ()
	return
	{
		-- keys control which AddRouteDialog controls are made sensitive.
		-- The following keys accept a default value to pre-seed the dialog.
		['how_many'] = 1,
		['name'] = 'MIDI',
		['channels'] = nil,
		['track_mode'] = nil,
		['strict_io'] = true,
		-- these keys just need to be set (to something other than nil)
		-- in order to set the control sensitives
		['insert_at'] = ARDOUR.PresentationInfo.max_order,
		['group'] = false, -- return value will be a RouteGroup*
		['instrument'] = true, -- return value will be a PluginInfoPtr
	}
end

-- The Script can be used as EditorAction in which case it can
-- optionally provide instantiation parmaters
function action_params ()
	return
	{
		['how_many']   = { title = "How Many tracks to add", default = "1" },
		["name"]       = { title = "Track Name Prefix", default = "MIDI" },
		["instrument"] = { title = "Add Instrument", default = "true" },
	}
end


function factory (params) return function ()
	-- When called from the AddRouteDialog, 'params' will be a table with
	-- keys as described in route_setup() above.

	local p          = params or route_setup ()
	local name       = p["name"] or 'Audio'
	local how_many   = p["how_many"] or 1
	local insert_at  = p["insert_at"] or ARDOUR.PresentationInfo.max_order;
	local group      = p["group"] or nil
	local strict_io  = p["strict_io"] or false
	local instrument = p["instrument"] or nil

	-- used in 'action-script mode'
	if instrument == "true" then
		instrument = ARDOUR.LuaAPI.new_plugin_info ("http://gareus.org/oss/lv2/gmsynth", ARDOUR.PluginType.LV2) -- general midi synth
		if instrument:isnil () then
			instrument = ARDOUR.LuaAPI.new_plugin_info ("https://community.ardour.org/node/7596", ARDOUR.PluginType.LV2) -- reasonable synth
		end
		if instrument:isnil () then
			LuaDialog.Message ("MIDI track add", "Cannot find instrument plugin",
				LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run ()
			return
		end
	end

	-- add no instrument
	if type (instrument) ~= "userdata" then
		instrument = ARDOUR.PluginInfo ()
	end

	Session:new_midi_track(
		ARDOUR.ChanCount(ARDOUR.DataType ("midi"), 1),
		ARDOUR.ChanCount(ARDOUR.DataType ("audio"), 2),
		strict_io,
		instrument, nil,
		group, how_many, name, insert_at, ARDOUR.TrackMode.Normal, true)

end end
