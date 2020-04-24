ardour {
    ["type"]    = "dsp",
    name        = "MIDI Note Mapper",
    category    = "Utility",
    license     = "MIT",
    author      = "Alby Musaelian",
    description = [[Map arbitrary MIDI notes to others. Affects Note On/Off and polyphonic key pressure. Note that if a single note is mapped multiple times, the last mapping wins (MIDI events are never duplicated).]]
}

-- The number of remapping pairs to allow. Increasing this (at least in theory)
-- decreases performace, so it's set fairly low as a default. The user can
-- increase this if they have a need to.
N_REMAPINGS = 10

OFF_NOTE = -1

function dsp_ioconfig ()
    return { { midi_in = 1, midi_out = 1, audio_in = 0, audio_out = 0}, }
end


function dsp_params ()

    local map_scalepoints = {}
    map_scalepoints["None"] = OFF_NOTE
    for note=0,127 do
        local name = ARDOUR.ParameterDescriptor.midi_note_name(note)
        map_scalepoints[string.format("%03d (%s)", note, name)] = note
    end

    local map_params = {}

    i = 1
    for mapnum = 1,N_REMAPINGS do
        -- From and to
        for _,name in pairs({"| #" .. mapnum .. "  Map note", "|__   to"}) do
            map_params[i] = {
                ["type"] = "input",
                name = name,
                min = -1,
                max = 127,
                default = OFF_NOTE,
                integer = true,
                enum = true,
                scalepoints = map_scalepoints
            }
            i = i + 1
        end
    end

    return map_params
end

function dsp_run (_, _, n_samples)
    assert (type(midiin) == "table")
    assert (type(midiout) == "table")
    local cnt = 1;

    function tx_midi (time, data)
        midiout[cnt] = {}
        midiout[cnt]["time"] = time;
        midiout[cnt]["data"] = data;
        cnt = cnt + 1;
    end

    -- We build the translation table every buffer because, as far as I can tell,
    -- there's no way to only rebuild it when the parameters have changed.
    -- As a result, it has to be updated every buffer for the parameters to have
    -- any effect.

    -- Restore translation table
    local translation_table = {}
    local ctrl = CtrlPorts:array()
    for i=1,N_REMAPINGS*2,2 do
        if not (ctrl[i] == OFF_NOTE) then
            translation_table[ctrl[i]] = ctrl[i + 1]
        end
    end

    -- for each incoming midi event
    for _,b in pairs (midiin) do
        local t = b["time"] -- t = [ 1 .. n_samples ]
        local d = b["data"] -- get midi-event
        local event_type
        if #d == 0 then event_type = -1 else event_type = d[1] >> 4 end

        if (#d == 3) and (event_type == 9 or event_type == 8 or event_type == 10) then -- note on, note off, poly. afterpressure
            -- Do the mapping - 2 is note byte for these types
            d[2] = translation_table[d[2]] or d[2]
            if not (d[2] == OFF_NOTE) then
                tx_midi (t, d)
            end
        else
            tx_midi (t, d)
        end
    end
end
