ardour {
  ["type"]    = "EditorAction",
  name        = "Create blank midi region clone",
  version     = "0.1.0",
  license     = "MIT",
  author      = "Daniel Appelt",
  description = [[Create a blank clone of a midi region]]
}

function factory () return function ()
  -- Get first selected region
  local regionList = Editor:get_selection().regions:regionlist()
  local region = regionList:front()

  -- Bail out if no region was selected
  if region:isnil() then
    LuaDialog.Message("Create blank midi region clone", "Please select a region first!",
      LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run()
    return
  end

  -- Get midi time axis view for region
  local rv = Editor:regionview_from_region(region)
  local tav = rv:get_time_axis_view()
  local mtav = tav:to_midi_time_axis_view()

  if mtav then
    local pos = region:position()
    local len = region:length()

    mtav:add_region(pos, len, true)
  end
end end
