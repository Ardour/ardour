ardour {
   ["type"]    = "dsp",
   name        = "Arpeggiator",
   category    = "Effect",
   author      = "Albert Gräf",
   license     = "MIT",
   description = [[simple_arp v0.3

Simple monophonic arpeggiator example with sample-accurate triggering, demonstrates how to process the new time_info data along with BBT info from Ardour's tempo map.
]]
}

-- Copyright (c) 2023 Albert Gräf, MIT License

-- The arpeggiator takes note input and constructs a new cyclic pattern each
-- time the input chord changes. Notes from the pattern are triggered at each
-- beat as transport is rolling. The plugin adjusts to the current time
-- signature, and also lets you subdivide the base pulse of the meter with a
-- control parameter in the setup. Note velocities for the different levels
-- can be adjusted in the setup as well.

-- NOTE: The scheme for varying note velocities in order to create rhythmic
-- accents is a bit on the simplistic side and only provides three distinct
-- velocity levels (bar, beat, and subdivision pulses). See barlow_arp.lua for
-- a more sophisticated implementation which uses Barlow's indispensability
-- formula.

-- The octave range can be adjusted up and down in the setup, notes from the
-- input chord are then repeated in the lower and/or upper octaves. The usual
-- pattern types are supported and can be selected in the setup: up, down,
-- up-down (exclusive and inclusive modes), order (notes are played in the
-- order in which they are input), and random.

-- The length of the notes can be set using the gate control as a fraction
-- (0..1 value) of the note division. The swing control lets you delay the
-- off-beat notes by varying amounts, given as a fraction ranging from 0.5 to
-- 0.75; a value of 0.5 produces a straight rhythm (no swing), 0.67 a triplet
-- feel.

-- A toggle in the setup lets you enable latch mode, in which the current
-- pattern keeps playing if you release all keys, until you start a new
-- chord. Another toggle enables sync mode, in which the pattern is properly
-- synchronized to bars and beats, no matter where you change chords. This
-- also works with patterns spanning multiple bars, and often creates a much
-- smoother arpeggio than just cycling through the pattern (which is the
-- default). Both latch and sync mode are especially helpful for imprecise
-- players (like me) who tend to miss beats in chord changes.

-- The bypass toggle, when engaged, suspends arpeggiator playback and sends
-- through the input notes as they are. This is intended to monitor the input
-- going into the arpeggiator, but can also be used as a performance tool.
-- (Disabling the arpeggiator plugin in Ardour has a similar effect, but
-- doesn't silence existing notes, which the bypass toggle does.)

-- All these parameters are plugin controls which can be automated and saved
-- in presets. Some factory presets are provided as well.

-- Last but not least, the plugin listens on all MIDI channels, and the last
-- MIDI channel used in the input also sets the MIDI channel for output. This
-- lets you play drumkits which expect their MIDI input on a certain MIDI
-- channel (usually channel 10), without having to fiddle with Ardour's MIDI
-- track parameters, provided that your MIDI controller can send data on the
-- appropriate MIDI channel.

function dsp_ioconfig ()
   return { { midi_in = 1, midi_out = 1, audio_in = -1, audio_out = -1}, }
end

function dsp_options ()
   return { time_info = true, regular_block_length = true }
end

function dsp_params ()
   return
      {
	 { type = "input", name = "Division", min = 1, max = 16, default = 1, integer = true, doc = "number of subdivisions of the beat" },
	 { type = "input", name = "Octave up", min = 0, max = 5, default = 0, integer = true, doc = "octave range up" },
	 { type = "input", name = "Octave down", min = 0, max = 5, default = 0, integer = true, doc = "octave range down" },
	 { type = "input", name = "Pattern", min = 1, max = 6, default = 1, integer = true, doc = "pattern style",
	   scalepoints =
	      {	["1 up"] = 1, ["2 down"] = 2, ["3 exclusive"] = 3, ["4 inclusive"] = 4, ["5 order"] = 5, ["6 random"] = 6 } },
	 { type = "input", name = "Velocity 1", min = 0, max = 127, default = 100, integer = true, doc = "velocity level (bar)" },
	 { type = "input", name = "Velocity 2", min = 0, max = 127, default = 80, integer = true, doc = "velocity level (beat)" },
	 { type = "input", name = "Velocity 3", min = 0, max = 127, default = 60, integer = true, doc = "velocity level (subdivision)" },
	 { type = "input", name = "Latch", min = 0, max = 1, default = 0, toggled = true, doc = "toggle latch mode" },
	 { type = "input", name = "Sync", min = 0, max = 1, default = 0, toggled = true, doc = "toggle sync mode" },
	 { type = "input", name = "Bypass", min = 0, max = 1, default = 0, toggled = true, doc = "bypass the arpeggiator, pass through input notes" },
	 { type = "input", name = "Gate", min = 0, max = 1, default = 1, doc = "gate as fraction of pulse length", scalepoints = { legato = 0 } },
	 { type = "input", name = "Swing", min = 0.5, max = 0.75, default = 0.5, doc = "swing factor (0.67 = triplet feel)" },
      }
end

function presets()
   -- just a few basic examples for now, we'll add more stuff here later
   return
      {
	 { name = "0 default", params = { Division = 1, ["Octave up"] = 0, ["Octave down"] = 0, Pattern = 1, ["Velocity 1"] = 100, ["Velocity 2"] = 80, ["Velocity 3"] = 60, Latch = 0, Sync = 0, Swing = 0.5, Gate = 1 } },
	 { name = "1 latch", params = { Latch = 1, Sync = 0 } },
	 { name = "2 latch and sync", params = { Latch = 1, Sync = 1 } },
	 { name = "3 bass", params = { Division = 1, ["Octave up"] = 0, ["Octave down"] = 1, Pattern = 1, Swing = 0.5, Gate = 1 } },
	 { name = "4 swing 60% #1 - synth", params = { Division = 2, ["Octave up"] = 1, ["Octave down"] = 1, Pattern = 3, Swing = 0.6, Gate = 1 } },
	 { name = "5 swing 60% #2 - drums", params = { Division = 2, ["Octave up"] = 0, ["Octave down"] = 0, Pattern = 1, Swing = 0.6, Gate = 1 } },
	 { name = "6 swing 66% #1 - synth", params = { Division = 2, ["Octave up"] = 1, ["Octave down"] = 1, Pattern = 3, Swing = 0.66, Gate = 1 } },
	 { name = "7 swing 66% #2 - drums", params = { Division = 2, ["Octave up"] = 0, ["Octave down"] = 0, Pattern = 1, Swing = 0.66, Gate = 1 } },
      }
end

-- debug level (1: print beat information in the log window, 2: also print the
-- current pattern whenever it changes, 3: also print note information, 4:
-- print everything)
local debug = 0

local chan = 0 -- MIDI output channel
local last_rolling -- last transport status, to detect changes
local last_beat, last_time -- last beat number and sample time
local last_num -- last note
local last_chan -- MIDI channel of last note
local last_gate -- off time of last note
local swing_time -- sample time of delayed pulse (swing)
local last_up, last_down, last_mode, last_sync, last_bypass -- previous params, to detect changes
local chord = {} -- current chord (note store)
local chord_index = 0 -- index of last chord note (0 if none)
local latched = {} -- latched notes
local pattern = {} -- current pattern
local index = 0 -- current pattern index (reset when pattern changes)

function dsp_run (_, _, n_samples)
   assert (type(midiout) == "table")
   assert (type(time) == "table")
   assert (type(midiout) == "table")

   local ctrl = CtrlPorts:array ()
   -- We need to make sure that these are integer values. (The GUI enforces
   -- this, but fractional values may occur through automation.)
   local subdiv, up, down, mode = math.floor(ctrl[1]), math.floor(ctrl[2]), math.floor(ctrl[3]), math.floor(ctrl[4])
   local vel1, vel2, vel3 = math.floor(ctrl[5]), math.floor(ctrl[6]), math.floor(ctrl[7])
   local latch = ctrl[8] > 0
   local sync = ctrl[9] > 0
   local bypass = ctrl[10] > 0
   local gate = ctrl[11]
   -- It seems customary to specify swing using a percentage (or fraction)
   -- where 50% = 1/2 denotes a straight rhythm (no swing) and 67% = 2/3 a
   -- triplet feel. Here we translate this to a swing factor which is
   -- multiplied with the note division time to give the timing of the
   -- off-beat notes.
   local swing = 1+2*(ctrl[12]-0.5)
   -- rolling state: It seems that we need to check the transport state (as
   -- given by Ardour's "transport finite state machine" = TFSM) here, even if
   -- the transport is not actually moving yet. Otherwise some input notes may
   -- errorneously slip through before playback really starts.
   local rolling = Session:transport_state_rolling ()
   local changed = false

   if up ~= last_up or down ~= last_down or mode ~= last_mode then
      last_up = up
      last_down = down
      last_mode = mode
      changed = true
   end

   if sync ~= last_sync then
      last_sync = sync
      index = 0
   end

   if not latch and next(latched) ~= nil then
      latched = {}
      changed = true
   end

   if swing == 1 then
      swing_time = nil
   end

   local all_notes_off = false
   if bypass ~= last_bypass then
      last_bypass = bypass
      all_notes_off = true
   end

   if last_rolling ~= rolling then
      last_rolling = rolling
      -- transport change, send all-notes off (we only do this when transport
      -- starts rolling, to silence any notes that may have been passed
      -- through beforehand; note that Ardour automatically sends
      -- all-notes-off to all MIDI channels anyway when transport is stopped)
      if rolling then
	 all_notes_off = true
      end
      swing_time = nil
   end

   local k = 1
   if all_notes_off then
      --print("all-notes-off", chan)
      midiout[k] = { time = 1, data = { 0xb0+chan, 123, 0 } }
      k = k+1
   end

   for _,ev in ipairs (midiin) do
      local status, num, val = table.unpack(ev.data)
      local ch = status & 0xf
      status = status & 0xf0
      if not rolling or bypass then
	 -- arpeggiator is just listening, pass through all MIDI data
	 midiout[k] = ev
	 k = k+1
      elseif status >= 0xb0 then
	 -- arpeggiator is playing, pass through all MIDI data that's not
	 -- note-related, i.e., control change, program change, channel
	 -- pressure, pitch wheel, and system messages
	 midiout[k] = ev
	 k = k+1
      end
      if status == 0x80 or status == 0x90 and val == 0 then
	 if debug >= 4 then
	    print("note off", num, val)
	 end
	 -- keep track of latched notes
	 if latch then
	    latched[num] = chord[num]
	 else
	    changed = true
	 end
	 chord[num] = nil
      elseif status == 0x90 then
	 if debug >= 4 then
	    print("note on", num, val, "ch", ch)
	 end
	 if latch and next(chord) == nil then
	    -- new pattern, get rid of latched notes
	    latched = {}
	 end
	 chord_index = chord_index+1
	 chord[num] = chord_index
	 if latch and latched[num] then
	    -- avoid double notes in latch mode
	    latched[num] = nil
	 else
	    changed = true
	 end
	 chan = ch
      elseif status == 0xb0 and num == 123 and ch == chan then
	 if debug >= 4 then
	    print("all notes off")
	 end
	 chord = {}
	 latched = {}
	 changed = true
      end
   end
   if changed then
      -- update the pattern
      pattern = {}
      function pattern_from_chord(pattern, chord)
	 for num, val in pairs(chord) do
	    table.insert(pattern, num)
	    for i = 1, down do
	       if num-i*12 >= 0 then
		  table.insert(pattern, num-i*12)
	       end
	    end
	    for i = 1, up do
	       if num+i*12 <= 127 then
		  table.insert(pattern, num+i*12)
	       end
	    end
	 end
      end
      pattern_from_chord(pattern, chord)
      if latch then
	 -- add any latched notes
	 pattern_from_chord(pattern, latched)
      end
      table.sort(pattern) -- order by ascending notes (up pattern)
      local n = #pattern
      if n > 0 then
	 if mode == 2 then
	    -- down pattern, reverse the list
	    table.sort(pattern, function(a,b) return a > b end)
	 elseif mode == 3 then
	    -- add the reversal of the list excluding the last element
	    for i = 1, n-2 do
	       table.insert(pattern, pattern[n-i])
	    end
	 elseif mode == 4 then
	    -- add the reversal of the list including the last element
	    for i = 1, n-1 do
	       table.insert(pattern, pattern[n-i+1])
	    end
	 elseif mode == 5 then
	    -- order the pattern by chord indices
	    local k = chord_index+1
	    local idx = {}
	    -- build a table of indices which also includes octaves up and
	    -- down, ordering them first by octave and then by index
	    function index_from_chord(idx, chord)
	       for num, val in pairs(chord) do
		  for i = 1, down do
		     if num-i*12 >= 0 then
			idx[num-i*12] = val - i*k
		     end
		  end
		  idx[num] = val
		  for i = 1, up do
		     if num+i*12 <= 127 then
			idx[num+i*12] = val + i*k
		     end
		  end
	       end
	    end
	    index_from_chord(idx, chord)
	    if latch then
	       index_from_chord(idx, latched)
	    end
	    table.sort(pattern, function(a,b) return idx[a] < idx[b] end)
	 elseif mode == 6 then
	    -- random order
	    for i = n, 2, -1 do
	       local j = math.random(i)
	       pattern[i], pattern[j] = pattern[j], pattern[i]
	    end
	 end
	 if debug >= 2 then
	    local s = "pattern:"
	    for i, num in ipairs(pattern) do
	       s = s .. " " .. num
	    end
	    print(s)
	 end
	 index = 0 -- reset pattern to the start
      else
	 chord_index = 0 -- pattern is empty, reset the chord index
	 if debug >= 2 then
	    print("pattern: <empty>")
	 end
      end
   end

   if rolling and not bypass then
      -- transport is rolling, not bypassed, so the arpeggiator is playing
      if last_gate and last_num and
	 last_gate >= time.sample and last_gate < time.sample_end then
	 -- Gated notes don't normally fall on a beat, so we detect them
	 -- here. (If the gate time hasn't been set or we miss it, then the
	 -- note-off will be taken care of when the next note gets triggered,
	 -- see below.)
	 if debug >= 3 then
	    print("note off", last_num)
	 end
	 -- sample-accurate "off" time
	 local ts = last_gate - time.sample + 1
	 midiout[k] = { time = ts, data = { 0x80+last_chan, last_num, 100 } }
	 last_num = nil
	 k = k+1
      end
      -- Check whether a beat is due, so that we trigger the next note. We
      -- want to do this in a sample-accurate manner in order to avoid jitter,
      -- which makes things a little complicated.  There are three cases to
      -- consider here:
      -- (1) Transport just started rolling or the playhead moved for some
      -- reason, in which case we *must* output the note immediately in order
      -- to not miss a beat (even if we're a bit late).
      -- (2) The beat occurs exactly at the beginning of a processing cycle,
      -- so we output the note immediately.
      -- (3) The beat happens some time during the cycle, in which case we
      -- calculate the sample at which the note is due.
      local denom = time.ts_denominator * subdiv
      -- beat numbers at start and end, scaled by base pulses and subdivisions
      local b1, b2 = denom/4*time.beat, denom/4*time.beat_end
      -- integral part of these
      local bf1, bf2 = math.floor(b1), math.floor(b2)
      -- sample times at start and end
      local s1, s2 = time.sample, time.sample_end
      -- current (nominal, i.e., unscaled) beat number, and its sample time
      local bt, ts
      if last_time and time.sample < last_time then
	 -- wrap-around (probably during a loop)
	 swing_time = nil
      end
      if swing_time and swing_time >= time.sample then
	 if swing_time < time.sample_end then
	    bt, ts = time.beat, swing_time
	 end
      elseif last_beat ~= math.floor(time.beat) or bf1 == b1 then
	 -- sudden jump in transport => next beat is due immediately
	 bt, ts = time.beat, time.sample
      elseif bf2 > bf1 and bf2 ~= b2 then
	 -- next beat is due some time in this cycle (we're assuming contant
	 -- tempo here, hence this number may be off in case the tempo is
	 -- changing very quickly during the cycle -- so don't do that)
	 local d = math.ceil((b2-bf2)/(b2-b1)*(s2-s1))
	 assert(d > 0)
	 bt, ts = time.beat_end, time.sample_end - d
      end
      if ts then
	 -- save the last nominal beat so that we can detect sudden changes of
	 -- the playhead later (e.g., when transport starts rolling, or at the
	 -- end of a loop when the playhead wraps around to the beginning)
	 last_beat = math.floor(bt)
	 -- same for sample time, to detect wrap-around
	 last_time = time.sample
	 -- get the tempo map information
	 local tm = Temporal.TempoMap.read ()
	 local pos = Temporal.timepos_t (ts)
	 local bbt = tm:bbt_at (pos)
	 local meter = tm:meter_at (pos)
	 local tempo = tm:tempo_at (pos)
	 -- duration of this step
	 local dur = tm:bbt_duration_at(pos, Temporal.BBT_Offset(0,1,0)):samples() / subdiv
	 -- next note offset in swing mode
	 local swing_dur = math.floor(dur * swing)
	 local swing_ts = ts + swing_dur
	 -- calculate the note-off time in samples, this is used if the gate
	 -- control is neither 0 nor 1
	 local gate_dur = math.floor(dur * gate)
	 -- adjust the gate duration for swing
	 if swing > 1 then
	    if swing_time then
	       gate_dur = gate_dur - math.floor(dur * (swing-1) * gate)
	    else
	       gate_dur = gate_dur + math.floor(dur * (swing-1) * gate)
	    end
	 end
	 local gate_ts = ts + gate_dur
	 local n = #pattern
	 ts = ts - time.sample + 1
	 if debug >= 1 then
	    -- print some debugging information: bbt, fractional beat number,
	    -- sample offset, current meter, current tempo
	    print (string.format("%s - %g [%d] - %d/%d - %g bpm", bbt:str(),
				 math.floor(denom*bt)/denom, ts-1,
				 meter:divisions_per_bar(), meter:note_value(),
				 tempo:quarter_notes_per_minute()))
	 end
	 if last_num then
	    -- kill the old note
	    if debug >= 3 then
	       print("note off", last_num)
	    end
	    midiout[k] = { time = ts, data = { 0x80+last_chan, last_num, 100 } }
	    last_num = nil
	    k = k+1
	 end
	 if n > 0 then
	    -- calculate a fractional pulse number from the current bbt
	    local p = bbt.beats-1 + math.max(0, bbt.ticks) / Temporal.ticks_per_beat
	    -- Calculate a basic velocity pattern: by default, 100 for the
	    -- first beat in a bar, 80 for the other non-fractional beats, 60
	    -- for everything else (subdivision pulses). These values can be
	    -- changed with the corresponding control. NOTE: There are much
	    -- more sophisticted ways to do this, but we try to keep things
	    -- simple here.
	    local v = vel3
	    if p == 0 then
	       v = vel1
	    elseif p == math.floor(p) then
	       v = vel2
	    end
	    --print("p", p, "v", v)
	    -- trigger the new note
	    if sync then
	       -- sync pattern to the bbt
	       local mdiv = meter:divisions_per_bar()
	       local npulses = mdiv * subdiv
	       local l = #pattern
	       local k = math.floor(p*subdiv) -- current index in bar
	       local n = math.floor(l/npulses) -- bars in pattern
	       if n > 0 then
		  k = k + index*npulses
		  if (k+1) % npulses == 0  then
		     -- next bar
		     index = (index+1) % n
		  end
	       end
	       num = pattern[k%l+1]
	    else
	       index = index%n + 1
	       num = pattern[index]
	    end
	    if debug >= 3 then
	       print("note on", num, v)
	    end
	    midiout[k] = { time = ts, data = { 0x90+chan, num, v } }
	    last_num = num
	    last_chan = chan
	    -- we take a very small gate value (close to 0) to mean legato
	    -- instead, which means the same as a 1 gate value here
	    local legato =  gate_ts < time.sample_end
	    if gate < 1 and not legato then
	       -- Set the sample time at which the note-off is due.
	       last_gate = gate_ts
	    else
	       -- Otherwise don't set the off time in which case the
	       -- note-off gets triggered automatically above.
	       last_gate = nil
	    end
	    if swing_time or swing == 1 then
	       swing_time = nil
	    else
	       if debug >= 4 then
		  print("swing", swing_dur)
	       end
	       swing_time = swing_ts
	    end
	 end
      end
   else
      -- transport not rolling or bypass; reset all cached status information
      last_beat, last_time = nil, nil
      swing_time = nil
   end

   if debug >= 1 and #midiout > 0 then
      -- monitor memory usage of the Lua interpreter
      print(string.format("mem: %0.2f KB", collectgarbage("count")))
   end

end
