ardour {
	["type"]    = "dsp",
	name        = "Midi Passthru",
	category    = "Example",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[An Example Audio/MIDI Passthrough Plugin using Buffer Pointers]]
}

-- return possible audio i/o configurations
function dsp_ioconfig ()
	-- -1, -1 = any number of channels as long as input and output count matches
	-- require 1 MIDI in, 1 MIDI out.
	return { { midi_in = 1, midi_out = 1, audio_in = -1, audio_out = -1}, }
end

function dsp_configure (ins, outs)
	n_out = outs
end

-- "dsp_runmap" uses Ardour's internal processor API, eqivalent to
-- 'connect_and_run()". There is no overhead (mapping, translating buffers).
-- The lua implementation is responsible to map all the buffers directly.
function dsp_runmap (bufs, in_map, out_map, n_samples, offset)

	-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:ChanMapping

	local ib = in_map:get (ARDOUR.DataType ("midi"), 0) -- get index of the 1st mapped midi input buffer

	if ib ~= ARDOUR.ChanMapping.Invalid then
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:MidiBuffer
		local mb = bufs:get_midi (ib) -- get the mapped buffer
		local events = mb:table () -- copy event list into a lua table

		-- iterate over all MIDI events
		for _, e in pairs (events) do
			-- e is-a http://manual.ardour.org/lua-scripting/class_reference/#Evoral:MidiEvent

			-- do something with the event e.g.
			print (e:channel (), e:time (), e:size (), e:buffer ():array ()[1], e:buffer ():get_table (e:size ())[1])
		end
	end

	----
	-- The following code is needed with "dsp_runmap" to work for arbitrary pin connections
	-- this passes though all audio/midi data unprocessed.

	ARDOUR.DSP.process_map (bufs, n_out, in_map, out_map, n_samples, offset)

	-- equivalent lua code.
	-- NOTE: the lua implementation below is intended for io-config [-1,-1].
	-- It only works for actually mapped channels due to in_map:count() out_map:count()
	-- being identical to the i/o pin count in this case.
	--
	-- Plugins that have multiple possible configurations will need to implement
	-- dsp_configure() and remember the actual channel count.
	--
	-- ARDOUR.DSP.process_map() does iterate over the mapping itself and works generally.
	-- Still the lua code below does lend itself as elaborate example.
	--
	--[[

	local audio_ins = in_map:count (): n_audio () -- number of mapped audio input buffers
	local audio_outs = out_map:count (): n_audio () -- number of mapped audio output buffers
	assert (audio_outs, audio_ins) -- ioconfig [-1, -1]: must match

	-- copy audio data if any
	for c = 1, audio_ins do
		local ib = in_map:get (ARDOUR.DataType ("audio"), c - 1) -- get index of mapped input buffer
		local ob = out_map:get (ARDOUR.DataType ("audio"), c - 1) -- get index of mapped output buffer
		if ib ~= ARDOUR.ChanMapping.Invalid and ob ~= ARDOUR.ChanMapping.Invalid and ib ~= ob then
			-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:DSP
			-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:AudioBuffer
			ARDOUR.DSP.copy_vector (bufs:get_audio (ob):data (offset), bufs:get_audio (ib):data (offset), n_samples)
		end
	end
	-- Clear unconnected output buffers.
	-- In case we're processing in-place some buffers may be identical,
	-- so this must be done *after* copying relvant data from that port.
	for c = 1, audio_outs do
		local ib = in_map:get (ARDOUR.DataType ("audio"), c - 1)
		local ob = out_map:get (ARDOUR.DataType ("audio"), c - 1)
		if ib == ARDOUR.ChanMapping.Invalid and ob ~= ARDOUR.ChanMapping.Invalid then
			bufs:get_audio (ob):silence (n_samples, offset)
		end
	end

	-- copy midi data
	local midi_ins = in_map:count (): n_midi () -- number of midi input buffers
	local midi_outs = out_map:count (): n_midi () -- number of midi input buffers

	-- with midi_in=1, midi_out=1 in dsp_ioconfig
	-- the following will always be true
	assert (midi_ins == 1)
	assert (midi_outs == 1)

	for c = 1, midi_ins do
		local ib = in_map:get (ARDOUR.DataType ("midi"), c - 1)
		local ob = out_map:get (ARDOUR.DataType ("midi"), c - 1)
		if ib ~= ARDOUR.ChanMapping.Invalid and ob ~= ARDOUR.ChanMapping.Invalid and ib ~= ob then
			bufs:get_midi (ob):copy (bufs:get_midi (ib))
		end
	end
	-- silence unused midi outputs
	for c = 1, midi_outs do
		local ib = in_map:get (ARDOUR.DataType ("midi"), c - 1)
		local ob = out_map:get (ARDOUR.DataType ("midi"), c - 1)
		if ib == ARDOUR.ChanMapping.Invalid and ob ~= ARDOUR.ChanMapping.Invalid then
			bufs:get_midi (ob):silence (n_samples, offset)
		end
	end
	--]]
end
