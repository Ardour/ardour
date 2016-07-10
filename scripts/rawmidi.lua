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

-- "dsp_runmap" uses Ardour's internal processor API, eqivalent to
-- 'connect_and_run()". There is no overhead (mapping, translating buffers).
-- The lua implementation is responsible to map all the buffers directly.
function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	-- see http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:ChanMapping
	local ib = in_map:get (ARDOUR.DataType ("midi"), 0); -- get index of the 1st mapped midi input buffer
	local ob = in_map:get (ARDOUR.DataType ("midi"), 0); -- get index of the 1st mapped midi output buffer
	assert (ib ~= ARDOUR.ChanMapping.Invalid);
	assert (ib == ob);  -- require inplace, buffers are identical

	-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:MidiBuffer
	local mb = bufs:get_midi (ib) -- get the mapped buffer
	events = mb:table () -- copy event list into a lua table

	-- iterate over all midi events
	for _,e in pairs (events) do
		-- e is-a http://manual.ardour.org/lua-scripting/class_reference/#Evoral:MidiEvent

		--print (e:channel())
	end
end
