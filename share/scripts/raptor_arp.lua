ardour {
   ["type"]    = "dsp",
   name        = "Arpeggiator (Raptor)",
   category    = "Effect",
   author      = "Albert Gräf",
   license     = "GPL",
   description = [[Raptor: The Random Arpeggiator (Raptor 6, Ardour implementation v0.3)

Advanced arpeggiator with random note generation, harmonic controls, input pitch and velocity tracking, and automatic modulation of various parameters.

In memory of Clarence Barlow (27 December 1945 – 29 June 2023).
]]
}

-- Raptor Random Arpeggiator for Ardour, ported from the pd-lua version at
-- https://github.com/agraef/raptor-lua.

-- Author: Albert Gräf <aggraef@gmail.com>, Dept. of Music-Informatics,
-- Johannes Gutenberg University (JGU) of Mainz, Germany, please check
-- https://agraef.github.io/ for a list of my software.

-- Copyright (c) 2021 by Albert Gräf <aggraef@gmail.com>

-- Distributed under the GPLv3+, please check the accompanying COPYING file
-- for details.

-- As the Ardour Lua interface wants everything in a single Lua module, this
-- is a hodgeposge of the modules making up the pd-lua version, with the
-- Ardour dsp thrown on top that.

-- -------------------------------------------------------------------------

-- Various helper functions to compute Barlow meters and harmonicities using
-- the methods from Clarence Barlow's Ratio book (Feedback Papers, Cologne,
-- 2001)


local M = {}

-- list helper functions

-- concatenate tables
function M.tableconcat(t1, t2)
   local res = {}
   for i=1,#t1 do
      table.insert(res, t1[i])
   end
   for i=1,#t2 do
      table.insert(res, t2[i])
   end
   return res
end

-- reverse a table
function M.reverse(list)
   local res = {}
   for _, v in ipairs(list) do
      table.insert(res, 1, v)
   end
   return res
end

-- arithmetic sequences
function M.seq(from, to, step)
   step = step or 1;
   local sgn = step>=0 and 1 or -1
   local res = {}
   while sgn*(to-from) >= 0 do
      table.insert(res, from)
      from = from + step
   end
   return res
end

-- cycle through a table
function M.cycle(t, i)
   local n = #t
   if n > 0 then
      while i > n do
	 i = i - n
      end
   end
   return t[i]
end

-- some functional programming goodies

function M.map(list, fn)
   local res = {}
   for _, v in ipairs(list) do
      table.insert(res, fn(v))
   end
   return res
end

function M.reduce(list, acc, fn)
   for _, v in ipairs(list) do
      acc = fn(acc, v)
   end
   return acc
end

function M.collect(list, acc, fn)
   local res = {acc}
   for _, v in ipairs(list) do
      acc = fn(acc, v)
      table.insert(res, acc)
   end
   return res
end

function M.sum(list)
   return M.reduce(list, 0, function(a,b) return a+b end)
end

function M.prd(list)
   return M.reduce(list, 1, function(a,b) return a*b end)
end

function M.sums(list)
   return M.collect(list, 0, function(a,b) return a+b end)
end

function M.prds(list)
   return M.collect(list, 1, function(a,b) return a*b end)
end

-- Determine the prime factors of an integer. The result is a list with the
-- prime factors in non-decreasing order.

function M.factor(n)
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

-- Collect the factors of the integer n and return them as a list of pairs
-- {p,k} where p are the prime factors in ascending order and k the
-- corresponding (nonzero) multiplicities. If the given number is a pair {p,
-- q}, considers p/q as a rational number and returns its prime factors with
-- positive or negative multiplicities.

function M.factors(x)
   if type(x) == "table" then
      local n, m = table.unpack(x)
      local pfs, nfs, mfs = {}, M.factors(n), M.factors(m)
      -- merge the factors in nfs and mfs into a single list
      local i, j, k, N, M = 1, 1, 1, #nfs, #mfs
      while i<=N or j<=M do
	 if j>M or (i<=N and mfs[j][1]>nfs[i][1]) then
	    pfs[k] = nfs[i]
	    k = k+1; i = i+1
	 elseif i>N or (j<=M and nfs[i][1]>mfs[j][1]) then
	    pfs[k] = mfs[j]
	    pfs[k][2] = -mfs[j][2]
	    k = k+1; j = j+1
	 else
	    pfs[k] = nfs[i]
	    pfs[k][2] = nfs[i][2] - mfs[j][2]
	    k = k+1; i = i+1; j = j+1
	 end
      end
      return pfs
   else
      local pfs, pf = {}, M.factor(x)
      if next(pf) then
	 local j, n = 1, #pf
	 pfs[j] = {pf[1], 1}
	 for i = 2, n do
	    if pf[i] == pfs[j][1] then
	       pfs[j][2] = pfs[j][2] + 1
	    else
	       j = j+1
	       pfs[j] = {pf[i], 1}
	    end
	 end
      end
      return pfs
   end
end

-- Probability functions. These are used with some of the random generation
-- functions below.

-- Create random permutations. Chooses n random values from a list ms of input
-- values according to a probability distribution given by a list ws of
-- weights. NOTES: ms and ws should be of the same size, otherwise excess
-- elements will be chosen at random. In particular, if ws is empty or missing
-- then shuffle(n, ms) will simply return n elements chosen from ms at random
-- using a uniform distribution. ms and ws and are modified *in place*,
-- removing chosen elements, so that their final contents will be the elements
-- *not* chosen and their corresponding weight distribution.

function M.shuffle(n, ms, ws)
   local res = {}
   if ws == nil then
      -- simply choose elements at random, uniform distribution
      ws = {}
   end
   while next(ms) ~= nil and n>0 do
      -- accumulate weights
      local sws = M.sums(ws)
      local s = sws[#sws]
      table.remove(sws, 1)
      -- pick a random index
      local k, r = 0, math.random()*s
      --print("r = ", r, "sws = ", table.unpack(sws))
      for i = 1, #sws do
	 if r < sws[i] then
	    k = i; break
	 end
      end
      -- k may be out of range if ws and ms aren't of the same size, in which
      -- case we simply pick an element at random
      if k==0 or k>#ms then
	 k = math.random(#ms)
      end
      table.insert(res, ms[k])
      n = n-1; table.remove(ms, k);
      if k<=#ws then
	 table.remove(ws, k)
      end
   end
   return res
end

-- Calculate modulated values. This is used for all kinds of parameters which
-- can vary automatically according to pulse strength, such as note
-- probability, velocity, gate, etc.

function M.mod_value(x1, x2, b, w)
   -- x2 is the nominal value which is always output if b==0. As b increases
   -- or decreases, the range extends downwards towards x1. (Normally,
   -- x2>x1, but you can reverse bounds to have the range extend upwards.)
   if b >= 0 then
      -- positive bias: mod_value(w) -> x1 as w->0, -> x2 as w->1
      -- zero bias: mod_value(w) == x2 (const.)
      return x2-b*(1-w)*(x2-x1)
   else
      -- negative bias: mod_value(w) -> x1 as w->1, -> x2 as w->0
      return x2+b*w*(x2-x1)
   end
end

-- Barlow meters. This stuff is mostly a verbatim copy of the guts of
-- meter.pd_lua, please check that module for details.

-- Computes the best subdivision q in the range 1..n and pulse p in the range
-- 0..q so that p/q matches the given phase f in the floating point range 0..1
-- as closely as possible. Returns p, q and the absolute difference between f
-- and p/q. NB: Seems to work best for q values up to 7.

function M.subdiv(n, f)
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

-- Compute pulse strengths according to Barlow's indispensability formula from
-- the Ratio book.

function M.indisp(q)
   local function ind(q, k)
      -- prime indispensabilities
      local function pind(q, k)
	 local function ind1(q, k)
	    local i = ind(M.reverse(M.factor(q-1)), k)
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
      local s = M.prds(q)
      local t = M.reverse(M.prds(M.reverse(q)))
      return
	 M.sum(M.map(M.seq(1, #q), function(i) return s[i] * pind(q[i], (math.floor((k-1) % t[1] / t[i+1]) + 1) % q[i]) end))
   end
   if type(q) == "number" then
      q = M.factor(q)
   end
   if type(q) ~= "table" then
      error("invalid argument, must be an integer or table of primes")
   else
      return M.map(M.seq(0,M.prd(q)-1), function(k) return ind(q,k) end)
   end
end

-- Barlow harmonicities from the Ratio book. These are mostly ripped out of an
-- earlier version of the Raptor random arpeggiator programs (first written in
-- Q, then rewritten in Pure, and now finally ported to Lua).

-- Some "standard" 12 tone scales and prime valuation functions to play with.
-- Add others as needed. We mostly use the just scale and the standard Barlow
-- valuation here.

M.just = -- standard just intonation, a.k.a. the Ptolemaic (or Didymic) scale
   {  {1,1}, {16,15}, {9,8}, {6,5}, {5,4}, {4,3}, {45,32},
      {3,2}, {8,5}, {5,3}, {16,9}, {15,8}, {2,1}  }
M.pyth = -- pythagorean (3-limit) scale
   {  {1,1}, {2187,2048}, {9,8}, {32,27}, {81,64}, {4,3}, {729,512},
      {3,2}, {6561,4096}, {27,16}, {16,9}, {243,128}, {2,1}  }
M.mean4 = -- 1/4 comma meantone scale, Barlow (re-)rationalization
   {  {1,1}, {25,24}, {10,9}, {6,5}, {5,4}, {4,3}, {25,18},
      {3,2}, {25,16}, {5,3}, {16,9}, {15,8}, {2,1}  }

function M.barlow(p)	return 2*(p-1)*(p-1)/p end
function M.euler(p)	return p-1 end
-- "mod 2" versions (octave is eliminated)
function M.barlow2(p)	if p==2 then return 0 else return M.barlow(p) end end
function M.euler2(p)	if p==2 then return 0 else return M.euler(p) end end

-- Harmonicity computation.

-- hrm({p,q}, pv) computes the disharmonicity of the interval p/q using the
-- prime valuation function pv.

-- hrm_dist({p1,q1}, {p2,q2}, pv) computes the harmonic distance between two
-- pitches, i.e., the disharmonicity of the interval between {p1,q1} and
-- {p2,q2}.

-- hrm_scale(S, pv) computes the disharmonicity metric of a scale S, i.e., the
-- pairwise disharmonicities of all intervals in the scale. The input is a
-- list of intervals as {p,q} pairs, the output is the distance matrix.

function M.hrm(x, pv)
   return M.sum(M.map(M.factors(x),
	function(f) local p, k = table.unpack(f)
	   return math.abs(k) * pv(p)
	end))
end

function M.hrm_dist(x, y, pv)
   local p1, q1 = table.unpack(x)
   local p2, q2 = table.unpack(y)
   return M.hrm({p1*q2,p2*q1}, pv)
end

function M.hrm_scale(S, pv)
   return M.map(S,
	function(s)
	   return M.map(S, function(t) return M.hrm_dist(s, t, pv) end)
	end)
end

-- Some common tables for convenience and testing. These are all based on a
-- standard 12-tone just tuning. NOTE: The given reference tables use rounded
-- values, but are good enough for most practical purposes; you might want to
-- employ these to avoid the calculation cost.

-- Barlow's "indigestibility" harmonicity metric
-- M.bgrad = {0,13.07,8.33,10.07,8.4,4.67,16.73,3.67,9.4,9.07,9.33,12.07,1}
M.bgrad = M.map(M.just, function(x) return M.hrm(x, M.barlow) end)

-- Euler's "gradus suavitatis" (0-based variant)
-- M.egrad = {0,10,7,7,6,4,13,3,7,6,8,9,1}
M.egrad = M.map(M.just, function(x) return M.hrm(x, M.euler) end)

-- In an arpeggiator we might want to treat different octaves of the same
-- pitch as equivalent, in which case we can use the following "mod 2" tables:
M.bgrad2 = M.map(M.just, function(x) return M.hrm(x, M.barlow2) end)
M.egrad2 = M.map(M.just, function(x) return M.hrm(x, M.euler2) end)

-- But in the following we stick to the standard Barlow table.
M.grad = M.bgrad

-- Calculate the harmonicity of the interval between two (MIDI) notes.
function M.hm(n, m)
   local d = math.max(n, m) - math.min(n, m)
   return 1/(1+M.grad[d%12+1])
end

-- Use this instead if you also want to keep account of octaves.
function M.hm2(n, m)
   local d = math.max(n, m) - math.min(n, m)
   return 1/(1+M.grad[d%12+1]+(d//12)*M.grad[13])
end

-- Calculate the average harmonicity (geometric mean) of a MIDI note relative
-- to a given chord (specified as a list of MIDI notes).
function M.hv(ns, m)
   if next(ns) ~= nil then
      local xs = M.map(ns, function(n) return M.hm(m, n) end)
      return M.prd(xs)^(1/#xs)
   else
      return 1
   end
end

-- Sort the MIDI notes in ms according to descending average harmonicities
-- w.r.t. the MIDI notes in ns. This allows you to quickly pick the "best"
-- (harmonically most pleasing) MIDI notes among given alternatives ms
-- w.r.t. a given chord ns.
function M.besthv(ns, ms)
   local mhv = M.map(ms, function(m) return {m, M.hv(ns, m)} end)
   table.sort(mhv, function(x, y) return x[2]>y[2] or
		 (x[2]==y[2] and x[1]<y[1]) end)
   return M.map(mhv, function(x) return x[1] end)
end

-- Randomized note filter. This is the author's (in)famous Raptor algorithm.
-- It needs a whole bunch of parameters, but also delivers much more
-- interesting results and can produce randomized chords as well. Basically,
-- it performs a random walk guided by Barlow harmonicities and
-- indispensabilities. The parameters are:

-- ns: input notes (chord memory of the arpeggiator, as in besthv these are
-- used to calculate the average harmonicities)

-- ms: candidate output notes (these will be filtered and participate in the
-- random walk)

-- w: indispensability value used to modulate the various parameters

-- nmax, nmod: range and modulation of the density (maximum number of notes
-- in each step)

-- smin, smax, smod: range and modulation of step widths, which limits the
-- steps between notes in successive pulses

-- dir, mode, uniq: arpeggio direction (0 = random, 1 = up, -1 = down), mode
-- (0 = random, 1 = up, 2 = down, 3 = up-down, 4 = down-up), and whether
-- repeated notes are disabled (uniq flag)

-- hmin, hmax, hmod: range and modulation of eligible harmonicities, which are
-- used to filter candidate notes based on average harmonicities w.r.t. the
-- input notes

-- pref, prefmod: range and modulation of harmonic preference. This is
-- actually one of the most important and effective parameters in the Raptor
-- algorithm which drives the random note selection process. A pref value
-- between -1 and 1 determines the weighted probabilities used to pick notes
-- at random. pref>0 gives preference to notes with high harmonicity, pref<0
-- to notes with low harmonicity, and pref==0 ignores harmonicity (in which
-- case all eligible notes are chosen with the same probability). The prefs
-- parameter can also be modulated by pulse strengths as indicated by prefmod
-- (prefmod>0 lowers preference on weak pulses, prefmod<0 on strong pulses).

function M.harm_filter(w, hmin, hmax, hmod, ns, ms)
   -- filters notes according to harmonicities and a given pulse weight w
   if next(ns) == nil then
      -- empty input (no eligible notes)
      return {}
   else
      local res = {}
      for _,m in ipairs(ms) do
	 local h = M.hv(ns, m)
	 -- modulate: apply a bias determined from hmod and w
	 if hmod > 0 then
	    h = h^(1-hmod*(1-w))
	 elseif hmod < 0 then
	    h = h^(1+hmod*w)
	 end
	 -- check that the (modulated) harmonicity is within prescribed bounds
	 if h>=hmin and h<=hmax then
	    table.insert(res, m)
	 end
      end
      return res
   end
end

function M.step_filter(w, smin, smax, smod, dir, mode, cache, ms)
   -- filters notes according to the step width parameters and pulse weight w,
   -- given which notes are currently playing (the cache)
   if next(ms) == nil or dir == 0 then
      return ms, dir
   end
   local res = {}
   while next(res) == nil do
      if next(cache) ~= nil then
	 -- non-empty cache, going any direction
	 local lo, hi = cache[1], cache[#cache]
	 -- NOTE: smin can be negative, allowing us, say, to actually take a
	 -- step *down* while going upwards. But we always enforce that smax
	 -- is non-negative in order to avoid deadlock situations where *no*
	 -- step is valid anymore, and even restarting the pattern doesn't
	 -- help. (At least that's what I think, I don't really recall what
	 -- the original rationale behind all this was, but since it's in the
	 -- original Raptor code, it must make sense somehow. ;-)
	 smax = math.max(0, smax)
	 smax = math.floor(M.mod_value(math.abs(smin), smax, smod, w)+0.5)
	 local function valid_step_min(m)
	    if dir==0 then
	       return (m>=lo+smin) or (m<=hi-smin)
	    elseif dir>0 then
	       return m>=lo+smin
	    else
	       return m<=hi-smin
	    end
	 end
	 local function valid_step_max(m)
	    if dir==0 then
	       return (m>=lo-smax) and (m<=hi+smax)
	    elseif dir>0 then
	       return (m>=lo+math.min(0,smin)) and (m<=hi+smax)
	    else
	       return (m>=lo-smax) and (m<=hi-math.min(0,smin))
	    end
	 end
	 for _,m in ipairs(ms) do
	    if valid_step_min(m) and valid_step_max(m) then
	       table.insert(res, m)
	    end
	 end
      elseif dir == 1 then
	 -- empty cache, going up, start at bottom
	 local lo = ms[1]
	 local max = math.floor(M.mod_value(smin, smax, smod, w)+0.5)
	 for _,m in ipairs(ms) do
	    if m <= lo+max then
	       table.insert(res, m)
	    end
	 end
      elseif dir == -1 then
	 -- empty cache, going down, start at top
	 local hi = ms[#ms]
	 local max = math.floor(M.mod_value(smin, smax, smod, w)+0.5)
	 for _,m in ipairs(ms) do
	    if m >= hi-max then
	       table.insert(res, m)
	    end
	 end
      else
	 -- empty cache, random direction, all notes are eligible
	 return ms, dir
      end
      if next(res) == nil then
	 -- we ran out of notes, restart the pattern
	 -- print("raptor: no notes to play, restart!")
	 cache = {}
	 if mode==0 then
	    dir = 0
	 elseif mode==1 or (mode==3 and dir==0) then
	    dir = 1
	 elseif mode==2 or (mode==4 and dir==0) then
	    dir = -1
	 else
	    dir = -dir
	 end
      end
   end
   return res, dir
end

function M.uniq_filter(uniq, cache, ms)
   -- filters out repeated notes (removing notes already in the cache),
   -- depending on the uniq flag
   if not uniq or next(ms) == nil or next(cache) == nil then
      return ms
   end
   local res = {}
   local i, j, k, N, M = 1, 1, 1, #cache, #ms
   while i<=N or j<=M do
      if j>M then
	 -- all elements checked, we're done
	 return res
      elseif i>N or ms[j]<cache[i] then
	 -- current element not in cache, add it
	 res[k] = ms[j]
	 k = k+1; j = j+1
      elseif ms[j]>cache[i] then
	 -- look at next cache element
	 i = i+1
      else
	 -- current element in cache, skip it
	 i = i+1; j = j+1
      end
   end
   return res
end

function M.pick_notes(w, n, pref, prefmod, ns, ms)
   -- pick n notes from the list ms of eligible notes according to the
   -- given harmonic preference
   local ws = {}
   -- calculate weighted harmonicities based on preference; this gives us the
   -- probability distribution for the note selection step
   local p = M.mod_value(0, pref, prefmod, w)
   if p==0 then
      -- no preference, use uniform distribution
      for i = 1, #ms do
	 ws[i] = 1
      end
   else
      for i = 1, #ms do
	 -- "Frankly, I don't know where the exponent came from," probably
	 -- experimentation. ;-)
	 ws[i] = M.hv(ns, ms[i]) ^ (p*10)
      end
   end
   return M.shuffle(n, ms, ws)
end

-- The note generator. This is invoked with the current pulse weight w, the
-- current cache (notes played in the previous step), the input notes ns, the
-- candidate output notes ms, and all the other parameters that we need
-- (density: nmax, nmod; harmonicity: hmin, hmax, hmod; step width: smin,
-- smax, smod; arpeggiator state: dir, mode, uniq; harmonic preference: pref,
-- prefmod). It returns a selection of notes chosen at random for the given
-- parameters, along with the updated direction dir of the arpeggiator.

function M.rand_notes(w, nmax, nmod,
		      hmin, hmax, hmod,
		      smin, smax, smod,
		      dir, mode, uniq,
		      pref, prefmod,
		      cache,
		      ns, ms)
   -- uniqueness filter: remove repeated notes
   local res = M.uniq_filter(uniq, cache, ms)
   -- harmonicity filter: select notes based on harmonicity
   res = M.harm_filter(w, hmin, hmax, hmod, ns, res)
   -- step filter: select notes based on step widths and arpeggiator state
   -- (this must be the last filter!)
   res, dir = M.step_filter(w, smin, smax, smod, dir, mode, cache, res)
   -- pick notes
   local n = math.floor(M.mod_value(1, nmax, nmod, w)+0.5)
   res = M.pick_notes(w, n, pref, prefmod, ns, res)
   return res, dir
end

local barlow = M

-- -------------------------------------------------------------------------

-- quick and dirty replacement for kikito's inspect; we mostly need this for
-- debugging messages, but also when saving data, so the output doesn't need
-- to be pretty, but should be humanly readable and conform to Lua syntax

local function inspect(x)
   if type(x) == "string" then
      return string.format("%q", x)
   elseif type(x) == "table" then
      local s = ""
      local n = 0
      for k,v in pairs(x) do
	 if n > 0 then
	    s = s .. ", "
	 end
	 s = s .. string.format("[%s] = %s", inspect(k), inspect(v))
	 n = n+1
      end
      return string.format("{ %s }", s)
   else
      return tostring(x)
   end
end

-- -------------------------------------------------------------------------

-- Arpeggiator object. In the Pd external, this takes input from the object's
-- inlets and returns results on the object's outlets. In the Ardour
-- implementation, the inlets are just method arguments, and the outlets
-- become the method's return values (there can be more than one, up to one
-- for each outlet, which are represented as tuples).

-- Also, the Ardour implementation replaces the hold toggle with a latch
-- control, which can be used in a similar fashion but is much more useful.

arpeggio = {}
arpeggio.__index = arpeggio

function arpeggio:new(m) -- constructor
   local x = setmetatable(
      {
	 -- some reasonable defaults (see also arpeggio:initialize below)
	 debug = 0, idx = 0, chord = {}, pattern = {},
	 latch = nil, down = -1, up = 1, mode = 0,
	 minvel = 60, maxvel = 120, velmod = 1,
	 wmin = 0, wmax = 1,
	 pmin = 0.3, pmax = 1, pmod = 0,
	 gate = 1, gatemod = 0,
	 veltracker = 1, minavg = nil, maxavg = nil,
	 gain = 1, g =  math.exp(-1/3),
	 loopstate = 0, loopsize = 0, loopidx = 0, loop = {}, loopdir = "",
	 nmax = 1, nmod = 0,
	 hmin = 0, hmax = 1, hmod = 0,
	 smin = 1, smax = 7, smod = 0,
	 uniq = 1,
	 pref = 1, prefmod = 0,
	 pitchtracker = 0, pitchlo = 0, pitchhi = 0,
	 n = 0
      },
      arpeggio)
   x:initialize(m)
   return x
end

function arpeggio:initialize(m)
   -- debugging (bitmask): 1 = pattern, 2 = input, 4 = output
   self.debug = 0
   -- internal state variables
   self.idx = 0
   self.chord = {}
   self.pattern = {}
   self.latch = nil
   self.down, self.up, self.mode = -1, 1, 0
   self.minvel, self.maxvel, self.velmod = 60, 120, 1
   self.pmin, self.pmax, self.pmod = 0.3, 1, 0
   self.wmin, self.wmax = 0, 1
   self.gate, self.gatemod = 1, 0
   -- velocity tracker
   self.veltracker, self.minavg, self.maxavg = 1, nil, nil
   -- This isn't really a "gain" control any more, it's more like a dry/wet
   -- mix (1 = dry, 0 = wet) between set values (minvel, maxvel) and the
   -- calculated envelope of MIDI input notes (minavg, maxavg).
   self.gain = 1
   -- smoothing filter, time in pulses (3 works for me, YMMV)
   local t = 3
    -- filter coefficient
   self.g = math.exp(-1/t)
   -- looper
   self.loopstate = 0
   self.loopsize = 0
   self.loopidx = 0
   self.loop = {}
   self.loopdir = ""
   -- Raptor params, reasonable defaults
   self.nmax, self.nmod = 1, 0
   self.hmin, self.hmax, self.hmod = 0, 1, 0
   self.smin, self.smax, self.smod = 1, 7, 0
   self.uniq = 1
   self.pref, self.prefmod = 1, 0
   self.pitchtracker = 0
   self.pitchlo, self.pitchhi = 0, 0
   -- Barlow meter
   -- XXXTODO: We only do integer pulses currently, so the subdivisions
   -- parameter self.n is currently disabled. Maybe we can find some good use
   -- for it in the future, e.g., for ratchets?
   self.n = 0
   if m == nil then
      m = {4} -- default meter (common time)
   end
   -- initialize the indispensability tables and reset the beat counter
   self.indisp = {}
   self:prepare_meter(m)
   -- return the initial number of beats
   return self.beats
end

-- Barlow indispensability meter computation, cf. barlow.pd_lua. This takes a
-- zero-based beat number, optionally with a phase in the fractional part to
-- indicate a sub-pulse below the beat level. We then compute the closest
-- matching subdivision and compute the corresponding pulse weight, using the
-- precomputed indispensability tables. The returned result is a pair w,n
-- denoting the Barlow indispensability weight of the pulse in the range
-- 0..n-1, where n denotes the total number of beats (number of beats in the
-- current meter times the current subdivision).

-- list helpers
local tabcat, reverse, cycle, map, seq = barlow.tableconcat, barlow.reverse, barlow.cycle, barlow.map, barlow.seq
-- Barlow indispensabilities and friends
local factor, indisp, subdiv = barlow.factor, barlow.indisp, barlow.subdiv
-- Barlow harmonicities and friends
local mod_value, rand_notes = barlow.mod_value, barlow.rand_notes

function arpeggio:meter(b)
   if b < 0 then
      error("meter: beat index must be nonnegative")
      return
   end
   local beat, f = math.modf(b)
   -- take the beat index modulo the total number of beats
   beat = beat % self.beats
   if self.n > 0 then
      -- compute the closest subdivision for the given fractional phase
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
      -- no subdivisions, just return the indispensability and number of beats
      -- as is
      local w = self.indisp[1][beat+1]
      return w, self.beats
   end
end

function arpeggio:numarg(x)
   if type(x) == "table" then
      x = x[1]
   end
   if type(x) == "number" then
      return x
   else
      error("arpeggio: expected number, got " .. tostring(x))
   end
end

function arpeggio:intarg(x)
   if type(x) == "table" then
      x = x[1]
   end
   if type(x) == "number" then
      return math.floor(x)
   else
      error("arpeggio: expected integer, got " .. tostring(x))
   end
end

-- the looper

function arpeggio:loop_clear()
   -- reset the looper
   self.loopstate = 0
   self.loopidx = 0
   self.loop = {}
end

function arpeggio:loop_set()
   -- set the loop and start playing it
   local n, m = #self.loop, self.loopsize
   local b, p, q = self.beats, self.loopidx, self.idx
   -- NOTE: Use Ableton-style launch quantization here. We quantize start and
   -- end of the loop, as well as m = the target loop size to whole bars, to
   -- account for rhythmic inaccuracies. Otherwise it's just much too easy to
   -- miss bar boundaries when recording a loop.
   m = math.ceil(m/b)*b -- rounding up
   -- beginning of last complete bar in cyclic buffer
   local k = (p-q-b) % 256
   if n <= 0 or m <= 0 or m > 256 or k >= n then
      -- We haven't recorded enough steps for a bar yet, or the target size is
      -- 0, bail out with an empty loop.
      self.loop = {}
      self.loopidx = 0
      self.loopstate = 1
      if m == 0 then
	 print("loop: zero loop size")
      else
	 print(string.format("loop: got %d steps, need %d.", p>=n and math.max(0, p-q) or q==0 and n or math.max(0, n-b), b))
      end
      return
   end
   -- At this point we have at least 1 bar, starting at k+1, that we can grab;
   -- try extending the loop until we hit the target size.
   local l = b
   while l < m do
      if k >= b then
	 k = k-b
      elseif p >= n or (k-b) % 256 < p then
	 -- in this case either the cyclic buffer hasn't been filled yet, or
	 -- wrapping around would take us past the buffer pointer, so bail out
	 break
      else
	 -- wrap around to the end of the buffer
	 k = (k-b) % 256
      end
      l = l+b
   end
   -- grab l (at most m) steps
   --print(string.format("loop: recorded %d/%d steps %d-%d", l, m, k+1, k+m))
   print(string.format("loop: recorded %d/%d steps", l, m))
   local loop = {}
   for i = k+1, k+l do
      loop[i-k] = cycle(self.loop, i)
   end
   self.loop = loop
   self.loopidx = q % l
   self.loopstate = 1
end

function arpeggio:loop_add(notes, vel, gate)
   -- we only start recording at the first note
   local have_notes = type(notes) == "number" or
      (notes ~= nil and next(notes) ~= nil)
   if have_notes or next(self.loop) ~= nil then
      self.loop[self.loopidx+1] = {notes, vel, gate}
      -- we always *store* up to 256 steps in a cyclic buffer
      self.loopidx = (self.loopidx+1) % 256
   end
end

function arpeggio:loop_get()
   local res = {{}, 0, 0}
   local p, n = self.loopidx, math.min(#self.loop, self.loopsize)
   if p < n then
      res = self.loop[p+1]
      -- we always *read* exactly n steps in a cyclic buffer
      self.loopidx = (p+1) % n
      if p % self.beats == 0 then
	 local a, b = p // self.beats + 1, n // self.beats
	 print(string.format("loop: playing bar %d/%d", a, b))
      end
   end
   -- we maybe should return the current loopidx here which is used to give
   -- visual feedback about the loop cycle in the Pd external; not sure how to
   -- do this in Ardour, though
   return res
end

local function fexists(name)
   local f=io.open(name,"r")
   if f~=nil then io.close(f) return true else return false end
end

function arpeggio:loop_file(file, cmd)
   -- default for cmd is 1 (save) if loop is playing, 0 (load) otherwise
   cmd = cmd or self.loopstate
   -- apply the loopdir if any
   local path = self.loopdir .. file
   if cmd == 1 then
      -- save: first create a backup copy if the file already exists
      if fexists(path) then
	 local k, bakname = 1
	 repeat
	    bakname = string.format("%s~%d~", path, k)
	    k = k+1
	 until not fexists(bakname)
	 -- ignore errors, if we can't rename the file, we probably can't
	 -- overwrite it either
	 os.rename(path, bakname)
      end
      local f, err = io.open(path, "w")
      if type(err) == "string" then
	 print(string.format("loop: %s", err))
	 return
      end
      -- shorten the table to the current loop size if needed
      local loop, n = {}, math.min(#self.loop, self.loopsize)
      table.move(self.loop, 1, n, 1, loop)
      -- add some pretty-printing
      local function bars(level, count)
	 if level == 1 and count%self.beats == 0 then
	    return string.format("-- bar %d", count//self.beats+1)
	 end
      end
      f:write(string.format("-- saved by Raptor %s\n", os.date()))
      f:write(inspect(loop, {extra = 1, addin = bars}))
      f:close()
      print(string.format("loop: %s: saved %d steps", file, n))
   elseif cmd == 0 then
      -- load: check that file exists and is loadable
      local f, err = io.open(path, "r")
      if type(err) == "string" then
	 print(string.format("loop: %s", err))
	 return
      end
      local fun, err = load("return " .. f:read("a"))
      f:close()
      if type(err) == "string" or type(fun) ~= "function" then
	 print(string.format("loop: %s: invalid format", file))
      else
	 local loop = fun()
	 if type(loop) ~= "table" then
	    print(string.format("loop: %s: invalid format", file))
	 else
	    self.loop = loop
	    self.loopsize = #loop
	    self.loopidx = self.idx % math.max(1, self.loopsize)
	    self.loopstate = 1
	    print(string.format("loop: %s: loaded %d steps", file, #loop))
	    return self.loopsize
	 end
      end
   elseif cmd == 2 then
      -- check that file exists, report result
      return fexists(path) and 1 or 0
   end
end

function arpeggio:set_loopsize(x)
   x = self:intarg(x)
   if type(x) == "number" then
      self.loopsize = math.max(0, math.min(256, x))
      if self.loopstate == 1 then
	 -- need to update the loop index in case the loopsize changed
	 if self.loopsize > 0 then
	    -- also resynchronize the loop with the arpeggiator if needed
	    self.loopidx = math.max(self.idx, self.loopidx % self.loopsize)
	 else
	    self.loopidx = 0
	 end
      end
   end
end

function arpeggio:set_loop(x)
   if type(x) == "string" then
      x = {x}
   end
   if type(x) == "table" and type(x[1]) == "string" then
      -- file operations
      self:loop_file(table.unpack(x))
   else
      x = self:intarg(x)
      if type(x) == "number" then
	 if x ~= 0 and self.loopstate == 0 then
	    self:loop_set()
	 elseif x == 0 and self.loopstate == 1 then
	    self:loop_clear()
	 end
      end
   end
end

function arpeggio:set_loopdir(x)
   if type(x) == "string" then
      x = {x}
   end
   if type(x) == "table" and type(x[1]) == "string" then
      -- directory for file operations
      self.loopdir = x[1] .. "/"
   end
end

-- velocity tracking

function arpeggio:update_veltracker(chord, vel)
   if next(chord) == nil then
      -- reset
      self.minavg, self.maxavg = nil, nil
      if self.debug&2~=0 then
	 print(string.format("min = %s, max = %s", self.minavg, self.maxavg))
      end
   elseif vel > 0 then
      -- calculate the velocity envelope
      if not self.minavg then
	 self.minavg = self.minvel
      end
      self.minavg = self.minavg*self.g + vel*(1-self.g)
      if not self.maxavg then
	 self.maxavg = self.maxvel
      end
      self.maxavg = self.maxavg*self.g + vel*(1-self.g)
      if self.debug&2~=0 then
	 print(string.format("vel min = %g, max = %g", self.minavg, self.maxavg))
      end
   end
end

function arpeggio:velrange()
   if self.veltracker ~= 0 then
      local g = self.gain
      local min = self.minavg or self.minvel
      local max = self.maxavg or self.maxvel
      min = g*self.minvel + (1-g)*min
      max = g*self.maxvel + (1-g)*max
      return min, max
   else
      return self.minvel, self.maxvel
   end
end

-- output the next note in the pattern and switch to the next pulse
-- The result is a tuple notes, vel, gate, w, n, where vel is the velocity,
-- gate the gate value (normalized duration), w the pulse weight
-- (indispensability), and n the total number of pulses. The first return
-- value indicates the notes to play. This may either be a singleton number or
-- a list (which can also be empty, or contain multiple note numbers).
function arpeggio:pulse()
   local w, n = self:meter(self.idx)
   -- normalized pulse strength
   local w1 = w/math.max(1,n-1)
   -- corresponding MIDI velocity
   local minvel, maxvel = self:velrange()
   local vel =
      math.floor(mod_value(minvel, maxvel, self.velmod, w1))
   local gate, notes = 0, nil
   if self.loopstate == 1 and self.loopsize > 0 then
      -- notes come straight from the loop, input is ignored
      notes, vel, gate = table.unpack(self:loop_get())
      self.idx = (self.idx + 1) % self.beats
      return notes, vel, gate, w, n
   end
   if type(self.pattern) == "function" then
      notes = self.pattern(w1)
   elseif next(self.pattern) ~= nil then
      notes = cycle(self.pattern, self.idx+1)
   end
   if notes ~= nil then
      -- note filtering
      local ok = true
      local wmin, wmax = self.wmin, self.wmax
      if w1 >= wmin and w1 <= wmax then
	 local pmin, pmax = self.pmin, self.pmax
	 -- Calculate the filter probablity. We allow for negative pmod values
	 -- here, in which case stronger pulses tend to be filtered out first
	 -- rather than weaker ones.
	 local p = mod_value(pmin, pmax, self.pmod, w1)
	 local r = math.random()
	 if self.debug&4~=0 then
	    print(string.format("w = %g, wmin = %g, wmax = %g, p = %g, r = %g",
				w1, wmin, wmax, p, r))
	 end
	 ok = r <= p
      else
	 ok = false
      end
      if ok then
	 -- modulated gate value
	 gate = mod_value(0, self.gate, self.gatemod, w1)
	 -- output notes (there may be more than one in Raptor mode)
	 if self.debug&4~=0 then
	    print(string.format("idx = %g, notes = %s, vel = %g, gate = %g", self.idx, inspect(notes), vel, gate))
	 end
      else
	 notes = {}
      end
   else
      notes = {}
   end
   self:loop_add(notes, vel, gate)
   self.idx = (self.idx + 1) % self.beats
   return notes, vel, gate, w, n
end

-- panic clears the chord memory and pattern
function arpeggio:panic()
   self.chord = {}
   self.pattern = {}
   self.last_q = nil
   -- XXXFIXME: Catch 22 here. This method gets invoked when transport starts
   -- rolling (at which time Ardour sends a bunch of all-note-offs to all
   -- channels). Unfortunately, the following line would then override the
   -- latch control of the plugin, which we don't want. So we have to disable
   -- the following call for now. This means that even the panic button won't
   -- really get rid of the latched notes, you must turn off the latch control
   -- explicitly to make them go away. (However, the current pattern gets
   -- cleared anyway, so hopefully nobody will ever notice.)
   --self:set_latch(0)
   self:update_veltracker({}, 0)
end

-- change the current pulse index
function arpeggio:set_idx(x)
   x = self:intarg(x)
   if type(x) == "number" and self.idx ~= x then
      self.idx = math.max(0, x) % self.beats
      if self.loopstate == 1 then
	 self.loopidx = self.idx % math.max(1, math.min(#self.loop, self.loopsize))
      end
   end
end

-- pattern computation

local function transp(chord, i)
   return map(chord, function (n) return n+12*i end)
end

function arpeggio:pitchrange(a, b)
   if self.pitchtracker == 0 then
      -- just octave range
      a = math.max(0, math.min(127, a+12*self.down))
      b = math.max(0, math.min(127, b+12*self.up))
   elseif self.pitchtracker == 1 then
      -- full range tracker
      a = math.max(0, math.min(127, a+12*self.down+self.pitchlo))
      b = math.max(0, math.min(127, b+12*self.up+self.pitchhi))
   elseif self.pitchtracker == 2 then
      -- treble tracker
      a = math.max(0, math.min(127, b+12*self.down+self.pitchlo))
      b = math.max(0, math.min(127, b+12*self.up+self.pitchhi))
   elseif self.pitchtracker == 3 then
      -- bass tracker
      a = math.max(0, math.min(127, a+12*self.down+self.pitchlo))
      b = math.max(0, math.min(127, a+12*self.up+self.pitchhi))
   end
   return seq(a, b)
end

function arpeggio:create_pattern(chord)
   -- create a new pattern using the current settings
   local pattern = chord
   -- By default we do outside-in by alternating up-down (i.e., lo-hi), set
   -- this flag to true to get something more Logic-like which goes down-up.
   local logic_like = false
   if next(pattern) == nil then
      -- nothing to see here, move along...
      return pattern
   elseif self.raptor ~= 0 then
      -- Raptor mode: Pick random notes from the eligible range based on
      -- average Barlow harmonicities (cf. barlow.lua). This also combines
      -- with mode 0..5, employing the corresponding Raptor arpeggiation
      -- modes. Note that these patterns may contain notes that we're not
      -- actually playing, if they're harmonically related to the input
      -- chord. Raptor can also play chords rather than just single notes, and
      -- with the right settings you can make it go from plain tonal to more
      -- jazz-like and free to completely atonal, and everything in between.
      local a, b = pattern[1], pattern[#pattern]
      -- NOTE: As this kind of pattern is quite costly to compute, we
      -- implement it as a closure which gets evaluated lazily for each pulse,
      -- rather than precomputing the entire pattern at once as in the
      -- deterministic modes.
      if self.mode == 5 then
	 -- Raptor by itself doesn't support mode 5 (outside-in), so we
	 -- emulate it by alternating between mode 1 and 2. This isn't quite
	 -- the same, but it's as close to outside-in as I can make it. You
	 -- might also consider mode 0 (random) as a reasonable alternative
	 -- instead.
	 local cache, mode, dir
	 local function restart()
	    -- print("raptor: restart")
	    cache = {{}, {}}
	    if logic_like then
	       mode, dir = 2, -1
	    else
	       mode, dir = 1, 1
	    end
	 end
	 restart()
	 pattern = function(w1)
	    local notes, _
	    if w1 == 1 then
	       -- beginning of bar, restart pattern
	       restart()
	    end
	    notes, _ =
	       rand_notes(w1,
			  self.nmax, self.nmod,
			  self.hmin, self.hmax, self.hmod,
			  self.smin, self.smax, self.smod,
			  dir, mode, self.uniq ~= 0,
			  self.pref, self.prefmod,
			  cache[mode],
			  chord, self:pitchrange(a, b))
	    if next(notes) ~= nil then
	       cache[mode] = notes
	    end
	    if dir>0 then
	       mode, dir = 2, -1
	    else
	       mode, dir = 1, 1
	    end
	    return notes
	 end
      else
	 local cache, mode, dir
	 local function restart()
	    -- print("raptor: restart")
	    cache = {}
	    mode = self.mode
	    dir = 0
	    if mode == 1 or mode == 3 then
	       dir = 1
	    elseif mode == 2 or mode == 4 then
	       dir = -1
	    end
	 end
	 restart()
	 pattern = function(w1)
	    local notes
	    if w1 == 1 then
	       -- beginning of bar, restart pattern
	       restart()
	    end
	    notes, dir =
	       rand_notes(w1,
			  self.nmax, self.nmod,
			  self.hmin, self.hmax, self.hmod,
			  self.smin, self.smax, self.smod,
			  dir, mode, self.uniq ~= 0,
			  self.pref, self.prefmod,
			  cache,
			  chord, self:pitchrange(a, b))
	    if next(notes) ~= nil then
	       cache = notes
	    end
	    return notes
	 end
      end
   else
      -- apply the octave range (not used in raptor mode)
      pattern = {}
      for i = self.down, self.up do
	 pattern = tabcat(pattern, transp(chord, i))
      end
      if self.mode == 0 then
	 -- random: this is just the run-of-the-mill random pattern permutation
	 local n, pat = #pattern, {}
	 local p = seq(1, n)
	 for i = 1, n do
	    local j = math.random(i, n)
	    p[i], p[j] = p[j], p[i]
	 end
	 for i = 1, n do
	    pat[i] = pattern[p[i]]
	 end
	 pattern = pat
      elseif self.mode == 1 then
	 -- up (no-op)
      elseif self.mode == 2 then
	 -- down
	 pattern = reverse(pattern)
      elseif self.mode == 3 then
	 -- up-down
	 local r = reverse(pattern)
	 -- get rid of the repeated note in the middle
	 table.remove(pattern)
	 pattern = tabcat(pattern, r)
      elseif self.mode == 4 then
	 -- down-up
	 local r = reverse(pattern)
	 table.remove(r)
	 pattern = tabcat(reverse(pattern), pattern)
      elseif self.mode == 5 then
	 -- outside-in
	 local n, pat = #pattern, {}
	 local p, q = n//2, n%2
	 if logic_like then
	    for i = 1, p do
	       -- highest note first (a la Logic?)
	       pat[2*i-1] = pattern[n+1-i]
	       pat[2*i] = pattern[i]
	    end
	 else
	    for i = 1, p do
	       -- lowest note first (sounds better IMHO)
	       pat[2*i-1] = pattern[i]
	       pat[2*i] = pattern[n+1-i]
	    end
	 end
	 if q > 0 then
	    pat[n] = pattern[p+1]
	 end
	 pattern = pat
      end
   end
   if self.debug&1~=0 then
      print(string.format("chord = %s", inspect(chord)))
      print(string.format("pattern = %s", inspect(pattern)))
   end
   return pattern
end

-- latch: keep chord notes when released until new chord or reset
function arpeggio:set_latch(x)
   x = self:intarg(x)
   if type(x) == "number" then
      if x ~= 0 then
	 self.latch = {table.unpack(self.chord)}
      elseif self.latch then
	 self.latch = nil
	 self.pattern = self:create_pattern(self.chord)
      end
   end
end

function arpeggio:get_chord()
   return self.latch and self.latch or self.chord
end

-- change the range of the pattern
function arpeggio:set_up(x)
   x = self:intarg(x)
   if type(x) == "number" then
      self.up = math.max(-2, math.min(2, x))
      self.pattern = self:create_pattern(self:get_chord())
   end
end

function arpeggio:set_down(x)
   x = self:intarg(x)
   if type(x) == "number" then
      self.down = math.max(-2, math.min(2, x))
      self.pattern = self:create_pattern(self:get_chord())
   end
end

function arpeggio:set_pitchtracker(x)
   x = self:intarg(x)
   if type(x) == "number" then
      self.pitchtracker = math.max(0, math.min(3, x))
      self.pattern = self:create_pattern(self:get_chord())
   end
end

function arpeggio:set_pitchlo(x)
   x = self:intarg(x)
   if type(x) == "number" then
      self.pitchlo = math.max(-36, math.min(36, x))
      self.pattern = self:create_pattern(self:get_chord())
   end
end

function arpeggio:set_pitchhi(x)
   x = self:intarg(x)
   if type(x) == "number" then
      self.pitchhi = math.max(-36, math.min(36, x))
      self.pattern = self:create_pattern(self:get_chord())
   end
end

-- change the mode (up, down, etc.)
function arpeggio:set_mode(x)
   x = self:intarg(x)
   if type(x) == "number" then
      self.mode = math.max(0, math.min(5, x))
      self.pattern = self:create_pattern(self:get_chord())
   end
end

-- this enables Raptor mode with randomized note output
function arpeggio:set_raptor(x)
   x = self:intarg(x)
   if type(x) == "number" then
      self.raptor = math.max(0, math.min(1, x))
      self.pattern = self:create_pattern(self:get_chord())
   end
end

-- change min/max velocities, gate, and note probabilities
function arpeggio:set_minvel(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.minvel = math.max(0, math.min(127, x))
   end
end

function arpeggio:set_maxvel(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.maxvel = math.max(0, math.min(127, x))
   end
end

function arpeggio:set_velmod(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.velmod = math.max(-1, math.min(1, x))
   end
end

function arpeggio:set_veltracker(x)
   x = self:intarg(x)
   if type(x) == "number" then
      self.veltracker = math.max(0, math.min(1, x))
   end
end

function arpeggio:set_gain(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.gain = math.max(0, math.min(1, x))
   end
end

function arpeggio:set_gate(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.gate = math.max(0, math.min(10, x))
   end
end

function arpeggio:set_gatemod(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.gatemod = math.max(-1, math.min(1, x))
   end
end

function arpeggio:set_pmin(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.pmin = math.max(0, math.min(1, x))
   end
end

function arpeggio:set_pmax(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.pmax = math.max(0, math.min(1, x))
   end
end

function arpeggio:set_pmod(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.pmod = math.max(-1, math.min(1, x))
   end
end

function arpeggio:set_wmin(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.wmin = math.max(0, math.min(1, x))
   end
end

function arpeggio:set_wmax(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.wmax = math.max(0, math.min(1, x))
   end
end

-- change the raptor parameters (harmonicity, etc.)
function arpeggio:set_nmax(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.nmax = math.max(0, math.min(10, x))
   end
end

function arpeggio:set_nmod(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.nmod = math.max(-1, math.min(1, x))
   end
end

function arpeggio:set_hmin(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.hmin = math.max(0, math.min(1, x))
   end
end

function arpeggio:set_hmax(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.hmax = math.max(0, math.min(1, x))
   end
end

function arpeggio:set_hmod(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.hmod = math.max(-1, math.min(1, x))
   end
end

function arpeggio:set_smin(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.smin = math.max(-127, math.min(127, x))
   end
end

function arpeggio:set_smax(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.smax = math.max(-127, math.min(127, x))
   end
end

function arpeggio:set_smod(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.smod = math.max(-1, math.min(1, x))
   end
end

function arpeggio:set_uniq(x)
   x = self:intarg(x)
   if type(x) == "number" then
      self.uniq = math.max(0, math.min(1, x))
   end
end

function arpeggio:set_pref(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.pref = math.max(-1, math.min(1, x))
   end
end

function arpeggio:set_prefmod(x)
   x = self:numarg(x)
   if type(x) == "number" then
      self.prefmod = math.max(-1, math.min(1, x))
   end
end

local function update_chord(chord, note, vel)
   -- update the chord memory, keeping the notes in ascending order
   local n = #chord
   if n == 0 then
      if vel > 0 then
	 table.insert(chord, 1, note)
      end
      return chord
   end
   for i = 1, n do
      if chord[i] == note then
	 if vel <= 0 then
	    -- note off: remove note
	    if i < n then
	       table.move(chord, i+1, n, i)
	    end
	    table.remove(chord)
	 end
	 return chord
      elseif chord[i] > note then
	 if vel > 0 then
	    -- insert note
	    table.insert(chord, i, note)
	 end
	 return chord
      end
   end
   -- if we come here, no note has been inserted or deleted yet
   if vel > 0 then
      -- note is larger than all present notes in chord, so it needs to be
      -- inserted at the end
      table.insert(chord, note)
   end
   return chord
end

-- note input; update the internal chord memory and recompute the pattern
function arpeggio:note(note, vel)
   if self.debug&2~=0 then
      print(string.format("note = %s", inspect({ note, vel })))
   end
   if type(note) == "number" and type(vel) == "number" then
      if self.latch and next(self.chord) == nil and vel>0 then
	 -- start new pattern
	 self.latch = {}
      end
      update_chord(self.chord, note, vel)
      if self.latch and vel>0 then
	 update_chord(self.latch, note, vel)
      end
      self.pattern = self:create_pattern(self:get_chord())
      self:update_veltracker(self:get_chord(), vel)
   end
end

-- this recomputes all indispensability tables
function arpeggio:prepare_meter(meter)
   local n = 1
   local m = {}
   if type(meter) ~= "table" then
      -- assume singleton number
      meter = { meter }
   end
   for _,q in ipairs(meter) do
      if q ~= math.floor(q) then
	 error("arpeggio: meter levels must be integer")
	 return
      elseif q < 1 then
	 error("arpeggio: meter levels must be positive")
	 return
      end
      -- factorize each level as Barlow's formula assumes primes
      m = tabcat(m, factor(q))
      n = n*q
   end
   self.beats = n
   self.last_q = nil
   if n > 1 then
      self.indisp[1] = indisp(m)
      for q = 2, self.n do
	 local qs = tabcat(m, factor(q))
	 self.indisp[q] = indisp(qs)
      end
   else
      self.indisp[1] = {0}
      for q = 2, self.n do
	 self.indisp[q] = indisp(q)
      end
   end
end

-- set a new meter (given either as a singleton number or as a list of
-- numbers) and return the number of pulses
function arpeggio:set_meter(meter)
   self:prepare_meter(meter)
   return self.beats
end

-- -------------------------------------------------------------------------

-- Ardour interface (this is mostly like barlow_arp)

-- debug level: This only affects the plugin code. 1: print the current beat
-- and other important state information, 3: also print note input, 4: print
-- everything, including note output. Output goes to Ardour's log window.
-- NOTE: To debug the internal state of the arpeggiator object, including
-- pattern changes and note generation, use the arp.debug setting below.
local debug = 0

function dsp_ioconfig ()
   return { { midi_in = 1, midi_out = 1, audio_in = -1, audio_out = -1}, }
end

function dsp_options ()
   -- NOTE: We need regular_block_length = true in this plugin to get rid of
   -- some intricate timing issues with scheduled note-offs for gated notes
   -- right at the end of a loop. This sometimes causes hanging notes with
   -- automation when transport wraps around to the loop start. It's unclear
   -- whether the issue is in Ardour (caused by split cycles with automation)
   -- or some unkown bug in the plugin. But the option makes it go away (which
   -- seems to indicate that the issue is on the Ardour side).
   return { time_info = true, regular_block_length = true }
end

local hrm_scalepoints = { ["0.09 (minor 7th and 3rd)"] = 0.09, ["0.1 (major 2nd and 3rd)"] = 0.1, ["0.17 (4th)"] = 0.17, ["0.21 (5th)"] = 0.21, ["1 (unison, octave)"] = 1 }

local params = {
   { type = "input", name = "bypass", min = 0, max = 1, default = 0, toggled = true, doc = "bypass the arpeggiator, pass through input notes" },
   { type = "input", name = "division", min = 1, max = 7, default = 1, integer = true, doc = "number of subdivisions of the beat" },
   { type = "input", name = "pgm", min = 0, max = 128, default = 0, integer = true, doc = "program change", scalepoints = { default = 0 } },
   { type = "input", name = "latch", min = 0, max = 1, default = 0, toggled = true, doc = "toggle latch mode" },
   { type = "input", name = "up", min = -2, max = 2, default = 1, integer = true, doc = "octave range up" },
   { type = "input", name = "down", min = -2, max = 2, default = -1, integer = true, doc = "octave range down" },
   -- Raptor's usual default for the pattern is 0 = random, but 1 = up
   -- seems to be a more sensible choice.
   { type = "input", name = "mode", min = 0, max = 5, default = 1, enum = true, doc = "pattern style",
     scalepoints =
	{ ["0 random"] = 0, ["1 up"] = 1, ["2 down"] = 2, ["3 up-down"] = 3, ["4 down-up"] = 4, ["5 outside-in"] = 5 } },
   { type = "input", name = "raptor", min = 0, max = 1, default = 0, toggled = true, doc = "toggle raptor mode" },
   { type = "input", name = "minvel", min = 0, max = 127, default = 60, integer = true, doc = "minimum velocity" },
   { type = "input", name = "maxvel", min = 0, max = 127, default = 120, integer = true, doc = "maximum velocity" },
   { type = "input", name = "velmod", min = -1, max = 1, default = 1, doc = "automatic velocity modulation according to current pulse strength" },
   { type = "input", name = "gain", min = 0, max = 1, default = 1, doc = "wet/dry mix between input velocity and set values (min/max velocity)" },
   -- Pd Raptor allows this to go from 0 to 1000%, but we only support
   -- 0-100% here
   { type = "input", name = "gate", min = 0, max = 1, default = 1, doc = "gate as fraction of pulse length", scalepoints = { legato = 0 } },
   { type = "input", name = "gatemod", min = -1, max = 1, default = 0, doc = "automatic gate modulation according to current pulse strength" },
   { type = "input", name = "wmin", min = 0, max = 1, default = 0, doc = "minimum note weight" },
   { type = "input", name = "wmax", min = 0, max = 1, default = 1, doc = "maximum note weight" },
   { type = "input", name = "pmin", min = 0, max = 1, default = 0.3, doc = "minimum note probability" },
   { type = "input", name = "pmax", min = 0, max = 1, default = 1, doc = "maximum note probability" },
   { type = "input", name = "pmod", min = -1, max = 1, default = 0, doc = "automatic note probability modulation according to current pulse strength" },
   { type = "input", name = "hmin", min = 0, max = 1, default = 0, doc = "minimum harmonicity", scalepoints = hrm_scalepoints },
   { type = "input", name = "hmax", min = 0, max = 1, default = 1, doc = "maximum harmonicity", scalepoints = hrm_scalepoints },
   { type = "input", name = "hmod", min = -1, max = 1, default = 0, doc = "automatic harmonicity modulation according to current pulse strength" },
   { type = "input", name = "pref", min = -1, max = 1, default = 1, doc = "harmonic preference" },
   { type = "input", name = "prefmod", min = -1, max = 1, default = 0, doc = "automatic harmonic preference modulation according to current pulse strength" },
   { type = "input", name = "smin", min = -12, max = 12, default = 1, integer = true, doc = "minimum step size" },
   { type = "input", name = "smax", min = -12, max = 12, default = 7, integer = true, doc = "maximum step size" },
   { type = "input", name = "smod", min = -1, max = 1, default = 0, doc = "automatic step size modulation according to current pulse strength" },
   { type = "input", name = "nmax", min = 0, max = 10, default = 1, integer = true, doc = "maximum polyphony (number of simultaneous notes)" },
   { type = "input", name = "nmod", min = -1, max = 1, default = 0, doc = "automatic modulation of the number of notes according to current pulse strength" },
   { type = "input", name = "uniq", min = 0, max = 1, default = 1, toggled = true, doc = "don't repeat notes in consecutive steps" },
   { type = "input", name = "pitchhi", min = -36, max = 36, default = 0, integer = true, doc = "extended pitch range up in semitones (raptor mode)" },
   { type = "input", name = "pitchlo", min = -36, max = 36, default = 0, integer = true, doc = "extended pitch range down in semitones (raptor mode)" },
   { type = "input", name = "pitchtracker", min = 0, max = 3, default = 0, enum = true, doc = "pitch tracker mode, follow input to adjust the pitch range (raptor mode)",
     scalepoints =
	{ ["0 off"] = 0, ["1 on"] = 1, ["2 treble"] = 2, ["3 bass"] = 3 } },
   { type = "input", name = "inchan", min = 0, max = 16, default = 0, integer = true, doc = "input channel (0 = omni = all channels)", scalepoints = { omni = 0 } },
   { type = "input", name = "outchan", min = 0, max = 16, default = 0, integer = true, doc = "input channel (0 = omni = input channel)", scalepoints = { omni = 0 } },
   { type = "input", name = "loopsize", min = 0, max = 16, default = 4, integer = true, doc = "loop size (number of bars)" },
   { type = "input", name = "loop", min = 0, max = 1, default = 0, toggled = true, doc = "toggle loop mode" },
   { type = "input", name = "mute", min = 0, max = 1, default = 0, toggled = true, doc = "turn the arpeggiator off, suppress all note output" },
}

local n_params = #params
local int_param = map(params, function(x) return x.integer == true or x.enum == true or x.toggled == true end)

function dsp_params ()
   return params
end

-- This is basically a collection of presets from the Pd external, with some
-- (very) minor adjustments / bugfixes where I saw fit. The program numbers
-- assume a GM patch set, if your synth isn't GM-compatible then you'll have
-- to adjust them accordingly. NOTE: The tr808 preset assumes a GM-compatible
-- drumkit, so it outputs through MIDI channel 10 by default; other presets
-- leave the output channel as is.

local raptor_presets = {
   { name = "default", params = { bypass = 0, latch = 0, division = 1, pgm = 0, up = 1, down = -1, mode = 1, raptor = 0, minvel = 60, maxvel = 120, velmod = 1, gain = 1, gate = 1, gatemod = 0, wmin = 0, wmax = 1, pmin = 0.3, pmax = 1, pmod = 0, hmin = 0, hmax = 1, hmod = 0, pref = 1, prefmod = 0, smin = 1, smax = 7, smod = 0, nmax = 1, nmod = 0, uniq = 1, pitchhi = 0, pitchlo = 0, pitchtracker = 0, inchan = 0, outchan = 0, loopsize = 4, loop = 0, mute = 0 } },
   { name = "arp", params = { pgm = 26, up = 0, down = -1, mode = 3, raptor = 1, minvel = 105, maxvel = 120, velmod = 1, gain = 0.5, gate = 1, gatemod = 0, wmin = 0, wmax = 1, pmin = 0.9, pmax = 1, pmod = -1, hmin = 0.11, hmax = 1, hmod = 0, pref = 0.8, prefmod = 0, smin = 2, smax = 7, smod = 0, nmax = 1, nmod = 0, uniq = 1, pitchhi = 0, pitchlo = -12, pitchtracker = 2, loopsize = 4 } },
   { name = "bass", params = { pgm = 35, up = 0, down = -1, mode = 3, raptor = 1, minvel = 40, maxvel = 120, velmod = 1, gain = 0.5, gate = 1, gatemod = 0, wmin = 0, wmax = 1, pmin = 0.2, pmax = 1, pmod = 1, hmin = 0.12, hmax = 1, hmod = 0.1, pref = 0.8, prefmod = 0.1, smin = 2, smax = 7, smod = 0, nmax = 1, nmod = 0, uniq = 1, pitchhi = 7, pitchlo = 0, pitchtracker = 3, loopsize = 4 } },
   { name = "piano", params = { pgm = 1, up = 1, down = -1, mode = 0, raptor = 1, minvel = 90, maxvel = 120, velmod = 1, gain = 0.5, gate = 1, gatemod = 0, wmin = 0, wmax = 1, pmin = 0.4, pmax = 1, pmod = 1, hmin = 0.14, hmax = 1, hmod = 0.1, pref = 0.6, prefmod = 0.1, smin = 2, smax = 5, smod = 0, nmax = 2, nmod = 0, uniq = 1, pitchhi = 0, pitchlo = -18, pitchtracker = 2, loopsize = 4 } },
   { name = "raptor", params = { pgm = 5, up = 1, down = -2, mode = 0, raptor = 1, minvel = 60, maxvel = 120, velmod = 1, gain = 0.5, gate = 1, gatemod = 0, wmin = 0, wmax = 1, pmin = 0.4, pmax = 0.9, pmod = 0, hmin = 0.09, hmax = 1, hmod = -1, pref = 1, prefmod = 1, smin = 1, smax = 7, smod = 0, nmax = 3, nmod = -1, uniq = 0, pitchhi = 0, pitchlo = 0, pitchtracker = 0, loopsize = 4 } },
   -- some variations of the raptor preset for different instruments
   { name = "raptor-arp", params = { pgm = 26, up = 0, down = -1, mode = 3, raptor = 1, minvel = 105, maxvel = 120, velmod = 1, gain = 0.5, gate = 1, gatemod = 0, wmin = 0, wmax = 1, pmin = 0.4, pmax = 0.9, pmod = 0, hmin = 0.09, hmax = 1, hmod = -1, pref = 1, prefmod = 1, smin = 2, smax = 7, smod = 0, nmax = 1, nmod = 0, uniq = 1, pitchhi = 0, pitchlo = -12, pitchtracker = 2, loopsize = 4 } },
   { name = "raptor-bass", params = { pgm = 35, up = 0, down = -1, mode = 3, raptor = 1, minvel = 40, maxvel = 120, velmod = 1, gain = 0.5, gate = 1, gatemod = 0, wmin = 0, wmax = 1, pmin = 0.4, pmax = 0.9, pmod = 0, hmin = 0.09, hmax = 1, hmod = -1, pref = 1, prefmod = -0.6, smin = 2, smax = 7, smod = 0, nmax = 1, nmod = 0, uniq = 1, pitchhi = 7, pitchlo = -6, pitchtracker = 3, loopsize = 4 } },
   { name = "raptor-piano", params = { pgm = 1, up = 1, down = -1, mode = 0, raptor = 1, minvel = 90, maxvel = 120, velmod = 1, gain = 0.5, gate = 1, gatemod = 0, wmin = 0, wmax = 1, pmin = 0.4, pmax = 0.9, pmod = 0, hmin = 0.09, hmax = 1, hmod = -1, pref = -0.4, prefmod = -0.6, smin = 2, smax = 5, smod = 0, nmax = 2, nmod = 0, uniq = 1, pitchhi = 0, pitchlo = -18, pitchtracker = 2, loopsize = 4 } },
   { name = "raptor-solo", params = { pgm = 25, up = 0, down = -1, mode = 3, raptor = 1, minvel = 40, maxvel = 110, velmod = 0.5, gain = 0.5, gate = 1, gatemod = 0.5, wmin = 0, wmax = 1, pmin = 0.2, pmax = 0.9, pmod = 0.5, hmin = 0.09, hmax = 1, hmod = -1, pref = -0.4, prefmod = 0, smin = 1, smax = 7, smod = 0, nmax = 1, nmod = 0, uniq = 1, pitchhi = 0, pitchlo = 0, pitchtracker = 0, loopsize = 4 } },
   { name = "tr808", params = { pgm = 26, outchan = 10, up = 0, down = 0, mode = 1, raptor = 0, minvel = 60, maxvel = 120, velmod = 1, gain = 0.5, gate = 1, gatemod = 0, wmin = 0, wmax = 1, pmin = 0.3, pmax = 1, pmod = 0, hmin = 0, hmax = 1, hmod = 0, pref = 1, prefmod = 0, smin = 1, smax = 7, smod = 0, nmax = 1, nmod = 0, uniq = 1, pitchhi = 0, pitchlo = 0, pitchtracker = 0, loopsize = 4 } },
   { name = "vibes", params = { pgm = 12, up = 0, down = -1, mode = 3, raptor = 1, minvel = 84, maxvel = 120, velmod = 1, gain = 0.5, gate = 1, gatemod = 0, wmin = 0, wmax = 1, pmin = 0.9, pmax = 1, pmod = -1, hmin = 0.14, hmax = 1, hmod = 0.1, pref = 0.6, prefmod = 0.1, smin = 2, smax = 5, smod = 0, nmax = 2, nmod = 0, uniq = 1, pitchhi = -5, pitchlo = -16, pitchtracker = 2, loopsize = 4 } },
   { name = "weirdmod", params = { pgm = 25, up = 0, down = -1, mode = 5, raptor = 0, minvel = 40, maxvel = 110, velmod = 0.5, gain = 0.5, gate = 1, gatemod = 0.5, wmin = 0, wmax = 1, pmin = 0.2, pmax = 0.9, pmod = 0.5, hmin = 0, hmax = 1, hmod = 0, pref = 1, prefmod = 0, smin = 1, smax = 7, smod = 0, nmax = 1, nmod = 0, uniq = 1, pitchhi = 0, pitchlo = 0, pitchtracker = 0, loopsize = 4 } },
}

function presets()
   return raptor_presets
end

-- pertinent state information, to detect changes
local last_rolling -- last transport status, to detect changes
local last_beat -- last beat number
local last_p -- last pulse index from bbt
local last_bypass -- last bypass toggle
local last_mute -- last mute toggle
-- previous param values, to detect changes
local last_param = {}

-- pertinent note information, to handle note input and output
local chan = 0 -- MIDI (input and) output channel
local last_notes -- last notes played
local last_chan -- MIDI channel of the last notes
local off_gate -- off time of last notes (sample time)
local inchan, outchan, pgm = 0, 0, 0

-- create the arpeggiator (default meter)
local last_m = 4 -- last division, to detect changes
local arp = arpeggio:new(4)

-- Debugging output from the arpeggiator object (bitmask):
-- 1 = pattern, 2 = input, 4 = output (e.g., 7 means "all")
-- This is intended for debugging purposes only. it spits out *a lot* of
-- cryptic debug messages in the log window, so it's better to keep this
-- disabled in production code.
--arp.debug = 7

-- param setters

local function arp_set_loopsize(self, x)
   -- need to translate beat numbers to steps
   self:set_loopsize(x*arp.beats)
end

local param_set = { nil, nil, function (_, x) pgm = x end, arp.set_latch, arp.set_up, arp.set_down, arp.set_mode, arp.set_raptor, arp.set_minvel, arp.set_maxvel, arp.set_velmod, arp.set_gain, arp.set_gate, arp.set_gatemod, arp.set_wmin, arp.set_wmax, arp.set_pmin, arp.set_pmax, arp.set_pmod, arp.set_hmin, arp.set_hmax, arp.set_hmod, arp.set_pref, arp.set_prefmod, arp.set_smin, arp.set_smax, arp.set_smod, arp.set_nmax, arp.set_nmod, arp.set_uniq, arp.set_pitchhi, arp.set_pitchlo, arp.set_pitchtracker, function (_, x) inchan = x end, function (_, x) outchan = x end, arp_set_loopsize, arp.set_loop, nil }

local function get_chan(ch)
   if outchan == 0 and inchan > 0 then
      ch = inchan-1 -- outchan == inchan > 0 override
   elseif outchan > 0 then
      ch = outchan-1 -- outchan > 0 override
   end
   return ch
end

local function check_chan(ch)
   return inchan == 0 or ch == inchan-1
end

function dsp_run (_, _, n_samples)
   assert (type(midiout) == "table")
   assert (type(time) == "table")
   assert (type(midiout) == "table")

   local ctrl = CtrlPorts:array ()
   local subdiv = math.floor(ctrl[2])
   local loopsize = math.floor(ctrl[n_params-2])
   -- bypass toggle
   local bypass = ctrl[1] > 0
   -- mute toggle
   local mute = ctrl[n_params] > 0
   -- rolling state: It seems that we need to check the transport state (as
   -- given by Ardour's "transport finite state machine" = TFSM) here, even if
   -- the transport is not actually moving yet. Otherwise some input notes may
   -- errorneously slip through before playback really starts.
   local rolling = Session:transport_state_rolling ()

   -- detect param changes (subdiv is caught as a meter change below)
   local last_pgm = pgm
   local last_inchan = inchan
   for i = 1, n_params do
      v = ctrl[i]
      if int_param[i] then
	 -- Force integer values. (The GUI enforces this, but fractional
	 -- values might occur through automation.)
	 v = math.floor(v)
      end
      if param_set[i] and v ~= last_param[i] then
	 last_param[i] = v
	 param_set[i](arp, v)
      end
   end

   local all_notes_off = false
   if bypass ~= last_bypass then
      last_bypass = bypass
      all_notes_off = true
   end

   if mute ~= last_mute then
      last_mute = mute
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

   if inchan ~= last_inchan and inchan > 0 then
      -- input channel has changed, kill off chord memory
      arp:panic()
      all_notes_off = true
   end

   local k = 1
   if all_notes_off then
      --print("all-notes-off", chan)
      midiout[k] = { time = 1, data = { 0xb0+chan, 123, 0 } }
      k = k+1
   end

   if pgm ~= last_pgm or get_chan(chan) ~= chan then
      -- program or output channel has changed, send the program change
      chan = get_chan(chan)
      if pgm > 0 then
	 midiout[k] = { time = 1, data = { 0xc0+chan, pgm-1 } }
	 k = k+1
      end
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
	 if status == 0xb0 and (num == 123 or num == 64) then
	    -- Better to skip these CCs (generated by Ardour to prevent
	    -- hanging notes when relocating the playback position, e.g.,
	    -- during loop playback). This avoids notes being cut short
	    -- further down in the signal path. Also, we don't want those
	    -- messages to proliferate if our MIDI gets sent off to another
	    -- track. Unfortunately, there's no way to check whether these
	    -- events are synthetic or user input. So it seems best to just
	    -- ignore them.
	 else
	    midiout[k] = ev
	    k = k+1
	 end
      end
      if status == 0x80 or status == 0x90 and val == 0 then
	 if check_chan(ch) then
	    if debug >= 4 then
	       print("note off", num, val)
	    end
	    arp:note(num, 0)
	 end
      elseif status == 0x90 then
	 if check_chan(ch) then
	    if debug >= 4 then
	       print("note on", num, val, "ch", ch)
	    end
	    arp:note(num, val)
	    chan = get_chan(ch)
	 end
      elseif not rolling and status == 0xb0 and num == 123 and ch == chan then
	 -- This disrupts the arpeggiator during playback, so we only process
	 -- these messages (generated by Ardour to prevent hanging notes when
	 -- relocating the playback position) if transport is stopped.
	 if debug >= 4 then
	    print("all notes off")
	 end
	 arp:panic()
      end
   end

   if rolling and not bypass and not mute then
      -- transport is rolling, not bypassed, so the arpeggiator is playing
      local function notes_off(ts)
	 if last_notes then
	    -- kill the old notes
	    for _, num in ipairs(last_notes) do
	       if debug >= 3 then
		  print("note off", num)
	       end
	       midiout[k] = { time = ts, data = { 0x80+last_chan, num, 100 } }
	       k = k+1
	    end
	    last_notes = nil
	 end
      end
      if off_gate and last_notes and
	 off_gate >= time.sample and off_gate < time.sample_end then
	 -- Gated notes don't normally fall on a beat, so we detect them
	 -- here. (If the gate time hasn't been set or we miss it, then the
	 -- note-offs will be taken care of when the next notes get triggered,
	 -- see below.)
	 -- sample-accurate "off" time
	 local ts = off_gate - time.sample + 1
	 notes_off(ts)
      end
      -- Check whether a beat is due, so that we trigger the next notes. We
      -- want to do this in a sample-accurate manner in order to avoid jitter,
      -- check barlow_arp.lua for details.
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
	 -- current meter (divisions per bar * subdivisions)
	 local m = meter:divisions_per_bar() * subdiv
	 -- detect meter changes
	 if m ~= last_m then
	    last_m = m
	    arp:set_meter(m)
	    -- we also need to update the loop size here
	    arp_set_loopsize(arp, loopsize)
	 end
	 -- calculate a fractional pulse number from the current bbt
	 local p = bbt.beats-1 + math.max(0, bbt.ticks) / Temporal.ticks_per_beat
	 -- round to current pulse index
	 p = math.floor(p * subdiv)
	 if p == last_p then
	    -- Avoid triggering the same pulse twice (probably a timing issue
	    -- which seems to happen when the playback position is relocated,
	    -- e.g., at the beginning of a loop).
	    goto skip
	 end
	 last_p = p
	 -- grab some notes from the arpeggiator
	 arp:set_idx(p) -- in case we've changed position
	 local notes, vel, gate, w, n = arp:pulse()
	 -- Make sure that the gate is clamped to the 0-1 range, since we
	 -- don't support overlapping notes in the current implementation.
	 gate = math.max(0, math.min(1, gate))
	 --print(string.format("[%d] notes", p), inspect(notes), vel, gate, w, n)
	 -- the arpeggiator may return a singleton note, make sure that it's
	 -- always a list
	 if type(notes) ~= "table" then
	    notes = { notes }
	 end
	 -- calculate the note-off time in samples, this is used if the gate
	 -- control is neither 0 nor 1
	 local gate_ts = ts + math.floor(tm:bbt_duration_at(pos, Temporal.BBT_Offset(0,1,0)):samples() / subdiv * gate)
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
	 if not legato then
	    notes_off(ts)
	 end
	 if next(notes) ~= nil then
	    if legato then
	       notes_off(ts)
	    end
	    for i, num in ipairs(notes) do
	       if debug >= 3 then
		  print("note on", num, vel)
	       end
	       midiout[k] = { time = ts, data = { 0x90+chan, num, vel } }
	       k = k+1
	    end
	    last_notes = notes
	    last_chan = chan
	    if gate < 1 and not legato then
	       -- Set the sample time at which the note-offs are due.
	       off_gate = gate_ts
	    else
	       -- Otherwise don't set the off time in which case the
	       -- note-offs gets triggered automatically above.
	       off_gate = nil
	    end
	 end
	 ::skip::
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
