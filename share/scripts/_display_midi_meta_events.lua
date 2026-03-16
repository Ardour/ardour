ardour {
  ["type"] = "EditorAction",
  name = "Display MIDI Meta Events",
  author = "Ardour Team",
  description = [[Displays all MIDI metadata (meta events, patch changes, note count)
for selected MIDI regions. Meta events include text, tempo, time/key signatures, etc.
Full output is always sent to Window > Log; a summary dialog is also shown.
If no regions are selected, all MIDI regions in the session are processed.]]
}

function factory () return function ()

  -- Read a Variable Length Quantity (VLQ) from an open file handle.
  -- Returns the decoded integer, or nil on truncated / malformed data.
  local function read_vlq (f)
    local val = 0
    for _ = 1, 4 do  -- VLQ is at most 4 bytes in valid MIDI
      local b = f:read (1)
      if not b then return nil end
      b = b:byte ()
      val = (val << 7) | (b & 0x7F)
      if (b & 0x80) == 0 then return val end
    end
    return nil  -- malformed VLQ (more than 4 bytes with MSB set)
  end

  -- Human-readable names for all standard MIDI meta event types.
  local META_NAMES = {
    [0x00] = "Sequence Number",
    [0x01] = "Text",
    [0x02] = "Copyright",
    [0x03] = "Track Name",
    [0x04] = "Instrument Name",
    [0x05] = "Lyric",
    [0x06] = "Marker",
    [0x07] = "Cue Point",
    [0x08] = "Program Name",
    [0x09] = "Device Name",
    [0x20] = "Channel Prefix",
    [0x21] = "MIDI Port",
    [0x2F] = "End of Track",
    [0x51] = "Tempo",
    [0x54] = "SMPTE Offset",
    [0x58] = "Time Signature",
    [0x59] = "Key Signature",
    [0x7F] = "Sequencer-Specific",
  }

  -- Decode meta event payload into a human-readable string.
  local function decode_meta (meta_type, meta_data)
    if meta_type >= 0x01 and meta_type <= 0x09 then
      return meta_data  -- text events: return as-is

    elseif meta_type == 0x00 then
      if #meta_data >= 2 then
        return string.format ("%d", (meta_data:byte (1) << 8) | meta_data:byte (2))
      end

    elseif meta_type == 0x20 then
      if #meta_data >= 1 then
        return string.format ("channel %d", meta_data:byte (1))
      end

    elseif meta_type == 0x21 then
      if #meta_data >= 1 then
        return string.format ("port %d", meta_data:byte (1))
      end

    elseif meta_type == 0x51 then
      if #meta_data >= 3 then
        local usec = (meta_data:byte (1) << 16) | (meta_data:byte (2) << 8) | meta_data:byte (3)
        return string.format ("%.3f BPM  (%d us/beat)", 60000000.0 / usec, usec)
      end

    elseif meta_type == 0x54 then
      if #meta_data >= 5 then
        local hr = meta_data:byte (1) & 0x1F
        return string.format ("%02d:%02d:%02d  frame %d+%d/100",
          hr, meta_data:byte (2), meta_data:byte (3),
          meta_data:byte (4), meta_data:byte (5))
      end

    elseif meta_type == 0x58 then
      if #meta_data >= 4 then
        return string.format ("%d/%d  (%d clocks/tick, %d 32nds/beat)",
          meta_data:byte (1), 2 ^ meta_data:byte (2),
          meta_data:byte (3), meta_data:byte (4))
      end

    elseif meta_type == 0x59 then
      if #meta_data >= 2 then
        local sf = meta_data:byte (1)
        if sf > 127 then sf = sf - 256 end
        local mode = meta_data:byte (2)
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

    elseif meta_type == 0x7F then
      -- Detect Ardour/Evoral internal note-ID events (0x99 0x01 <VLQ id>).
      -- These are written before every note for undo/redo tracking.
      if #meta_data >= 3
          and meta_data:byte (1) == 0x99
          and meta_data:byte (2) == 0x01 then
        -- Decode the VLQ note ID starting at byte 3
        local id = 0
        for i = 3, #meta_data do
          local b = meta_data:byte (i)
          id = (id << 7) | (b & 0x7F)
          if (b & 0x80) == 0 then break end
        end
        return string.format ("Ardour/Evoral note ID %d", id)
      end
      local hex = {}
      for i = 1, math.min (#meta_data, 24) do
        hex[i] = string.format ("%02X", meta_data:byte (i))
      end
      if #meta_data > 24 then hex[#hex + 1] = "..." end
      return table.concat (hex, " ")
    end

    -- fallback: hex dump
    local hex = {}
    for i = 1, math.min (#meta_data, 12) do
      hex[i] = string.format ("%02X", meta_data:byte (i))
    end
    if #meta_data > 12 then hex[#hex + 1] = "..." end
    return table.concat (hex, " ")
  end

  -- Parse all meta events from a MIDI file.
  -- Returns (events_list, ppqn) on success, or (nil, error_string) on failure.
  local function parse_midi_meta_events (path)
    local f = io.open (path, "rb")
    if not f then return nil, "Cannot open: " .. path end

    -- Safe seek wrapper: raises an error on I/O failure so pcall catches it.
    local function fseek (whence, offset)
      local pos, err = f:seek (whence, offset)
      if pos == nil then
        f:close ()
        error ("seek error: " .. (err or "?"), 2)
      end
      return pos
    end

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

    for track_num = 1, n_tracks do
      local chunk_hdr = f:read (8)
      if not chunk_hdr or #chunk_hdr < 8 then break end

      local chunk_type = chunk_hdr:sub (1, 4)
      local chunk_len  = (chunk_hdr:byte (5) << 24) | (chunk_hdr:byte (6) << 16)
                       | (chunk_hdr:byte (7) <<  8) |  chunk_hdr:byte (8)

      if chunk_type ~= "MTrk" then
        fseek ("cur", chunk_len)
        goto next_track
      end

      local track_end      = fseek () + chunk_len
      local abs_ticks      = 0
      local running_status = 0

      while fseek () < track_end do
        local pos_before = fseek ()

        local delta = read_vlq (f)
        if delta == nil then break end
        abs_ticks = abs_ticks + delta

        local b1 = f:read (1)
        if not b1 then break end
        b1 = b1:byte ()

        local status
        if b1 >= 0x80 then
          status = b1
          if b1 < 0xF0 then
            running_status = b1
          else
            running_status = 0
          end
        else
          if running_status == 0 then goto next_event end
          status = running_status
          fseek ("cur", -1)
        end

        if status == 0xFF then
          local mt = f:read (1)
          if not mt then break end
          local meta_type = mt:byte ()

          local meta_len = read_vlq (f)
          if meta_len == nil then break end

          local meta_data = meta_len > 0 and (f:read (meta_len) or "") or ""

          if meta_type == 0x2F then break end  -- End of Track

          table.insert (all_events, {
            track = track_num,
            ticks = abs_ticks,
            name  = META_NAMES[meta_type] or string.format ("Meta 0x%02X", meta_type),
            data  = decode_meta (meta_type, meta_data),
          })

        elseif status == 0xF0 or status == 0xF7 then
          local slen = read_vlq (f)
          if slen then f:read (slen) end

        elseif status >= 0x80 and status <= 0xEF then
          local cmd = status & 0xF0
          if cmd == 0xC0 or cmd == 0xD0 then f:read (1) else f:read (2) end

        elseif status == 0xF2 then
          f:read (2)
        elseif status == 0xF3 then
          f:read (1)
        end

        ::next_event::
        -- Every path here has consumed >= 2 bytes; guard is a last-resort fence.
        if fseek () <= pos_before then break end
      end

      fseek ("set", track_end)
      ::next_track::
    end

    f:close ()
    return all_events, ppqn
  end

  -- Wrap parser so fseek() errors surface cleanly without leaking the handle.
  local function safe_parse (path)
    local ok, a, b = pcall (parse_midi_meta_events, path)
    if not ok then return nil, "I/O error: " .. tostring (a) end
    return a, b
  end

  -- Build a map  region_name -> full MIDI file path  by reading the session
  -- XML file (.ardour).  The XML records <Source id=N name=filename type=midi>
  -- and <Region name=X source-0=N>, so we join them to get the path.
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

    -- Second pass: map region name -> file path
    -- (Sources always appear before Regions in Ardour's XML, so one pass
    --  would suffice, but two passes is more robust.)
    f:seek ("set", 0)
    local region_paths = {}
    for line in f:lines () do
      if line:find ("<Region") then
        local rname  = line:match ('%sname="([^"]+)"')
        local src_id = line:match ('source%-0="(%d+)"')
        if rname and src_id then
          local src_name = sources[src_id]
          if src_name then
            -- Absolute name means an external file; relative means interchange.
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

  local function fmt_beat (ticks, ppqn)
    return string.format ("beat %d + %d/%d",
      math.floor (ticks / ppqn), ticks % ppqn, ppqn)
  end

  -- Format the meta-event section of one parsed MIDI file into a string.
  local function format_file_section (label, path)
    local out = "======================================================\n"
             .. label .. "\n"
             .. "File   : " .. path .. "\n"
    local events, ppqn = safe_parse (path)
    if events == nil then
      return out .. "  Error: " .. (ppqn or "?") .. "\n\n"
    end
    out = out .. string.format ("PPQN   : %d\n", ppqn)
    out = out .. "\n-- Meta Events --\n"
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

  -- -------------------------------------------------------------------------
  -- Optional: ask the user for an external MIDI file to parse directly.

  local rv = LuaDialog.Dialog ("MIDI Metadata", {
    { type = "label", title = "Optionally specify an external MIDI file\nto parse directly (e.g. the original before Ardour import).\nLeave empty to skip." },
    { type = "entry", key = "extra", title = "Extra MIDI file", default = "" },
  }):run ()
  if not rv then return end
  local extra_path = (rv["extra"] ~= "") and rv["extra"] or nil

  -- -------------------------------------------------------------------------
  -- Collect MIDI regions: selected ones if any, otherwise all in the session.

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

  -- -------------------------------------------------------------------------
  -- Build output.

  local output = ""

  -- Extra file first (if supplied).
  if extra_path then
    output = output .. format_file_section ("External file", extra_path)
  end

  -- Session regions.
  for _, r in ipairs (midi_regions) do
    local mr   = r:to_midiregion ()
    local path = region_paths[r:name ()]

    if not path then
      output = output
        .. "======================================================\n"
        .. "Region : " .. r:name () .. "\n"
        .. "  (source file not found in session XML)\n\n"
      goto continue
    end

    local section = format_file_section ("Region : " .. r:name (), path)

    -- Augment with model-based info (note count, patch changes).
    local mm         = mr:midi_source (0):model ()
    local note_count = 0
    for _ in ARDOUR.LuaAPI.note_list (mm):iter () do
      note_count = note_count + 1
    end
    local patch_list = ARDOUR.LuaAPI.patch_change_list (mm)
    local pc_count   = 0
    for _ in patch_list:iter () do pc_count = pc_count + 1 end

    local model_line = string.format (
      "Notes  : %d  |  Patch changes: %d\n", note_count, pc_count)

    -- Insert model line after the "PPQN" line.
    section = section:gsub ("(PPQN.-\n)", "%1" .. model_line, 1)

    if pc_count > 0 then
      local pc_text = "\n-- Patch Changes --\n"
      for pc in patch_list:iter () do
        local t = pc:time ()
        pc_text = pc_text .. string.format ("  beat %d + %d  ->  Bank %d, Program %d\n",
          t:get_beats (), t:get_ticks (), pc:bank (), pc:program ())
      end
      -- Insert patch changes before the meta events block.
      section = section:gsub ("(\n%-%- Meta Events)", pc_text .. "%1", 1)
    end

    output = output .. section

    ::continue::
  end

  -- -------------------------------------------------------------------------
  -- Output: always to log, dialog if short enough.

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
