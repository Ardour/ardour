ardour {
	["type"]    = "EditorAction",
	name        = "Add Audio Track",
	license     = "MIT",
	author      = "Vincent Tassy",
	description = [[Adds an Audio Track after selection and make it selected]]
}

function factory () return function ()
	local sel = Editor:get_selection ()
	if not Editor:get_selection ():empty () and not Editor:get_selection ().tracks:routelist ():empty ()  then
		tr = Session:new_audio_track (1, 2, ARDOUR.RouteGroup(), 1, "Audio", Editor:get_selection ().tracks:routelist ():front():presentation_info_ptr ():order () + 1, ARDOUR.TrackMode.Normal, true)
	else
		tr = Session:new_audio_track (1, 2, ARDOUR.RouteGroup(), 1, "Audio", -1, ARDOUR.TrackMode.Normal, true)
	end

	if tr:empty() or tr:front ():isnil () then
		return
	end

	tr:front ():peak_meter ():set_meter_type (ARDOUR.MeterType.MeterKrms)

	--[[
	-- Not needed, new tracks are automatically selected.
	local tav = Editor:rtav_from_route(tr:front ()):to_stripabletimeaxisview ()
	Editor:set_selected_track (tav, ArdourUI.SelectionOp.Set, true);
	--]]

	tr = nil
	collectgarbage()
end end
