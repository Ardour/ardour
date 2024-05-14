ardour {
    ["type"]    = "dsp",
    name        = "DM10-mkII-Studio HiHat",
    category    = "Utility",
    license     = "MIT",
    author      = "Ardour Community",
    description = [[Map HiHat MIDI events depending on pedal CC. Specifically MIDI Note Number 8 is translated to 54,47,58 deending on CC-8.]]
}

function dsp_ioconfig ()
    return { { midi_in = 1, midi_out = 1, audio_in = 0, audio_out = 0}, }
end

local hihat_note = -1
local hihat_state = 0

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

	-- for each incoming midi event
	for _,b in pairs (midiin) do
		local t = b["time"] -- t = [ 1 .. n_samples ]
		local d = b["data"] -- midi-event data
		local event_type
		if #d == 0 then event_type = -1 else event_type = d[1] >> 4 end

		-- intercept CC message
		if #d == 3 and event_type == 11 and d[2] == 4 then
			hihat_state = d[3]
		end

		-- map Note event
		if (#d == 3) and d[2] == 8 and event_type == 9 then
			if     hihat_state < 42 then hihat_note = 54 -- Hihat_Closed
			elseif hihat_state < 92 then hihat_note = 46 -- Hihat_Semi_Open
			else                         hihat_note = 58 -- Hihat_Open
			end
			d[2] = hihat_note
		end

		-- translate aftertouch
		if (#d == 3) and d[2] == 8 and event_type == 10 then
			if (hihat_note > 0) then
				d[2] = hihat_note
			end
		end

		-- intercept note off
		if (#d == 3) and d[2] == 8 and event_type == 8 then
			if (hihat_note > 0) then
				d[2] = hihat_note
			end
			--hihat_note = -1
		end

		tx_midi (t, d)
	end
end
