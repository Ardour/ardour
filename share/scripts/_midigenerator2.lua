ardour {
	["type"]    = "dsp",
	name        = "MIDI Generator II",
	category    = "Example",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[An Example Midi Generator for prototyping.]]
}

function dsp_ioconfig ()
	return { { midi_in = 1, midi_out = 1, audio_in = -1, audio_out = -1}, }
end

function dsp_configure (ins, outs)
	n_out = outs
	n_out:set_midi (0)
end

function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	local ob = out_map:get (ARDOUR.DataType ("midi"), 0)
	if ob ~= ARDOUR.ChanMapping.Invalid then
		local mb = bufs:get_midi (ob)

		-- see _midigenerator.lua for
		-- how to use a timed sequence

		local ba = C.ByteVector () -- construct a byte vector
		ba:add ({0x90, 64, 127}) -- add some data to the vector
		-- send a message at cycle-start
		mb:push_back (offset, Evoral.EventType.MIDI_EVENT, ba:size (), ba:to_array());

		ba:clear ()
		ba:add ({0x80, 64, 127})
		mb:push_back (n_samples - 1 - offset, Evoral.EventType.MIDI_EVENT, ba:size (), ba:to_array(), 0);
	end
	ARDOUR.DSP.process_map (bufs, n_out, in_map, out_map, n_samples, offset)
end
