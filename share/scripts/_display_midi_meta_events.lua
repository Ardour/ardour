-- ==========================================================================
-- Display MIDI Meta Events
-- ==========================================================================
--
-- This Ardour Lua script inspects MIDI regions and displays their metadata,
-- combining two complementary data sources:
--
--   1. **Model-based API** — Ardour's in-memory MidiModel, accessed via
--      the Lua bindings (ARDOUR.LuaAPI).  The model stores text-type
--      meta events (types 0x01–0x09) alongside notes, CCs and patch
--      changes.  The model API is authoritative for what Ardour "sees"
--      at runtime.
--
--   2. **Raw SMF file parser** — a self-contained MIDI file reader written
--      in pure Lua.  It parses ALL meta events in the on-disk file,
--      including types that Ardour deliberately does not load into the
--      model (tempo 0x51, time signature 0x58, key signature 0x59,
--      Ardour/Evoral note IDs 0x7F, end-of-track 0x2F, etc.).
--      This provides a complete picture of the file's content.
--
-- By showing both views side by side, the user can:
--   - Verify which meta events survived the import into Ardour's model
--   - See tempo/key/time-signature metadata that lives outside the model
--   - Compare an original external MIDI file with the Ardour interchange copy
--
-- **References:**
--   - "Standard MIDI Files 1.0" (RP-001 v1.0), in The Complete MIDI 1.0
--     Detailed Specification, Document Version 96.1.
--     MIDI Manufacturers Association, Los Angeles, CA, USA, 1996.
--   - "Recommended Practice RP-019: SMF Device Name and Program Name
--     Meta Events." Approved by MMA 4/10/98, AMEI 5/7/99.
--     Copyright 1999 MIDI Manufacturers Association Incorporated.
--
-- ==========================================================================

ardour {
  ["type"] = "EditorAction",
  name = "Display MIDI Meta Events",
  author = "Ardour Team",
  description = [[Displays all MIDI metadata (meta events, patch changes, note count)
for selected MIDI regions, from both the Ardour model and the raw SMF file.
Meta events include text, tempo, time/key signatures, etc.
Full output is always sent to Window > Log; a summary dialog is also shown.
If no regions are selected, all MIDI regions in the session are processed.]]
}

function factory () return function ()

  ---------------------------------------------------------------------------
  -- Variable Length Quantity (VLQ) decoder
  ---------------------------------------------------------------------------
  -- VLQ is the encoding used throughout MIDI files for delta times and
  -- meta-event lengths.  Each byte contributes 7 data bits; the MSB
  -- (bit 7) is a continuation flag.  A valid VLQ is at most 4 bytes
  -- (28 bits), which is enough for any value up to 0x0FFFFFFF.
  --
  -- Example:  0x83 0x2A  →  (0x03 << 7) | 0x2A  =  426
  ---------------------------------------------------------------------------
  local function read_vlq (f)
    local val = 0
    for _ = 1, 4 do  -- at most 4 bytes in valid MIDI
      local b = f:read (1)
      if not b then return nil end
      b = b:byte ()
      val = (val << 7) | (b & 0x7F)
      if (b & 0x80) == 0 then return val end
    end
    return nil  -- malformed: more than 4 continuation bytes
  end

  ---------------------------------------------------------------------------
  -- Meta event type names
  ---------------------------------------------------------------------------
  -- The SMF 1.0 specification (RP-001) defines meta events as 0xFF <type>
  -- <length> <data>.  The type byte identifies the kind of metadata.
  -- Types 0x01–0x07 are defined in RP-001; types 0x08 (Program Name) and
  -- 0x09 (Device Name) were added by RP-019 (approved 1998/1999).
  ---------------------------------------------------------------------------
  local META_NAMES = {
    [0x00] = "Sequence Number",   -- RP-001: optional, must be at tick 0
    [0x01] = "Text",              -- RP-001: free-form text annotation
    [0x02] = "Copyright",         -- RP-001: copyright notice
    [0x03] = "Track Name",        -- RP-001: track or sequence name
    [0x04] = "Instrument Name",   -- RP-001: instrument used on this track
    [0x05] = "Lyric",             -- RP-001: song lyric (one syllable per event)
    [0x06] = "Marker",            -- RP-001: rehearsal mark or section label
    [0x07] = "Cue Point",         -- RP-001: description of stage action
    [0x08] = "Program Name",      -- RP-019: name of the program/patch
    [0x09] = "Device Name",       -- RP-019: name of the target MIDI device
    [0x20] = "Channel Prefix",    -- RP-001: applies subsequent meta events to a channel
    [0x21] = "MIDI Port",         -- unofficial but widely used
    [0x2F] = "End of Track",      -- RP-001: mandatory, marks end of track
    [0x51] = "Tempo",             -- RP-001: microseconds per quarter note
    [0x54] = "SMPTE Offset",      -- RP-001: starting SMPTE time code
    [0x58] = "Time Signature",    -- RP-001: numerator, denominator, clocks, 32nds
    [0x59] = "Key Signature",     -- RP-001: sharps/flats count + major/minor
    [0x7F] = "Sequencer-Specific",-- RP-001: vendor-private data
  }

  ---------------------------------------------------------------------------
  -- Decode a meta event's binary payload into a human-readable string
  ---------------------------------------------------------------------------
  -- This function interprets the raw bytes according to the meta type.
  -- Text types (0x01–0x09) are returned as-is (they are ASCII/Latin-1).
  -- Structured types (tempo, time sig, key sig, etc.) are decoded into
  -- meaningful values.  Unknown or unrecognized payloads get a hex dump.
  ---------------------------------------------------------------------------
  local function decode_meta (meta_type, meta_data)
    -- Text events (0x01–0x09): just return the string content.
    -- abc2midi notably uses 0x01 (Text) for lyrics instead of 0x05 (Lyric).
    if meta_type >= 0x01 and meta_type <= 0x09 then
      return meta_data

    -- Sequence Number: 2-byte big-endian integer
    elseif meta_type == 0x00 then
      if #meta_data >= 2 then
        return string.format ("%d", (meta_data:byte (1) << 8) | meta_data:byte (2))
      end

    -- Channel Prefix: subsequent meta events apply to this channel
    elseif meta_type == 0x20 then
      if #meta_data >= 1 then
        return string.format ("channel %d", meta_data:byte (1))
      end

    -- MIDI Port: unofficial but common
    elseif meta_type == 0x21 then
      if #meta_data >= 1 then
        return string.format ("port %d", meta_data:byte (1))
      end

    -- Tempo: 3-byte big-endian microseconds-per-quarter-note.
    -- BPM = 60,000,000 / usec.  Example: 500000 µs = 120 BPM.
    elseif meta_type == 0x51 then
      if #meta_data >= 3 then
        local usec = (meta_data:byte (1) << 16)
                   | (meta_data:byte (2) << 8)
                   |  meta_data:byte (3)
        return string.format ("%.3f BPM  (%d us/beat)", 60000000.0 / usec, usec)
      end

    -- SMPTE Offset: hr:mn:se frame.subframe
    elseif meta_type == 0x54 then
      if #meta_data >= 5 then
        local hr = meta_data:byte (1) & 0x1F
        return string.format ("%02d:%02d:%02d  frame %d+%d/100",
          hr, meta_data:byte (2), meta_data:byte (3),
          meta_data:byte (4), meta_data:byte (5))
      end

    -- Time Signature: nn/2^dd, cc MIDI clocks per metronome tick,
    -- bb notated 32nd-notes per MIDI quarter note (usually 8).
    elseif meta_type == 0x58 then
      if #meta_data >= 4 then
        return string.format ("%d/%d  (%d clocks/tick, %d 32nds/beat)",
          meta_data:byte (1), 2 ^ meta_data:byte (2),
          meta_data:byte (3), meta_data:byte (4))
      end

    -- Key Signature: sf = number of sharps (>0) or flats (<0), mi = mode.
    -- sf is stored as a signed byte: values 129–255 represent -127 to -1.
    elseif meta_type == 0x59 then
      if #meta_data >= 2 then
        local sf = meta_data:byte (1)
        if sf > 127 then sf = sf - 256 end  -- interpret as signed
        local mode = meta_data:byte (2)     -- 0 = major, 1 = minor
        local major_keys = {
          [-7]="Cb", [-6]="Gb", [-5]="Db", [-4]="Ab", [-3]="Eb",
          [-2]="Bb", [-1]="F",  [0]="C",   [1]="G",   [2]="D",
          [3]="A",   [4]="E",   [5]="B",   [6]="F#",  [7]="C#",
        }
        local minor_keys = {
          [-7]="Ab", [-6]="Eb", [-5]="Bb", [-4]="F",  [-3]="C",
          [-2]="G",  [-1]="D",  [0]="A",   [1]="E",   [2]="B",
          [3]="F#",  [4]="C#",  [5]="G#",  [6]="D#",  [7]="A#",
        }
        if mode == 0 then
          return (major_keys[sf] or "?") .. " major"
        else
          return (minor_keys[sf] or "?") .. " minor"
        end
      end

    -- Sequencer-Specific (0x7F): vendor-private data.
    -- Ardour/Evoral uses this to store internal note IDs: the payload
    -- starts with 0x99 0x01 followed by a VLQ-encoded note ID.
    -- These are written before every note in the SMF file for
    -- undo/redo tracking and note identity preservation.
    elseif meta_type == 0x7F then
      if #meta_data >= 3
          and meta_data:byte (1) == 0x99
          and meta_data:byte (2) == 0x01 then
        local id = 0
        for i = 3, #meta_data do
          local b = meta_data:byte (i)
          id = (id << 7) | (b & 0x7F)
          if (b & 0x80) == 0 then break end
        end
        return string.format ("Ardour/Evoral note ID %d", id)
      end
      -- Non-Ardour sequencer-specific: show raw hex
      local hex = {}
      for i = 1, math.min (#meta_data, 24) do
        hex[i] = string.format ("%02X", meta_data:byte (i))
      end
      if #meta_data > 24 then hex[#hex + 1] = "..." end
      return table.concat (hex, " ")
    end

    -- Fallback for any unrecognized type: hex dump of the payload
    local hex = {}
    for i = 1, math.min (#meta_data, 12) do
      hex[i] = string.format ("%02X", meta_data:byte (i))
    end
    if #meta_data > 12 then hex[#hex + 1] = "..." end
    return table.concat (hex, " ")
  end

  ---------------------------------------------------------------------------
  -- Decode a meta event from its raw SMF buffer (as returned by the model)
  ---------------------------------------------------------------------------
  -- In the Ardour model, meta events are stored as Evoral::Event objects
  -- whose buffer contains the full SMF encoding: 0xFF <type> <VLQ length>
  -- <data>.  This function extracts the type and payload from that buffer
  -- and delegates to decode_meta() for human-readable formatting.
  --
  -- The buffer is obtained via ARDOUR.LuaAPI.event_buffer(), which returns
  -- a Lua binary string (safe for embedded NUL bytes thanks to lua_pushlstring).
  ---------------------------------------------------------------------------
  local function decode_meta_from_buffer (buf)
    if not buf or #buf < 2 then
      return nil, nil, nil
    end
    -- buf[1] should be 0xFF (the meta event status byte)
    if buf:byte (1) ~= 0xFF then
      return nil, nil, nil
    end
    local meta_type = buf:byte (2)
    -- The payload starts after the VLQ-encoded length at byte 3+.
    -- We need to skip the length field to get to the actual data.
    local pos = 3
    local length = 0
    while pos <= #buf do
      local b = buf:byte (pos)
      length = (length << 7) | (b & 0x7F)
      pos = pos + 1
      if (b & 0x80) == 0 then break end
    end
    local meta_data = buf:sub (pos, pos + length - 1)
    local name = META_NAMES[meta_type] or string.format ("Meta 0x%02X", meta_type)
    local decoded = decode_meta (meta_type, meta_data)
    return name, decoded, meta_type
  end

  ---------------------------------------------------------------------------
  -- Raw SMF file parser
  ---------------------------------------------------------------------------
  -- This parser reads a MIDI file byte-by-byte, extracting ALL meta events
  -- from every track.  It handles:
  --   - Multi-track (Type 1) files
  --   - Running status (repeated status bytes omitted)
  --   - Variable-length quantities for delta times and lengths
  --   - Non-MTrk chunks (skipped gracefully)
  --
  -- It does NOT interpret the musical content (notes, CCs) — only meta
  -- events and structural framing.
  --
  -- Returns (events_list, ppqn) on success, or (nil, error_string) on failure.
  -- Each event in the list is a table: {track, ticks, name, data}.
  ---------------------------------------------------------------------------
  local function parse_midi_meta_events (path)
    local f = io.open (path, "rb")
    if not f then return nil, "Cannot open: " .. path end

    -- Safe seek wrapper: raises an error on I/O failure so pcall catches it.
    -- Using error() here lets the caller use pcall() to handle I/O errors
    -- without leaking the file handle (the pcall wrapper closes it).
    local function fseek (whence, offset)
      local pos, err = f:seek (whence, offset)
      if pos == nil then
        f:close ()
        error ("seek error: " .. (err or "?"), 2)
      end
      return pos
    end

    ---------- MThd (header chunk) ----------
    -- Every MIDI file starts with "MThd" followed by 6 bytes:
    --   format (2 bytes): 0=single track, 1=multi-track, 2=independent tracks
    --   ntrks  (2 bytes): number of track chunks
    --   division (2 bytes): timing resolution
    -- If bit 15 of division is 0, it's ticks-per-quarter-note (PPQN).
    -- If bit 15 is 1, it's SMPTE timing (not supported here).
    local header = f:read (14)
    if not header or #header < 14 or header:sub (1, 4) ~= "MThd" then
      f:close ()
      return nil, "Not a valid MIDI file"
    end

    local n_tracks = (header:byte (11) << 8) | header:byte (12)
    local division = (header:byte (13) << 8) | header:byte (14)
    if division & 0x8000 ~= 0 then
      f:close ()
      return nil, "SMPTE timing not supported"
    end
    local ppqn = division

    local all_events = {}

    ---------- Track chunks ----------
    -- Each track starts with "MTrk" + 4-byte big-endian length.
    -- Inside, events are stored as <delta-time> <event-data>.
    -- Delta times are VLQ-encoded ticks since the previous event.
    for track_num = 1, n_tracks do
      local chunk_hdr = f:read (8)
      if not chunk_hdr or #chunk_hdr < 8 then break end

      local chunk_type = chunk_hdr:sub (1, 4)
      local chunk_len  = (chunk_hdr:byte (5) << 24) | (chunk_hdr:byte (6) << 16)
                       | (chunk_hdr:byte (7) <<  8) |  chunk_hdr:byte (8)

      -- Skip non-MTrk chunks (the spec allows arbitrary chunk types)
      if chunk_type ~= "MTrk" then
        fseek ("cur", chunk_len)
        goto next_track
      end

      local track_end      = fseek () + chunk_len
      local abs_ticks      = 0
      local running_status = 0

      ---------- Event loop ----------
      -- Parse events until we reach the end of the track chunk.
      while fseek () < track_end do
        local pos_before = fseek ()

        -- Delta time: VLQ ticks since the previous event.
        local delta = read_vlq (f)
        if delta == nil then break end
        abs_ticks = abs_ticks + delta

        -- First byte: if >= 0x80, it's a new status byte.
        -- Otherwise, "running status" reuses the previous status.
        local b1 = f:read (1)
        if not b1 then break end
        b1 = b1:byte ()

        local status
        if b1 >= 0x80 then
          status = b1
          -- Running status applies to channel messages (0x80–0xEF) only.
          -- System messages (0xF0–0xFF) clear or don't set running status.
          if b1 < 0xF0 then
            running_status = b1
          else
            running_status = 0
          end
        else
          -- Running status: re-use previous channel status.
          -- We've already consumed b1, so seek back one byte.
          if running_status == 0 then goto next_event end
          status = running_status
          fseek ("cur", -1)
        end

        ---------- Meta event: 0xFF <type> <VLQ-length> <data> ----------
        if status == 0xFF then
          local mt = f:read (1)
          if not mt then break end
          local meta_type = mt:byte ()

          local meta_len = read_vlq (f)
          if meta_len == nil then break end

          local meta_data = meta_len > 0 and (f:read (meta_len) or "") or ""

          -- End of Track (0x2F) is mandatory; stop parsing this track.
          if meta_type == 0x2F then break end

          table.insert (all_events, {
            track = track_num,
            ticks = abs_ticks,
            name  = META_NAMES[meta_type] or string.format ("Meta 0x%02X", meta_type),
            data  = decode_meta (meta_type, meta_data),
          })

        ---------- SysEx: 0xF0 <VLQ-length> <data...F7> ----------
        -- or escape: 0xF7 <VLQ-length> <data> (for split sysex)
        elseif status == 0xF0 or status == 0xF7 then
          local slen = read_vlq (f)
          if slen then f:read (slen) end

        ---------- Channel messages: 0x80–0xEF ----------
        -- These carry 1 or 2 data bytes depending on the command nibble.
        -- We skip them entirely (this script only cares about meta events).
        elseif status >= 0x80 and status <= 0xEF then
          local cmd = status & 0xF0
          -- Program Change (0xC0) and Channel Pressure (0xD0) have 1 data byte;
          -- all others (Note On/Off, Poly Pressure, CC, Pitch Bend) have 2.
          if cmd == 0xC0 or cmd == 0xD0 then f:read (1) else f:read (2) end

        ---------- System Common messages ----------
        elseif status == 0xF2 then  -- Song Position Pointer: 2 bytes
          f:read (2)
        elseif status == 0xF3 then  -- Song Select: 1 byte
          f:read (1)
        -- 0xF1 (MTC Quarter Frame), 0xF4-0xF6 etc.: 0 data bytes
        end

        ::next_event::
        -- Safety net: if we haven't advanced in the file, break to avoid
        -- an infinite loop on malformed data.
        if fseek () <= pos_before then break end
      end

      -- Jump to the end of this track chunk (in case we broke out early
      -- or the track has trailing data after the End of Track event).
      fseek ("set", track_end)
      ::next_track::
    end

    f:close ()
    return all_events, ppqn
  end

  ---------------------------------------------------------------------------
  -- pcall wrapper for the parser
  ---------------------------------------------------------------------------
  -- The parser uses error() for I/O failures (via fseek).  Wrapping in
  -- pcall ensures the file handle is closed and errors surface as strings.
  ---------------------------------------------------------------------------
  local function safe_parse (path)
    local ok, a, b = pcall (parse_midi_meta_events, path)
    if not ok then return nil, "I/O error: " .. tostring (a) end
    return a, b
  end

  ---------------------------------------------------------------------------
  -- Session XML parser: map region names to SMF file paths
  ---------------------------------------------------------------------------
  -- Ardour stores MIDI data in "interchange/<session>/midifiles/" as SMF
  -- files.  The session XML file (.ardour) records the mapping:
  --   <Source id="N" name="filename.mid" type="midi" .../>
  --   <Region name="region-name" source-0="N" .../>
  --
  -- We need this mapping because the Lua API's to_filesource() on
  -- MidiSource has a LuaBridge shared_ptr type mismatch issue (the cast
  -- CastMemberPtr<Source,FileSource> fails for MidiSource userdata).
  -- Parsing the session XML is a reliable workaround.
  ---------------------------------------------------------------------------
  local function build_region_path_map ()
    local session_file = ARDOUR.LuaAPI.build_filename (
      Session:path (), Session:name () .. ".ardour")
    local f = io.open (session_file, "r")
    if not f then return {} end

    local mididir = ARDOUR.LuaAPI.build_filename (
      Session:path (), "interchange", Session:name (), "midifiles")

    -- First pass: collect source id -> filename
    local sources = {}
    for line in f:lines () do
      if line:find ("<Source") and line:find ('type="midi"') then
        local id   = line:match ('id="(%d+)"')
        local name = line:match ('%sname="([^"]+)"')
        if id and name then sources[id] = name end
      end
    end

    -- Second pass: map region name -> file path.
    -- Sources always appear before Regions in Ardour's XML, so a single
    -- pass would work, but two passes is more robust against reordering.
    f:seek ("set", 0)
    local region_paths = {}
    for line in f:lines () do
      if line:find ("<Region") then
        local rname  = line:match ('%sname="([^"]+)"')
        local src_id = line:match ('source%-0="(%d+)"')
        if rname and src_id then
          local src_name = sources[src_id]
          if src_name then
            -- An absolute path means an external file (e.g. linked, not copied).
            -- A relative name means it's in the interchange midifiles directory.
            if src_name:sub (1, 1) == "/" then
              region_paths[rname] = src_name
            else
              region_paths[rname] = mididir .. "/" .. src_name
            end
          end
        end
      end
    end

    f:close ()
    return region_paths
  end

  ---------------------------------------------------------------------------
  -- Formatting helpers
  ---------------------------------------------------------------------------

  -- Format an absolute tick count as "beat N + T/PPQN" for readability.
  local function fmt_beat (ticks, ppqn)
    return string.format ("beat %d + %d/%d",
      math.floor (ticks / ppqn), ticks % ppqn, ppqn)
  end

  -- Format the file-parsed meta-event section for one MIDI file.
  -- If header_label is provided, a separator banner and file path are
  -- printed first (used for standalone "External file" output).
  -- If omitted, only the "File :" line and meta events are shown
  -- (used when appending to an existing region section).
  local function format_file_section (path, header_label)
    local out = ""
    if header_label then
      out = out .. "======================================================\n"
               .. header_label .. "\n"
    end
    out = out .. "File   : " .. path .. "\n"
    local events, ppqn = safe_parse (path)
    if events == nil then
      return out .. "  Error: " .. (ppqn or "?") .. "\n\n"
    end
    out = out .. string.format ("PPQN   : %d\n", ppqn)
    out = out .. "\n-- Meta Events (from raw SMF file) --\n"
    if #events == 0 then
      out = out .. "  (none)\n"
    else
      for _, ev in ipairs (events) do
        out = out .. string.format ("  Trk %d  %-26s  %-22s  %s\n",
          ev.track, fmt_beat (ev.ticks, ppqn), ev.name, ev.data)
      end
    end
    return out .. "\n"
  end

  ---------------------------------------------------------------------------
  -- Format model-based meta events for one MIDI region
  ---------------------------------------------------------------------------
  -- This uses the Lua API functions:
  --   ARDOUR.LuaAPI.meta_event_list(model) → list of EventPtr
  --   ARDOUR.LuaAPI.event_buffer(event)    → raw buffer as Lua string
  --
  -- The model only stores text-type meta events (0x01–0x09).  Tempo,
  -- time signature, key signature are handled by Ardour's TempoMap and
  -- are NOT stored in the MidiModel.  Sequencer-specific events (0x7F)
  -- with Ardour note IDs are also excluded from the model.
  ---------------------------------------------------------------------------
  local function format_model_meta_events (mr)
    local mm = mr:midi_source (0):model ()
    local meta_list = ARDOUR.LuaAPI.meta_event_list (mm)
    local count = 0
    local lines = {}

    for ev in meta_list:iter () do
      local buf = ARDOUR.LuaAPI.event_buffer (ev)
      local name, decoded, meta_type = decode_meta_from_buffer (buf)
      if name then
        local t = ev:time ()
        table.insert (lines, string.format (
          "  beat %d + %d  %-22s  %s",
          t:get_beats (), t:get_ticks (), name, decoded or "(empty)"))
      end
      count = count + 1
    end

    local out = "\n-- Meta Events (from Ardour model) --\n"
    if count == 0 then
      out = out .. "  (none — text meta events 0x01-0x09 appear here after import)\n"
    else
      out = out .. table.concat (lines, "\n") .. "\n"
    end
    return out, count
  end

  -- =========================================================================
  -- Main script body
  -- =========================================================================

  ---------------------------------------------------------------------------
  -- Step 1: Optional external MIDI file
  ---------------------------------------------------------------------------
  -- The user can specify an external file (e.g. the original before Ardour
  -- import) to parse alongside the session's interchange copies.  This is
  -- useful for comparing what metadata was in the original vs. what Ardour
  -- kept.

  local rv = LuaDialog.Dialog ("MIDI Metadata", {
    { type = "label", title =
        "Optionally specify an external MIDI file\n"
     .. "to parse directly (e.g. the original before Ardour import).\n"
     .. "Leave empty to skip." },
    { type = "entry", key = "extra", title = "Extra MIDI file", default = "" },
  }):run ()
  if not rv then return end
  local extra_path = (rv["extra"] ~= "") and rv["extra"] or nil

  ---------------------------------------------------------------------------
  -- Step 2: Collect MIDI regions
  ---------------------------------------------------------------------------
  -- If the user has selected regions in the editor, use those.
  -- Otherwise, iterate all tracks in the session and collect every
  -- MIDI region.  This makes the script work both as a targeted
  -- inspection tool and as a whole-session survey.

  local midi_regions = {}
  local sel = Editor:get_selection ()
  for r in sel.regions:regionlist ():iter () do
    if not r:to_midiregion ():isnil () then
      table.insert (midi_regions, r)
    end
  end
  if #midi_regions == 0 then
    for track in Session:get_tracks ():iter () do
      local playlist = track:to_track ():playlist ()
      if playlist then
        for r in playlist:region_list ():iter () do
          if not r:to_midiregion ():isnil () then
            table.insert (midi_regions, r)
          end
        end
      end
    end
  end

  if #midi_regions == 0 then
    LuaDialog.Message ("MIDI Metadata", "No MIDI regions found in this session.",
      LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run ()
    return
  end

  -- Build region name -> file path map from session XML.
  local region_paths = build_region_path_map ()

  ---------------------------------------------------------------------------
  -- Step 3: Build output
  ---------------------------------------------------------------------------
  -- For each region we output:
  --   - Header with region name and file path
  --   - Note count and patch change count (from the model)
  --   - Patch change details (if any)
  --   - Meta events from the Ardour model (text types 0x01–0x09)
  --   - Meta events from the raw SMF file (all types)
  --
  -- This dual view lets the user see what's in the model (what Ardour
  -- uses at runtime) versus what's in the file (the complete picture).

  local output = ""

  -- Extra file first (if supplied).
  if extra_path then
    output = output .. format_file_section (extra_path, "External file")
  end

  -- Session regions.
  for _, r in ipairs (midi_regions) do
    local mr   = r:to_midiregion ()
    local path = region_paths[r:name ()]

    local section = "======================================================\n"
                 .. "Region : " .. r:name () .. "\n"

    -- Model-based info is always available (it lives in memory, not on
    -- disk), so we show it regardless of whether the file path is known.
    -- This ensures the script is useful immediately after import, before
    -- the session has been saved.
    local mm         = mr:midi_source (0):model ()
    local note_count = 0
    for _ in ARDOUR.LuaAPI.note_list (mm):iter () do
      note_count = note_count + 1
    end
    local patch_list = ARDOUR.LuaAPI.patch_change_list (mm)
    local pc_count   = 0
    for _ in patch_list:iter () do pc_count = pc_count + 1 end

    section = section .. string.format (
      "Notes  : %d  |  Patch changes: %d\n", note_count, pc_count)

    -- Patch change details (from the model).
    if pc_count > 0 then
      section = section .. "\n-- Patch Changes (from Ardour model) --\n"
      for pc in patch_list:iter () do
        local t = pc:time ()
        section = section .. string.format (
          "  beat %d + %d  ->  Bank %d, Program %d\n",
          t:get_beats (), t:get_ticks (), pc:bank (), pc:program ())
      end
    end

    -- Model-based meta events (text types 0x01–0x09 that survived import).
    local model_meta_text = format_model_meta_events (mr)
    section = section .. model_meta_text

    -- Raw SMF file section: only available when we can resolve the file
    -- path from the session XML.  Before the first save, the session XML
    -- on disk is stale and won't contain newly imported regions.
    section = section
      .. "\n(Note: SMF file content reflects the last save;"
      .. " save the session to update.)\n"
    if path then
      section = section .. format_file_section (path)
    else
      section = section
        .. "\n-- Meta Events (from raw SMF file) --\n"
        .. "  (file path not yet in session XML — save session first)\n"
    end

    output = output .. section .. "\n"
  end

  ---------------------------------------------------------------------------
  -- Step 4: Display output
  ---------------------------------------------------------------------------
  -- Always send the full output to Window > Log (via print()).
  -- Also show a dialog with a truncated version if the output is very long,
  -- since the dialog widget has limited space.

  print (output)

  local MAX_DIALOG = 4000
  local dialog_text = output
  if #output > MAX_DIALOG then
    dialog_text = output:sub (1, MAX_DIALOG - 80)
      .. "\n...(truncated - see Window > Log for the full listing)"
  end

  LuaDialog.Message ("MIDI Metadata", dialog_text,
    LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run ()

end end
