ardour {
	["type"]    = "EditorAction",
	name        = "MIDI Tuning",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[generate MIDI tuning standard (MTS) messages for 12TET pitch changes]]
}

function factory () return function ()

	-- return table of all MIDI tracks, and all instrument plugins.
	--
	-- MidiTrack::write_immediate_event() injects MIDI events to the track's input
	-- PluginInsert::write_immediate_event() sends events directly to a plugin
	function midi_targets ()
		local rv = {}
		for r in Session:get_tracks():iter() do

			if not r:to_track():isnil() then
				local mtr = r:to_track():to_midi_track()
				if not mtr:isnil() then
					rv["Track: '" .. r:name() .. "'"] = mtr
				end
			end

			local i = 0;
			while true do
				local proc = r:nth_plugin (i)
				if proc:isnil () then break end
				local pi = proc:to_plugininsert ()
				if pi:is_instrument () then
					rv["Track: '" .. r:name() .. "' | Plugin: '" .. pi:name() .. "'"] = pi
				end
				i = i + 1
			end
		end
		return rv
	end

	function log2 (v)
		return math.log (v) / math.log (2)
	end

	-- calculate MIDI note-number and cent-offset for a given frequency
	--
	-- "The first byte of the frequency data word specifies the nearest equal-tempered
	--  semitone below the frequency. The next two bytes (14 bits) specify the fraction
	--  of 100 cents above the semitone at which the frequency lies."
	--
	-- 68 7F 7F = 439.9984 Hz
	-- 69 00 00 = 440.0000 Hz
	-- 69 00 01 = 440.0016 Hz
	--
	-- NB. 7F 7F 7F = no change (reserved)
	--
	function freq_to_mts (hz)
		local note = math.floor (12. * log2 (hz / 440) + 69.0)
		local freq = 440.0 * 2.0 ^ ((note - 69) / 12);
		local cent = 1200.0 * log2 (hz / freq)
		-- fixup rounding errors
		if cent >= 99.99 then
			note = note + 1
			cent = 0
		end
		if cent < 0 then
			cent = 0
		end
		return note, cent
	end

	local dialog_options = {
		{ type = "number", key = "tuning", title = "Tuning A4 (Hz)", min= 220.0, max = 880.0, default = 440.0, digits = 1, step = 0.1 },
		{ type = "dropdown", key = "tx", title = "MIDI SysEx Target", values = midi_targets () }
	}

	local rv = LuaDialog.Dialog ("MIDI Tuning", dialog_options):run ()
	dialog_options = nil -- drop references (track, plugins, shared ptr)
	collectgarbage () -- and release the references immediately

	if not rv then return end -- user cancelled

	-- prepare sending data
	local tx = rv["tx"] -- output port
	local ref_freq = rv["tuning"] -- ref freq (440.0)
	local parser = ARDOUR.RawMidiParser () -- construct a MIDI parser
	local checksum = 0

	-- helper function to send MIDI
	function tx_midi (syx, len, hdr)
		for b = 1, len do
			--dump:write (string.char(syx:byte (b)))

			-- calculate checksum, xor of all payload data
			-- (excluding the 0xf0, 0xf7, and the checksum field)
			if b >= hdr then
				checksum = checksum ~ syx:byte (b)
			end

			-- parse message to C/C++ uint8_t* array (Validate message correctness. This
			-- also returns C/C++ uint8_t* array for direct use with write_immediate_event.)
			if parser:process_byte (syx:byte (b)) then
				tx:write_immediate_event (Evoral.EventType.MIDI_EVENT, parser:buffer_size (), parser:midi_buffer ())
				-- Slow things down a bit to ensure that no messages as lost.
				-- Physical MIDI is sent at 31.25kBaud.
				-- Every message is sent as 10bit message on the wire,
				-- so every MIDI byte needs 320usec.
				ARDOUR.LuaAPI.usleep (400 * parser:buffer_size ())
			end
		end
	end

	-- show progress dialog
	local pdialog = LuaDialog.ProgressWindow ("MIDI Tuning", true)
	pdialog:progress (0, "Tuning");

	-- MIDI Tuning message
	-- http://technogems.blogspot.com/2018/07/using-midi-tuning-specification-mts.html
	-- http://www.ludovico.net/download/materiale_didattico/midi/08_midi_tuning.pdf
	local syx = string.char (
	0xf0, 0x7e, -- non-realtime sysex
	0x00,       -- target-id
	0x08, 0x01, -- tuning, bulk dump reply
	0x00,       -- tuning program number 0 to 127 in hexadecimal
	-- 16 chars name (zero padded)
	0x53, 0x63, 0x6C, 0x2D, 0x4D, 0x54, 0x53, 0x00,  -- Scl-MTS
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	)
	tx_midi (syx, 22, 1)

	-- iterate over MIDI notes
	for nn = 0, 127 do
		if pdialog:canceled () then break end

		-- calculate the frequency of the note
		local fq = ref_freq * 2 ^ ((nn - 69) / 12)

		-- and then convert this frequency to the MIDI note number (and cent offset)
		local base, cent = freq_to_mts (fq)

		-- MTS uses two MIDI bytes (2^14) for cents
		local cc = math.floor (163.83 * cent + 0.5) | 0
		local cent_msb = (cc >> 7) & 127
		local cent_lsb = cc & 127

		--[[
		print (string.format ("MIDI-Note %3d | Freq: %8.2f Hz = note: %3d + %6.3f ct (0x%02x 0x%02x 0x%02x)",
		                      nn, fq, base, cent, base, cent_msb, cent_lsb))
		--]]

		if (base < 0 or base > 127) then
			if base < 0 then
				base = 0
			else
				base = 127
			end
			cent_msb = 0
			cent_lsb = 0
		end

		local syx = string.char (
			base,       -- semitone (MIDI note number to retune to, unit is 100 cents)
			cent_msb,   -- MSB of fractional part (1/128 semitone = 100/128 cents = .78125 cent units)
			cent_lsb,   -- LSB of fractional part (1/16384 semitone = 100/16384 cents = .0061 cent units)
			0xf7
		)
		tx_midi (syx, 3, 0)

		-- show progress
		pdialog:progress (nn / 127, string.format ("Note %d freq: %.2f (%d + %d)", nn, fq, base, cc))
		if pdialog:canceled () then break end

		::continue::
	end

	if not pdialog:canceled () then
		tx_midi (string.char ((checksum & 127), 0xf7), 2, 2)
	end

	-- hide modal progress dialog and destroy it
	pdialog:done ();

	tx = nil
	parser = nil
	collectgarbage () -- and release any references

	--dump:close ()

end end

-- simple icon
function icon (params) return function (ctx, width, height, fg)
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil(math.min (width, height) * .45) .. "px")
	txt:set_text ("MTS")
	local tw, th = txt:get_pixel_size ()
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	txt:show_in_cairo_context (ctx)
end end
