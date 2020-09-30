ardour {
    ["type"]    = "dsp",
    name        = "AVL Drumkit MIDI Map",
    category    = "Utility",
    license     = "MIT",
    author      = "Ardour Community", -- based on MIDI Note Mapper by Alby Musaelian
    description = [[Map MIDI notes to drum-kit pcs of 'Black Pearl' or 'Red Zeppelin' AVL drumkit plugins. Unmapped notes are ignored. In case of duplicate assignments the later one is preferred.]]
}

OFF_NOTE = -1

function dsp_ioconfig ()
    return { { midi_in = 1, midi_out = 1, audio_in = 0, audio_out = 0}, }
end

function dsp_params ()
	local avldrums = {
		[36] = "Kick Drum",
		[37] = "Snare Side Stick",
		[38] = "Snare Center",
		[39] = "Hand Clap",
		[40] = "Snare Edge",
		[41] = "Floor Tom Center",
		[42] = "Closed HiHat",
		[43] = "Floor Tom Edge",
		[44] = "Pedal HiHat",
		[45] = "Tom Center",
		[46] = "Semi-Open HiHat",
		[47] = "Tom Edge",
		[48] = "Swish HiHat",
		[49] = "Crash Cymbal 1 (left)",
		[50] = "Crash Cymbal 1 Choked",
		[51] = "Ride Cymbal Tip",
		[52] = "Ride Cymbal Choked",
		[53] = "Ride Cymbal Bell",
		[54] = "Tambourine",
		[55] = "Splash Cymbal",
		[56] = "Cowbell",
		[57] = "Crash Cymbal 2 (right)",
		[58] = "Crash Cymbal 2 Choked",
		[59] = "Ride Cymbal Shank",
		[60] = "Crash Cymbal 3 (large Paiste)",
		[61] = "Maracas"
	}

	local map_scalepoints = {}
	map_scalepoints["None"] = OFF_NOTE

	for note=0,127 do
		local name = ARDOUR.ParameterDescriptor.midi_note_name(note)
		map_scalepoints[string.format("%03d (%s)", note, name)] = note
	end

	local map_params = {}

	local i = 1
	for note, name in pairs (avldrums) do
		map_params[i] = {
			["type"] = "input",
			name = name,
			min = -1,
			max = 127,
			default = note,
			integer = true,
			enum = true,
			scalepoints = map_scalepoints
		}
		i = i + 1
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

	-- build translation table
	local translation_table = {}
	local ctrl = CtrlPorts:array()
	for i = 1, 26 do
		if not (ctrl[i] == OFF_NOTE) then
			translation_table[ctrl[i]] = 35 + i
		end
	end

	-- for each incoming midi event
	for _,b in pairs (midiin) do
		local t = b["time"] -- t = [ 1 .. n_samples ]
		local d = b["data"] -- midi-event data
		local event_type
		if #d == 0 then event_type = -1 else event_type = d[1] >> 4 end

		if (#d == 3) and (event_type == 9 or event_type == 8 or event_type == 10) then
			-- Do the mapping - 2 is note byte for these types
			d[2] = translation_table[d[2]] or OFF_NOTE
			if not (d[2] == OFF_NOTE) then
				tx_midi (t, d)
			end
		else
			tx_midi (t, d)
		end
	end
end
