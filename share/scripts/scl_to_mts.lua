ardour {
	["type"]    = "EditorAction",
	name        = "Scala to MIDI Tuning",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Read scala (.scl) tuning from a file, generate MIDI tuning standard (MTS) messages and send them to a MIDI port]]
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
	function freq_to_mts (hz)
		local note = math.floor (12. * log2 (hz / 440) + 69.0)
		local freq = 440.0 * 2.0 ^ ((note - 69) / 12);
		assert (freq > note)
		local cent = 1200.0 * log2 (hz / freq)
		return note, cent
	end

	local dialog_options = {
		{ type = "file", key = "file", title = "Select .scl file" },
		{ type = "dropdown", key = "tx", title = "MIDI SysEx Target", values = midi_targets () }
	}

	local rv = LuaDialog.Dialog ("Select Scala File and MIDI Taget", dialog_options):run ()
	dialog_options = nil -- drop references (track, plugins, shared ptr)
	collectgarbage () -- and release the references immediately

	if not rv then return end -- user cancelled

	-- read the scl file
	local freqtbl = {}
	local ln = 0
	local expected_len = 0
	local f = io.open (rv["file"], "r")

	if not f then
		LuaDialog.Message ("Scala to MTS", "File Not Found", LuaDialog.MessageType.Error, LuaDialog.ButtonType.Close):run ()
		return
	end

	-- parse scala file and convert all intervals to cents
	-- http://www.huygens-fokker.org/scala/scl_format.html
	freqtbl[1] = 0.0 -- implicit
	for line in f:lines () do
		line = string.gsub (line, "%s", "") -- remove all whitespace
		if line:sub(0,1) == '!' then goto nextline end -- comment
		ln = ln + 1
		if ln < 2 then goto nextline end -- name
		if ln < 3 then
			expected_len = tonumber (line) -- number of notes on scale
			if expected_len < 1 or expected_len > 256 then break end -- invalid file
			goto nextline
		end

		local cents
		if string.find (line, ".", 1, true) then
			cents = tonumber (line)
		else
			local n, d = string.match(line, "(%d+)/(%d+)")
			if n then
				cents = 1200 * log2 (n / d)
			else
				local n = tonumber (line)
				cents = 1200 * log2 (n)
			end
		end
		--print ("SCL", ln - 2, cents)
		freqtbl[ln - 1] = cents

		::nextline::
	end
	f:close ()

	-- We need at least one interval.
	-- While legal in scl, single note scales are not useful here.
	if expected_len < 1 or expected_len + 2 ~= ln then
		LuaDialog.Message ("Scala to MTS", "Invalid or unusable scale file.", LuaDialog.MessageType.Error, LuaDialog.ButtonType.Close):run ()
		return
	end

	assert (expected_len + 2 == ln)
	assert (expected_len > 0)

	-- TODO consider reading a .kbm file or make these configurable in the dialog
	-- http://www.huygens-fokker.org/scala/help.htm#mappings
	local ref_root = 60 -- middle C
	local ref_note = 69 -- A4
	local ref_freq = 440.0

	-- calculate frequency at ref_root
	local delta = ref_note - ref_root
	local delta_octv = math.floor (delta / expected_len)
	local delta_note = delta % expected_len

	local ref_base = ref_freq * 2 ^ ((freqtbl[delta_note + 1] + freqtbl[expected_len + 1] * delta_octv) / -1200)

	-- prepare sending data
	local tx = rv["tx"] -- output port
	local parser = ARDOUR.RawMidiParser () -- construct a MIDI parser

	-- show progress dialog
	local pdialog = LuaDialog.ProgressWindow ("Scala to MIDI Tuning", true)
	pdialog:progress (0, "Tuning");

	-- iterate over all MIDI notes
	for nn = 0, 127 do
		if pdialog:canceled () then break end

		-- calculate the note relative to kbm's ref_root
		delta = nn - ref_root
		delta_octv = math.floor (delta / expected_len)
		delta_note = delta % expected_len

		-- calculate the frequency of the note according to the scl
		local fq = ref_base * 2 ^ ((freqtbl[delta_note + 1] + freqtbl[expected_len + 1] * delta_octv) / 1200)

		-- and then convert this frequency to the MIDI note number (and cent offset)
		local base, cent = freq_to_mts (fq)

		-- MTS uses two MIDI bytes (2^14) for cents
		local cc = math.floor (163.83 * cent + 0.5) | 0
		local cent_lsb = (cc >> 7) & 127
		local cent_msb = cc & 127

		--print ("MIDI Note:", nn, "scale-note:", delta_note, "Octave:", delta_octv, "-> Freq:", fq, "= note:", base, "+", cent, "cents (", cc, ")")

		if (base < 0 or base > 127) then
			-- skip out of bounds MIDI notes
			goto continue
		end

		-- MIDI Tuning message
		-- http://www.microtonal-synthesis.com/MIDItuning.html
		local syx = string.char (
			0xf0, 0x7f, -- realtime sysex
			0x7f,       -- target-id
			0x08, 0x02, -- tuning, note change request
			0x00,       -- tuning program number 0 to 127 in hexadecimal
			0x01,       -- number of notes to be changed
			nn,         -- note number to be changed
			base,       -- semitone (MIDI note number to retune to, unit is 100 cents)
			cent_msb,   -- MSB of fractional part (1/128 semitone = 100/128 cents = .78125 cent units)
			cent_lsb,   -- LSB of fractional part (1/16384 semitone = 100/16384 cents = .0061 cent units)
			0xf7
		)

		-- parse message to C/C++ uint8_t* array (Validate message correctness. This
		-- also returns C/C++ uint8_t* array for direct use with write_immediate_event.)
		for b = 1, 12 do
			if parser:process_byte (syx:byte (b)) then
				tx:write_immediate_event (parser:buffer_size (), parser:midi_buffer ())
				-- Slow things down a bit to ensure that no messages as lost.
				-- Physical MIDI is sent at 31.25kBaud.
				-- Every message is sent as 10bit message on the wire,
				-- so every MIDI byte needs 320usec.
				ARDOUR.LuaAPI.usleep (400 * parser:buffer_size ())
			end
		end

		-- show progress
		pdialog:progress (nn / 127, string.format ("Note %d freq: %.2f (%d + %d)", nn, fq, base, cc))
		if pdialog:canceled () then break end

		::continue::
	end

	-- hide modal progress dialog and destroy it
	pdialog:done ();

end end

-- simple icon
function icon (params) return function (ctx, width, height, fg)
	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil(math.min (width, height) * .45) .. "px")
	txt:set_text ("SCL\nMTS")
	local tw, th = txt:get_pixel_size ()
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	txt:show_in_cairo_context (ctx)
end end
