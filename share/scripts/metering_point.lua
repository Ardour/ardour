ardour {
    ["type"] = "EditorAction",
    name = "Meter Point",
    author = "Ardour Team",
    description = [[Batch change metering point for selected or tracks in the given session.]]
}

function factory () return function ()

	local dialog_options = {
		{ type = "label", colspan = 5, title = "" },
		{ type = "radio", col = 1, colspan = 7, key = "select", title = "", values ={ ["Set All: Input"] = ARDOUR.MeterPoint.MeterInput, ["Set All: Pre Fader"] = ARDOUR.MeterPoint.MeterPreFader, ["Set All: Post Fader"] = ARDOUR.MeterPoint.MeterPostFader, ["Set All: Output"] = ARDOUR.MeterPoint.MeterOutput, ["Set All: Custom"] = ARDOUR.MeterPoint.MeterCustom}, default = "Set All: Input"},
		{ type = "label", colspan = 5, title = "" },
		{ type = "checkbox", col=1, colspan = 1, key = "select-tracks", default = true, title = "Selected tracks only"},
		{ type = "checkbox", col=2, colspan = 1, key = "rec-tracks", default = true, title = "Record Enabled tracks only"},
		{ type = "label", colspan = 5, title = "" },
	}

	local rv = LuaDialog.Dialog("Change all Meter Taps:", dialog_options):run()
	if not rv then return end -- user cancelled

	local rl;
	if rv['select-tracks'] then
		rl = Editor:get_selection ().tracks:routelist ()
	else
		rl = Session:get_routes()
	end

	local meter_point = rv['select']

	for route in rl:iter() do
		if not(route:to_track():isnil()) then
			if rv['rec-tracks'] then
				if route:rec_enable_control():get_value() == 1.0 then
					route:to_track():set_meter_point(meter_point, false)
				end
			else
				route:to_track():set_meter_point(meter_point, false)
			end
		end
	end
end end
