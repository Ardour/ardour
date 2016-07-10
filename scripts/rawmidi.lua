ardour {
	["type"]    = "dsp",
	name        = "Midi Passthru",
	category    = "Example",
	license     = "MIT",
	author      = "Robin Gareus",
	email       = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[An Example Midi Passthrough Plugin using raw buffers.]]
}

function dsp_ioconfig ()
	return { { audio_in = 0, audio_out = 0}, }
end

function dsp_has_midi_input () return true end
function dsp_has_midi_output () return true end

function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	local ib = in_map:get(ARDOUR.DataType("midi"), 0); -- get id of input buffer
	local ob = in_map:get(ARDOUR.DataType("midi"), 0); -- get id of output buffer
	assert (ib ~= ARDOUR.ChanMapping.Invalid);
	assert (ib == ob);  -- inplace, buffers are identical

	local mb = bufs:get_midi (ib)
	events = mb:table() -- copy event list into lua table

	for _,e in pairs (events) do
		-- e is an http://ardourman/lua-scripting/class_reference/#Evoral:MidiEvent
		--
		--print (e:channel())
	end
end
