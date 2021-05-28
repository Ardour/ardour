ardour {
	["type"]    = "EditorAction",
	name        = "Rob's 16 MIDI Trick Pony",
	description = [[clearly broken approach to go about things]]
}

function route_setup ()
	return {
		['Insert_at'] = ARDOUR.PresentationInfo.max_order,
		['name'] = 'Sweet16',
		['group'] = false, -- return value will be a RouteGroup* or nil
	}
end

function factory (p) return function ()
	local name      = "Sweet16"
	local insert_at = ARDOUR.PresentationInfo.max_order
	local group     = nil

	-- check for "MIDI Channel Map" LV2 from x42 midifilters.lv2
	if ARDOUR.LuaAPI.new_plugin_info ("http://gareus.org/oss/lv2/midifilter#channelmap", ARDOUR.PluginType.LV2):isnil () then
		LuaDialog.Message ("16 MIDI Tracks", "Error: Plugin 'MIDI Simple Channel Map' was not found.", LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run ()
		return
	end

	if type (p) == 'table' and p['how_many'] ~= nil then
		-- used from the AddRouteDialog (or w/action_params)
		name      = p["name"] or 'Sweet16'
		insert_at = p["insert_at"] or ARDOUR.PresentationInfo.max_order;
		group     = p["group"] or nil
	else
		-- used standalone, prompt for name and insert position
		local dialog_options = {
			{ type = "entry", key = "name", default = 'Sweet16', title = "Name Prefix" },
			{ type = "entry", key = "group", default = '', title = "Group (empty for none)" },
			{ type = "dropdown", key = "insertpos", title = "Position", default = "Last", values =
				{
					["First"]            = ArdourUI.InsertAt.First,
					["Before Selection"] = ArdourUI.InsertAt.BeforeSelection,
					["After Selection"]  = ArdourUI.InsertAt.AfterSelection,
					["Last"]             = ArdourUI.InsertAt.Last
				}
			}
		}

		local od = LuaDialog.Dialog ("16 MIDI Tracks", dialog_options)
		local rv = od:run()
		if (not rv) then return end
		name = rv['name'] or 'Sweet16'
		if rv['insertpos'] then
			insert_at = ArdourUI.translate_order (rv['insertpos'])
		end
		if rv['group'] and rv['group'] ~= '' then
			group = Session:new_route_group (rv['group'])
		end
	end
	collectgarbage ()

	-- all systems go

	local tl = Session:new_midi_track (
		ARDOUR.ChanCount(ARDOUR.DataType ("midi"), 1),
		ARDOUR.ChanCount(ARDOUR.DataType ("midi"), 1),
		true, -- strict i/o
		ARDOUR.PluginInfo(), nil, -- no instrument, no instrument preset
		group,
		16, -- how many
		name, insert_at, ARDOUR.TrackMode.Normal, true)

	local i = 1
	for track in tl:iter() do
		local p = ARDOUR.LuaAPI.new_plugin(Session, "http://gareus.org/oss/lv2/midifilter#channelmap", ARDOUR.PluginType.LV2, "")
		assert (not p:isnil ())
		track:add_processor_by_index(p, 0, nil, true)
		for j = 1, 16 do
			ARDOUR.LuaAPI.set_processor_param (p, j, i)
		end
		i = i + 1
	end
end end
