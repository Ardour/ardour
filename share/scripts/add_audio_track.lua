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
		tr = Session:new_audio_track (1, 2, nil, 1, "Audio", Editor:get_selection ().tracks:routelist ():front():presentation_info_ptr ():order () + 1, ARDOUR.TrackMode.Normal, true)
	else
		tr = Session:new_audio_track (1, 2, nil, 1, "Audio", -1, ARDOUR.TrackMode.Normal, true)
	end
	tr:front ():peak_meter ():set_meter_type (ARDOUR.MeterType.MeterKrms)

	Editor:access_action("Mixer", "select-none")
	repeat
		Editor:access_action("Editor", "select-next-route")
	until tr:front():is_selected()

	routeScrollIntoView(tr:front())
end end

-- Scrolls the mixer's route pane such that this route is visible. Typically this will make it the left-most visible
-- route but only if the available scrolling range allows it.
-- r: the route in question.
-- unhide: if the route is hidden then this function does nothing unless this parameter is true (or nil) in which case it unhides the route.
function routeScrollIntoView(r, unhide)

  if r:is_hidden() then
    if unhide ~= nil and unhide ~= true then return end
    Editor:show_track_in_display(routeToTav(r), true)
  end

  for i = 1, Session:get_stripables():size() do
    Editor:access_action("Mixer", "scroll-left")
  end

  local pos = routePosition(r)
  local numVisibleBeforeThis = 0

  for r2 in Session:get_routes():iter() do
    if routePosition(r2) < pos and not r2:is_hidden() then
      numVisibleBeforeThis = numVisibleBeforeThis + 1
    end
  end

  for i = 1, numVisibleBeforeThis do
    Editor:access_action("Mixer", "scroll-right")
  end

end

-- Returns the TimeAxisView for this route.
-- r: the route in question.
function routeToTav(r)
  return Editor:rtav_from_route(r):to_timeaxisview()
end

-- Returns strip position (presentation order), first strip = 0.
-- r: the route in question.
function routePosition(r)
  return r:presentation_info_ptr():order()
end