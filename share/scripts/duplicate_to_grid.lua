ardour {
   ["type"]    = "EditorAction",
   name        = "Duplicate to grid",
   version     = "0.1.0",
   license     = "MIT",
   author      = "Daniel Appelt",
   description = [[Duplicate region to grid]]
}

function factory () return function ()
  -- Get first selected region
  local regionList = Editor:get_selection().regions:regionlist()
  local region = regionList:front()

  -- Bail out if no region was selected
  if region:isnil() then
    LuaDialog.Message("Duplicate to grid", "Please select a region first!",
      LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run()
    return
  end

  -- Create duplicate of region
  local playlist = region:playlist()
  local curPos = region:position()
  local curBeat = Editor:get_grid_type_as_beats(true, curPos)
  local nextPos = Temporal.timepos_t.from_ticks(curPos:ticks() + curBeat:to_ticks())
  -- gap could be used to create multiple duplicates at once
  local gap = Temporal.timecnt_t.from_ticks(curBeat:to_ticks())
  playlist:duplicate(region, nextPos, gap, 1)

  -- Change selection to duplicate to allow repeated application
  local selectionList = ArdourUI.SelectionList()
  region = playlist:find_next_region(curPos, ARDOUR.RegionPoint.Start, 1)
  local regionView = Editor:regionview_from_region(region)
  selectionList:push_back(regionView)
  Editor:set_selection(selectionList, ArdourUI.SelectionOp.Set)
end end
