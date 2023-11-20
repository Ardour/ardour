ardour {
   ["type"]    = "dsp",
   name        = "Arpeggiator (Barlow)",
   category    = "Effect",
   author      = "Albert Gräf",
   license     = "GPL",
   description = [[barlow_arp v0.3

Simple monophonic arpeggiator example with sample-accurate triggering and velocities computed using Barlow's indispensability formula. This automatically adjusts to the current time signature and division to produce rhythmic accents in accordance with the meter by varying the note velocities in a given range.

In memory of Clarence Barlow (27 December 1945 – 29 June 2023).
]]
}

-- Copyright (c) 2023 Albert Gräf, GPLv3+

-- This is basically the same as simple_arp.lua (which see), but computes note
-- velocities using the Barlow indispensability formula which produces more
-- detailed rhythmic accents and handles arbitrary time signatures with ease.
-- It also offers a pulse filter which lets you filter notes by normalized
-- pulse strengths. Any pulse with a strength below/above the given
-- minimum/maximum values in the 0-1 range will be skipped. In this case you
-- can set the gate value to 0 a.k.a. "legato" to have notes extend over
-- skipped steps until the next note arrives. This only makes an audible
-- difference if the pulse filter is in effect, otherwise a gate value of 0
-- has effectively the same meaning as 1.

-- NOTE: A limitation of the present algorithm is that only subdivisions <= 7
-- (a.k.a. septuplets) are supported, but if you really need more, then you
-- may also just change the time signature accordingly. Also, there's no swing
-- control, but you can easily get a triplet feel with the pulse filter
-- instead (e.g., in 4/4 try a triplet division along with a minimum pulse
-- strength of 0.3).

function dsp_ioconfig ()
   return { { midi_in = 1, midi_out = 1, audio_in = -1, audio_out = -1}, }
end

function dsp_options ()
   return { time_info = true, regular_block_length = true }
end

function dsp_params ()
   return
      {
	 { type = "input", name = "Division", min = 1, max = 7, default = 1, integer = true, doc = "number of subdivisions of the beat" },
	 { type = "input", name = "Octave up", min = 0, max = 5, default = 0, integer = true, doc = "octave range up" },
	 { type = "input", name = "Octave down", min = 0, max = 5, default = 0, integer = true, doc = "octave range down" },
	 { type = "input", name = "Pattern", min = 1, max = 6, default = 1, integer = true, doc = "pattern style",
	   scalepoints =
	      {	["1 up"] = 1, ["2 down"] = 2, ["3 exclusive"] = 3, ["4 inclusive"] = 4, ["5 order"] = 5, ["6 random"] = 6 } },
	 { type = "input", name = "Min Velocity", min = 0, max = 127, default = 60, integer = true, doc = "minimum velocity" },
	 { type = "input", name = "Max Velocity", min = 0, max = 127, default = 120, integer = true, doc = "maximum velocity" },
	 { type = "input", name = "Min Filter", min = 0, max = 1, default = 0, doc = "minimum pulse strength" },
	 { type = "input", name = "Max Filter", min = 0, max = 1, default = 1, doc = "maximum pulse strength" },
	 { type = "input", name = "Latch", min = 0, max = 1, default = 0, toggled = true, doc = "toggle latch mode" },
	 { type = "input", name = "Sync", min = 0, max = 1, default = 0, toggled = true, doc = "toggle sync mode" },
	 { type = "input", name = "Bypass", min = 0, max = 1, default = 0, toggled = true, doc = "bypass the arpeggiator, pass through input notes" },
	 { type = "input", name = "Gate", min = 0, max = 1, default = 1, doc = "gate as fraction of pulse length", scalepoints = { legato = 0 } },
      }
end

function presets()
   -- just a few basic examples for now, we'll add more stuff here later
   return
      {
	 { name = "0 default", params = { Division = 1, ["Octave up"] = 0, ["Octave down"] = 0, Pattern = 1, ["Min Velocity"] = 60, ["Max Velocity"] = 120, ["Min Filter"] = 0, ["Max Filter"] = 1, Latch = 0, Sync = 0, Gate = 1 } },
	 { name = "1 latch", params = { Latch = 1, Sync = 0 } },
	 { name = "2 latch and sync", params = { Latch = 1, Sync = 1 } },
	 { name = "3 bass", params = { Division = 1, ["Octave up"] = 0, ["Octave down"] = 1, Pattern = 1, ["Min Filter"] = 0, ["Max Filter"] = 1, Gate = 1 } },
	 { name = "4 triplet feel #1 - synth", params = { Division = 3, ["Octave up"] = 1, ["Octave down"] = 1, Pattern = 3, ["Min Filter"] = 0.2, ["Max Filter"] = 1, Gate = 0 } },
	 { name = "5 triplet feel #2 - drums", params = { Division = 3, ["Octave up"] = 0, ["Octave down"] = 0, Pattern = 1, ["Min Filter"] = 0.2, ["Max Filter"] = 1, Gate = 0 } },
      }
end

-- debug level (1: print beat information in the log window, 2: also print the
-- current pattern whenever it changes, 3: also print note information, 4:
-- print everything)
local debug = 0

local chan = 0 -- MIDI output channel
local last_rolling -- last transport status, to detect changes
local last_beat -- last beat number
local last_num -- last note
local last_chan -- MIDI channel of last note
local last_gate -- off time of last note
local last_up, last_down, last_mode, last_sync, last_bypass -- previous params, to detect changes
local chord = {} -- current chord (note store)
local chord_index = 0 -- index of last chord note (0 if none)
local latched = {} -- latched notes
local pattern = {} -- current pattern
local index = 0 -- current pattern index (reset when pattern changes)

-- Meter object
Meter = {}
Meter.__index = Meter

function Meter:new(m) -- constructor
   -- n = maximum subdivision, septoles seem to work reasonably well
   -- meter = meter, {4} a.k.a. common time is default
   -- indisp = indispensability tables, computed below
   local x = setmetatable({ n = 7, meter = {4}, indisp = {} }, Meter)
   x:compute(m)
   return x
end

-- Computes the best subdivision q in the range 1..n and pulse p in the range
-- 0..q so that p/q matches the given phase f in the floating point range 0..1
-- as closely as possible. Returns p, q and the absolute difference between f
-- and p/q. NB: Seems to work best for q values up to 7.

local function subdiv(n, f)
   local best_p, best_q, best = 0, 0, 1
   for q = 1, n do
      local p = math.floor(f*q+0.5) -- round towards nearest pulse
      local diff = math.abs(f-p/q)
      if diff < best then
	 best_p, best_q, best = p, q, diff
      end
   end
   return best_p, best_q, best
end

-- prime factors of integers
local function factor(n)
   local factors = {}
   if n<0 then n = -n end
   while n % 2 == 0 do
      table.insert(factors, 2)
      n = math.floor(n / 2)
   end
   local p = 3
   while p <= math.sqrt(n) do
      while n % p == 0 do
	 table.insert(factors, p)
	 n = math.floor(n / p)
      end
      p = p + 2
   end
   if n > 1 then -- n must be prime
      table.insert(factors, n)
   end
   return factors
end

-- reverse a table

local function reverse(list)
   local res = {}
   for k, v in ipairs(list) do
      table.insert(res, 1, v)
   end
   return res
end

-- arithmetic sequences

local function seq(from, to, step)
   step = step or 1;
   local sgn = step>=0 and 1 or -1
   local res = {}
   while sgn*(to-from) >= 0 do
      table.insert(res, from)
      from = from + step
   end
   return res
end

-- some functional programming goodies

local function map(list, fn)
   local res = {}
   for k, v in ipairs(list) do
      table.insert(res, fn(v))
   end
   return res
end

local function reduce(list, acc, fn)
   for k, v in ipairs(list) do
      acc = fn(acc, v)
   end
   return acc
end

local function collect(list, acc, fn)
   local res = {acc}
   for k, v in ipairs(list) do
      acc = fn(acc, v)
      table.insert(res, acc)
   end
   return res
end

local function sum(list)
   return reduce(list, 0, function(a,b) return a+b end)
end

local function prd(list)
   return reduce(list, 1, function(a,b) return a*b end)
end

local function sums(list)
   return collect(list, 0, function(a,b) return a+b end)
end

local function prds(list)
   return collect(list, 1, function(a,b) return a*b end)
end

-- indispensabilities (Barlow's formula)
local function indisp(q)
   function ind(q, k)
      -- prime indispensabilities
      function pind(q, k)
	 function ind1(q, k)
	    local i = ind(reverse(factor(q-1)), k)
	    local j = i >= math.floor(q / 4) and 1 or 0;
	    return i+j
	 end
	 if q <= 3 then
	    return (k-1) % q
	 elseif k == q-2 then
	    return math.floor(q / 4)
	 elseif k == q-1 then
	    return ind1(q, k-1)
	 else
	    return ind1(q, k)
	 end
      end
      local s = prds(q)
      local t = reverse(prds(reverse(q)))
      return
	 sum(
	    map(seq(1, #q),
		function(i)
		   return s[i] *
		      pind(q[i], (math.floor((k-1) % t[1] / t[i+1]) + 1) % q[i])
		end
	 ))
   end
   if type(q) == "number" then
      q = factor(q)
   end
   if type(q) ~= "table" then
      error("invalid argument, must be an integer or table of primes")
   else
      return map(seq(0,prd(q)-1), function(k) return ind(q,k) end)
   end
end

local function tableconcat(t1,t2)
   local res = {}
   for i=1,#t1 do
      table.insert(res, t1[i])
   end
   for i=1,#t2 do
      table.insert(res, t2[i])
   end
   return res
end

-- This optionally takes a new meter as argument and (re)computes the
-- indispensability tables. NOTE: This can be called (and the meter be
-- changed) at any time.
function Meter:compute(meter)
   meter = meter or self.meter
   -- a number is interpreted as a singleton list
   meter = type(meter) == "number" and {meter} or meter
   self.meter = meter
   local n = 1
   local m = {}
   for i,q in ipairs(meter) do
      if q ~= math.floor(q) then
	 error("meter: levels must be integer")
      elseif q < 1 then
	 error("meter: levels must be positive")
      end
      -- factorize each level as Barlow's formula assumes primes
      m = tableconcat(m, factor(q))
      n = n*q
   end
   self.beats = n
   self.last_q = nil
   if self.beats > 1 then
      self.indisp[1] = indisp(m)
      for q = 2, self.n do
	 local qs = tableconcat(m, factor(q))
	 self.indisp[q] = indisp(qs)
      end
   else
      self.indisp[1] = {0}
      for q = 2, self.n do
	 self.indisp[q] = indisp(q)
      end
   end
end

-- This takes the (possibly fractional) pulse and returns the pulse strength
-- along with the total number of beats.
function Meter:pulse(f)
   if type(f) ~= "number" then
      error("meter: beat index must be a number")
   elseif f < 0 then
      error("meter: beat index must be nonnegative")
   end
   local beat, f = math.modf(f)
   -- take the beat index modulo the total number of beats
   beat = beat % self.beats
   if self.n > 0 then
      local p, q = subdiv(self.n, f)
      if self.last_q then
	 local x = self.last_q / q
	 if math.floor(x) == x then
	    -- If the current best match divides the previous one, stick to
	    -- it, in order to prevent the algorithm from quickly changing
	    -- back to the root meter at each base pulse. XXFIXME: This may
	    -- stick around indefinitely until the meter changes. Maybe we'd
	    -- rather want to reset this automatically after some time (such
	    -- as a complete bar without non-zero phases)?
	    p, q = x*p, x*q
	 end
      end
      self.last_q = q
      -- The overall zero-based pulse index is beat*q + p. We add 1 to
      -- that to get a 1-based index into the indispensabilities table.
      local w = self.indisp[q][beat*q+p+1]
      return w, self.beats*q
   else
      local w = self.indisp[1][beat+1]
      return w, self.beats
   end
end

-- NOTE: Computing the necessary tables for the Barlow meter is a fairly
-- cpu-intensive operation, so changing the time signature mid-flight might
-- cause some cpu spikes and thus x-runs. To mitigate this, we cache each
-- meter as soon as we first encounter it, so that no costly recomputations
-- are needed later. An initial scan of the timeline makes sure that the cache
-- is well-populated from the get-go.

local last_mdiv
-- cached Barlow meters
local barlow_meters = { [4] = Meter:new() } -- common time
-- current Barlow meter
local barlow_meter = barlow_meters[4]

function dsp_init (rate)
   local loc = Session:locations():session_range_location()
   if loc then
      local tm = Temporal.TempoMap.read ()
      local a, b = loc:start():beats(), loc:_end():beats()
      if debug >= 1 then
	 print(loc:name(), a, b)
      end
      -- Scan through the timeline to find all time signatures and cache the
      -- resulting Barlow meters. Note that only care about the number of
      -- divisions here, that's all the algorithm needs.
      while a <= b do
	 local m = tm:meter_at_beats(a)
	 local mdiv = m:divisions_per_bar()
	 if not barlow_meters[mdiv] then
	    if debug >= 1 then
	       print(a, string.format("%d/%d", mdiv, m:note_value()))
	    end
	    barlow_meters[mdiv] = Meter:new(mdiv)
	 end
	 a = a:next_beat()
      end
   elseif debug >= 1 then
      print("empty session")
   end
end

function dsp_run (_, _, n_samples)
   assert (type(midiout) == "table")
   assert (type(time) == "table")
   assert (type(midiout) == "table")

   local ctrl = CtrlPorts:array ()
   -- We need to make sure that these are integer values. (The GUI enforces
   -- this, but fractional values may occur through automation.)
   local subdiv, up, down, mode = math.floor(ctrl[1]), math.floor(ctrl[2]), math.floor(ctrl[3]), math.floor(ctrl[4])
   local minvel, maxvel = math.floor(ctrl[5]), math.floor(ctrl[6])
   -- these are floating point values in the 0-1 range
   local minw, maxw = ctrl[7], ctrl[8]
   local gate = ctrl[12]
   -- latch toggle
   local latch = ctrl[9] > 0
   -- sync toggle
   local sync = ctrl[10] > 0
   -- bypass toggle
   local bypass = ctrl[11] > 0
   -- rolling state: It seems that we need to check the transport state (as
   -- given by Ardour's "transport finite state machine" = TFSM) here, even if
   -- the transport is not actually moving yet. Otherwise some input notes may
   -- errorneously slip through before playback really starts.
   local rolling = Session:transport_state_rolling ()
   -- whether the pattern must be recomputed, due to parameter changes or MIDI
   -- input
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
	 --print(string.format("[%d] %0x %d %d", ev.time, ev.data[1], ev.data[2], ev.data[3]))
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
      if last_beat ~= math.floor(time.beat) or bf1 == b1 then
	 -- next beat is due immediately
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
	 -- get the tempo map information
	 local tm = Temporal.TempoMap.read ()
	 local pos = Temporal.timepos_t (ts)
	 local bbt = tm:bbt_at (pos)
	 local meter = tm:meter_at (pos)
	 local tempo = tm:tempo_at (pos)
	 -- calculate the note-off time in samples, this is used if the gate
	 -- control is neither 0 nor 1
	 local gate_ts = ts + math.floor(tm:bbt_duration_at(pos, Temporal.BBT_Offset(0,1,0)):samples() / subdiv * gate)
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
	 -- we take a very small gate value (close to 0) to mean legato
	 -- instead, in which case notes extend to the next unfiltered note
	 local legato =  gate_ts < time.sample_end
	 function note_off()
	    if last_num then
	       -- kill the old note
	       if debug >= 3 then
		  print("note off", last_num)
	       end
	       midiout[k] = { time = ts, data = { 0x80+last_chan, last_num, 100 } }
	       last_num = nil
	       k = k+1
	    end
	 end
	 if not legato then
	    note_off()
	 end
	 if n > 0 then
	    -- calculate a fractional pulse number from the current bbt
	    local p = bbt.beats-1 + math.max(0, bbt.ticks) / Temporal.ticks_per_beat
	    -- Detect meter changes and update the Barlow meter object
	    -- accordingly.
	    local mdiv = meter:divisions_per_bar()
	    if mdiv ~= last_mdiv then
	       if not barlow_meters[mdiv] then
		  if debug >= 1 then
		     print(bt, string.format("%d/%d", mdiv, meter:note_value()))
		  end
		  barlow_meters[mdiv] = Meter:new(mdiv)
	       end
	       barlow_meter = barlow_meters[mdiv]
	       last_mdiv = mdiv
	    end
	    -- Use the algorithm to determine the pulse weight.
	    local w, npulses = barlow_meter:pulse (p)
	    if debug >= 4 then
	       print(" Beat:", p, " Weight =", w, "/", npulses-1)
	    end
	    -- normalize the weight to the 0-1 range
	    w = w/(npulses-1)
	    -- filter notes
	    if w >= minw and w <= maxw then
	       if legato then
		  note_off()
	       end
	       -- compute the velocity, round to nearest integer
	       local v = minvel + w * (maxvel-minvel)
	       v = math.floor(v+0.5)
	       --print("p", p, "v", v)
	       -- trigger the new note
	       if sync then
		  -- sync pattern to the bbt
		  local l = #pattern
		  local k = math.floor(p*subdiv+0.5) -- current index in bar
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
	       if gate < 1 and not legato then
		  -- Set the sample time at which the note-off is due.
		  last_gate = gate_ts
	       else
		  -- Otherwise don't set the off time in which case the
		  -- note-off gets triggered automatically above.
		  last_gate = nil
	       end
	    end
	 end
      end
   else
      -- transport not rolling or bypass; reset the last beat number
      last_beat = nil
   end

   if debug >= 1 and #midiout > 0 then
      -- monitor memory usage of the Lua interpreter
      print(string.format("mem: %0.2f KB", collectgarbage("count")))
   end

end
