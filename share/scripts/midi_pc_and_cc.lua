ardour {
  ["type"]    = "dsp",
  name        = "MIDI PC/CC",
  category    = "Utility",
  author      = "Brent Baccala",
  description = [[Handle MIDI PC and CC messages

It only needs to be on one track to hear the MIDI Program Change (PC) and Continuous Controller (CC) messages.

It operates on all of the tracks based on commands set in their comment blocks.

Every track with "MIDI Program #" in its comments in activated when that PC is sent, and is deactivated when any other PC is sent

The program number can be comma-separated ranges, as in "MIDI Program 0-7,9"

Tracks can also be labeled "MIDI Bank # Program #", in which case both bank and program numbers have to match.

Each such track can have MIDI CC lines with arbitrary Lua code.

In such code, variables route (the current route) and value (the CC value) are defined.

The following are working MIDI CC lines.

Mute or unmute the current track:
MIDI CC 93: Session:set_control(route:mute_control(), (value == 0) and 1 or 0, PBD.GroupControlDisposition.NoGroup)

Program blocks are supported.  This makes CC 93 role transport:
MIDI CC 93: if value<40 then
MIDI CC 93:    Session:request_stop (false, false, ARDOUR.TransportRequestSource.TRS_UI)
MIDI CC 93: else
MIDI CC 93:    Session:request_locate(0, false, ARDOUR.LocateTransportDisposition.MustStop, ARDOUR.TransportRequestSource.TRS_UI)
MIDI CC 93:    Session:request_roll (ARDOUR.TransportRequestSource.TRS_UI)
MIDI CC 93: end

There's a convenience function to roll transport at a start time in seconds:
MIDI CC 93: roll_transport(value, 0)

You can also roll transport at a marker:
MIDI CC 93: roll_transport(value, "01")

Another convenience function to activate or deactivate a plugin:
MIDI CC 91: activate_processor_by_name(route, value, "Gigaverb")

A convenience function to select a preset:
MIDI CC 93: load_preset(route, "plugin", "preset")

Remember that a "MIDI Program #" (or "MIDI Bank # Program #") line is required for MIDI CC lines to be processed
]]
}

function dsp_ioconfig ()
  return { { midi_in = 1, midi_out = 1, audio_in = 0, audio_out = 0}, }
end

--
-- activate_processor_by_name(route, name)
--
-- a convenience function that enables or disables a plugin by name (a string)
-- route should be an Ardour Route object (not a string)

function get_processor_by_name(route, name)
   local i = 0;
   repeat
      local proc = route:nth_plugin (i) -- get Nth Ardour::Processor
      if (not proc:isnil()) and proc:display_name() == name then
         return proc
      end
      i = i + 1
   until proc:isnil()
   return nil
end

function activate_processor_by_name(route, value, name)
   local proc = get_processor_by_name(route, name)
   if not proc:isnil() then
      if value == 0 then
         proc:deactivate()
      else
         proc:activate()
      end
   end
end

--
-- load_preset(route, plugin, preset)
--
-- a convenience function to load a plugin's preset by name
-- route is an Ardour Route object
-- plugin and preset are strings

function load_preset(route, plugin, preset)
   local proc = get_processor_by_name(route, plugin)
   if not proc:isnil() then
      local pp = proc:to_insert():plugin(0)
      pp:load_preset(pp:preset_by_label(preset))
   end
end

-- roll_transport(value, location)
--
-- a convenience function to start or stop the transport
-- value is 0 to stop transport and not 0 to start transport
-- location is either a value in seconds or a string matching a marker

function string.starts(String,Start)
   return string.sub(String,1,string.len(Start))==Start
end

function roll_transport(value, location)
   if value==0 then
      Session:request_stop (false, false, ARDOUR.TransportRequestSource.TRS_UI)
   else
      -- default value of location is 0 (start of session)
      local start_sample
      if not location then
        start_sample = 0
      elseif type(location) == "string" then
        for loc in Session:locations():list():iter() do
          if loc:is_mark() then
            if string.starts(loc:name(), location) then
              start_sample = loc:start():samples();
            end
          end
        end
      else
        start_sample = Session:nominal_sample_rate() * location;
      end
      if start_sample then
        Session:request_locate(start_sample, false, ARDOUR.LocateTransportDisposition.MustStop, ARDOUR.TransportRequestSource.TRS_UI)
        Session:request_roll (ARDOUR.TransportRequestSource.TRS_UI)
      end
   end
end

-- the currently active functions on the Continuous Controllers
-- it's a list of pairs, indexed by CC number, each pair a Route object and a Lua function
CC_functions = { }

-- global variables to track current bank and program
bank_msb = 0
bank_lsb = 0
bank = 0
program = 0

-- return true/false if number (an integer) is in range (a string)
-- format of range is a comma-separated list of items, each either START-STOP or VALUE
function match_number_range(number, range)
    local fieldstart = 1
    local match = false
    range = range .. ','
    repeat
       local r,_,start,stop = string.find(range, "^(%d+)-(%d+)", fieldstart)
       if r and number >= tonumber(start) and number <= tonumber(stop) then
          match = true
       end
       r,_,value = string.find(range, "^(%d+)", fieldstart)
       if r and number == tonumber(value) then
          match = true
       end
       local nexti = string.find(range, ",", fieldstart)
       fieldstart = nexti + 1
    until fieldstart > string.len(range)
    return match
end

function program_change()
   -- Program Change or Bank Change message
   -- program and bank are global variables
   -- Parse/reparse the comment blocks
   -- Clear the global table of CC functions; it'll be recreated during the parse
   CC_functions = { }
   -- Run through all comment blocks on all routes looking for certain strings
   for route in Session:get_routes():iter() do
      local nextchar = 1
      local MIDI_Program_seen = false
      local route_comment = route:comment()
      local route_active = false
      local local_CC_functions = { }
      local program_functions = { }  -- to store functions to execute after track activation
      -- Look for "MIDI Bank # Program #: code" statements with function calls
      while true do
          local MIDI_Program_start, MIDI_Program_end, bank_list, program_list, code
          MIDI_Program_start,MIDI_Program_end,bank_list,program_list,code = string.find(route_comment, "MIDI Bank (%d[-%d,]*) Program (%d[-%d,]*): ([^\n]+)", nextchar)
          if bank_list then
              local matches = match_number_range(program, program_list) and match_number_range(bank, bank_list)
              route_active = route_active or matches
              MIDI_Program_seen = true
              if matches and code then
                  table.insert(program_functions, code)
              end
              nextchar = MIDI_Program_end + 1
          else
              break
          end
      end
      -- Look for "MIDI Bank # Program #" statements that match both the program number and the bank number
      nextchar = 1
      while true do
          local MIDI_Program_start, MIDI_Program_end, bank_list, program_list
          MIDI_Program_start,MIDI_Program_end,bank_list,program_list = string.find(route_comment, "MIDI Bank (%d[-%d,]*) Program (%d[-%d,]*)", nextchar)
          if bank_list then
              route_active = route_active or (match_number_range(program, program_list) and match_number_range(bank, bank_list))
              MIDI_Program_seen = true
              nextchar = MIDI_Program_end + 1
          else
              break
          end
      end
      -- Look for "MIDI Program #: code" statements with function calls
      nextchar = 1
      while true do
          local MIDI_Program_start, MIDI_Program_end, program_list, code
          MIDI_Program_start,MIDI_Program_end,program_list,code = string.find(route_comment, "MIDI Program (%d[-%d,]*): ([^\n]+)", nextchar)
          if program_list then
             local matches = match_number_range(program, program_list)
             route_active = route_active or matches
             MIDI_Program_seen = true
             if matches and code then
                 table.insert(program_functions, code)
             end
             nextchar = MIDI_Program_end + 1
          else
             break
          end
      end
      -- Look for "MIDI Program #" statements that match just the program number
      nextchar = 1
      while true do
          local MIDI_Program_start, MIDI_Program_end, program_list
          MIDI_Program_start,MIDI_Program_end,program_list = string.find(route_comment, "MIDI Program (%d[-%d,]*)", nextchar)
          if program_list then
             route_active = route_active or match_number_range(program, program_list)
             MIDI_Program_seen = true
             nextchar = MIDI_Program_end + 1
          else
             break
          end
      end
      -- if at least one of either kind of line was seen, there is further handling of this track
      if MIDI_Program_seen then
         -- all tracks with a "MIDI Program" line present are set either active or inactive, depending on if they matched
         -- all other tracks (no "MIDI Progam" line) are not affected (they never get to this point)
         route:set_active(route_active, nil);
         -- if this route is active, execute any program change functions and parse any CC functions in the route's comment block
         if route_active then
           -- execute program change functions after track activation
           for _, code in ipairs(program_functions) do
              local func, err = load("return function(route) " .. code .. " end")
              if func then
                  local ok, generated_func = pcall(func)
                  if ok then
                      local success, exec_err = pcall(generated_func, route)
                      if not success then
                          print("Program function execution error:", exec_err)
                      end
                  else
                      print("Program function generation error:", generated_func)
                  end
              else
                  print("Program function compilation error:", err)
              end
           end
           local nextchar = 1
           local local_CC_functions = { }
           -- we wrap the code inside "return function (route, value)" and "end"
           -- so that when we pcall it with no argument it returns a function that takes two arguments
           -- First parse "MIDI Bank # Program # CC #: code" patterns with bank and program restrictions
           while true do
              MIDI_CC_start, MIDI_CC_end, bank_list, program_list, CC_num, CC_program = string.find(route_comment, "MIDI Bank (%d[-%d,]*) Program (%d[-%d,]*) CC (%d+): ([^\n]+)", nextchar)
              if MIDI_CC_start == nil then break end
              local key = CC_num .. "|" .. bank_list .. "|" .. program_list
              if (local_CC_functions[key] == nil) then
                 local_CC_functions[key] = { code = "return function(route, value)\n", bank_list = bank_list, program_list = program_list, CC_num = CC_num }
              end
              local_CC_functions[key].code = local_CC_functions[key].code .. CC_program .. "\n"
              nextchar = MIDI_CC_end + 1
           end
           -- Then parse "MIDI Program # CC #: code" patterns with only program restrictions
           nextchar = 1
           while true do
              MIDI_CC_start, MIDI_CC_end, program_list, CC_num, CC_program = string.find(route_comment, "MIDI Program (%d[-%d,]*) CC (%d+): ([^\n]+)", nextchar)
              if MIDI_CC_start == nil then break end
              local key = CC_num .. "||" .. program_list
              if (local_CC_functions[key] == nil) then
                 local_CC_functions[key] = { code = "return function(route, value)\n", bank_list = nil, program_list = program_list, CC_num = CC_num }
              end
              local_CC_functions[key].code = local_CC_functions[key].code .. CC_program .. "\n"
              nextchar = MIDI_CC_end + 1
           end
           -- Finally parse "MIDI CC #: code" patterns without restrictions
           nextchar = 1
           while true do
              MIDI_CC_start, MIDI_CC_end, CC_num, CC_program = string.find(route_comment, "MIDI CC (%d+): ([^\n]+)", nextchar)
              if MIDI_CC_start == nil then break end
              if (local_CC_functions[CC_num] == nil) then
                 local_CC_functions[CC_num] = { code = "return function(route, value)\n", bank_list = nil, program_list = nil, CC_num = CC_num }
              end
              local_CC_functions[CC_num].code = local_CC_functions[CC_num].code .. CC_program .. "\n"
              nextchar = MIDI_CC_end + 1
           end
           -- done parsing (or at least gathering all of the lines together)
           -- now compile the CC functions and insert them in the global table
           for key, entry in pairs(local_CC_functions) do
              local code = entry.code .. "\nend"
              local generator, err = load(code)
              if generator then
                  local ok, func = pcall(generator)
                  if ok then
                      local cc_num = tonumber(entry.CC_num)
                      if not CC_functions[cc_num] then
                          CC_functions[cc_num] = { }
                      end
                      -- Store route, function, and program/bank restrictions
                      table.insert(CC_functions[cc_num], {
                          route = route,
                          func = func,
                          program_list = entry.program_list,
                          bank_list = entry.bank_list
                      })
                  else
                     print("Execution error:", func)
                  end
              else
                  print("Compilation error:", err)
              end
           end
         end
      end
   end
end

function process_midi_messages()
    for _,b in pairs (midiin) do
        local t = b["time"] -- t = [ 1 .. n_samples ]
        local d = b["data"] -- midi-event data
        local event_type
        local channel
        if #d == 0 then
            event_type = -1
            channel = -1
        else
            event_type = d[1] >> 4
            channel = d[1] & 15
        end
        if (event_type == 11) then
           -- Continuous Controller message
           local num = d[2]
           local val = d[3]
           -- handle Bank Select messages
           if num == 0 or num == 32 then
               if num == 0 then
                   bank_msb = val
               else
                   bank_lsb = val
               end
               bank = 128 * bank_msb + bank_lsb
               program_change()
           end
           -- if any functions are registered for this CC, call them with their respective Route objects and the value of the controller
           local lst = CC_functions[num]
           if lst then
             for _, entry in ipairs(lst) do
                   -- Check if program/bank restrictions match current program/bank
                   local program_match = true
                   local bank_match = true

                   if entry.program_list then
                       program_match = match_number_range(program, entry.program_list)
                   end
                   if entry.bank_list then
                       bank_match = match_number_range(bank, entry.bank_list)
                   end

                   -- Only execute if restrictions match (or no restrictions)
                   if program_match and bank_match then
                       entry.func(entry.route, val)
                   end
             end
           end
        end
        if (event_type == 12) then
           -- Program Change message
           program = d[2]
           program_change()
        end
    end
end

function dsp_run (_, _, n_samples)
    -- without pcall, any errors would crash Ardour; instead, print errors
    status, error = pcall(process_midi_messages)
    if not status then
        print(error)
    end
end
