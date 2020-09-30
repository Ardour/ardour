ardour {
    ["type"] = "EditorAction",
    name = "Track Organizer",
    author = "Mixbus Team",
    description = [[Easily modifiable session overview and track property editor]]
}

function factory () return function ()

    local rbow = { ["----"] = "", ["Red"] = 0xD10000FF, ["Orange"] = 0xFF6622FF, ["Yellow"] = 0xFFDA21FF,
    ["Green"] = 0x33DD00FF, ["Blue"] = 0x1133CCFF, ["Indigo"] = 0x220066FF, ["Violet"] = 0x330044FF
}

   --now  starting to build our dialog
   local dialog_options = {
      { type = "label", colspan="4", title = "Change your Track settings here:" },
      { type = "heading", title = "Track",   col = 0, colspan = 1 },
      { type = "heading", title = "Group",   col = 1, colspan = 1 },
      { type = "heading", title = "Comment", col = 2, colspan = 1 },
      { type = "heading", title = "Color",   col = 3, colspan = 1 },
   }

   --group option payload
   --@ToDo: Add 'fake' groups for people to select, create them if they want it
   local pl = {["----"]   = "", ["Drums"] = "Drums", ["Bass"] = "Bass", ["Guitars"] = "Guitars",
   ["Keys"] = "Keys", ["Strings"] = "Strings", ["Vox"] = "Vox"
}

   for g in Session:route_groups():iter() do
       pl[g:name()] = g
   end

   --helper function to find default group option
   function interrogate(t)
       local v = "----"
       for g in Session:route_groups():iter() do
           for r in g:route_list():iter() do
               if r:name() == t:name() then v = g:name() end
           end
       end return v
   end

   function find_color(t)
       local c = "----"
       for k, v in pairs(rbow) do
           if(t:presentation_info_ptr():color() == v) then c = k end
       end return c
   end

   --insert an entry into our dialog_options table for each track with appropriate info
   for t in Session:get_tracks():iter() do
       table.insert(dialog_options, {
           type = "entry",    key = t:name() .. ' n',  col = 0, colspan = 1, default = t:name(), title = "" --@ToDo: Shorten Names so they can still see what track they're changing?
       }) --name
       table.insert(dialog_options, {
           type = "dropdown", key = t:name() .. ' g',  col = 1, colspan = 1, title = "", values = pl, default = interrogate(t)
       }) --group
       table.insert(dialog_options, {
           type = "entry",    key = t:name() .. ' cm', col = 2, colspan = 1, default = t:comment(), title = ""
       }) --comment
       table.insert(dialog_options, {
           type = "dropdown", key = t:name() .. ' c',  col = 3, colspan = 1, title = "", values = rbow, default = find_color(t)
       }) --color
   end

   --run dialog_options
   local rv = LuaDialog.Dialog("Track Organizer", dialog_options):run()
   if not(rv) then goto script_end end
   assert(rv, 'Dialog box was cancelled or is ' .. type(rv))

   --begin track operation
   for t in Session:get_tracks():iter() do
       local cgrp = interrogate(t)
       local name = rv[t:name() .. ' n' ]
       local ngrp = rv[t:name() .. ' g' ]
       local cmnt = rv[t:name() .. ' cm']
       local colr = rv[t:name() .. ' c' ]

       if t:name() ~= name    then t:set_name(name)         end

       if t:comment() ~= cmnt then t:set_comment(cmnt, nil) end

       if not(colr == "") then t:presentation_info_ptr():set_color(colr) end

       if type(ngrp) == "userdata" then
           if cgrp ~= ngrp:name() then
               ngrp:add(t)
           end
       end

       if type(ngrp) == "string" and not(ngrp == "") then
           ngrp = Session:new_route_group(ngrp)
           if cgrp ~= ngrp:name() then
               ngrp:add(t)
           end
       end
   end
	::script_end::
end end
