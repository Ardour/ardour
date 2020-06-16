ardour {
	["type"]    = "EditorAction",
	name        = "Scala to MIDI Tuning",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Read scala (.scl) tuning from a file, generate MIDI tuning standard (MTS) messages and send them to a MIDI port]]
}

function factory () return function ()

	function portlist ()
		local rv = {}
		local a = Session:engine()
		local _, t = a:get_ports (ARDOUR.DataType("midi"), ARDOUR.PortList())
		for p in t[2]:iter() do
			local amp = p:to_asyncmidiport ()
			if amp:isnil() or not amp:sends_output() then goto continue end
			rv[amp:name()] = amp
			--print (amp:name(), amp:sends_output())
			::continue::
		end
		return rv
	end

	function log2 (v)
		return math.log (v) / math.log (2)
	end

	function freq_to_mts (hz)
		local note = math.floor (12. * log2 (hz / 440) + 69.0)
		local freq = 440.0 * 2.0 ^ ((note - 69) / 12);
		assert (freq > note)
		local cent = 1200.0 * log2 (hz / freq)
		return note, cent
	end

	function calc_freq (hz, cent, octave)
		return hz * 2 ^ ((cent + 1200 * octave) / 1200)
	end

	local dialog_options = {
		{ type = "file", key = "file", title = "Select .scl MIDI file" },
		{ type = "dropdown", key = "port", title = "Target Port", values = portlist () }
	}

	local rv = LuaDialog.Dialog ("Select Taget", dialog_options):run ()
	dialog_options = nil -- drop references (ports, shared ptr)
	collectgarbage () -- and release the references immediately

	if not rv then return end -- user cancelled

	-- read the scl file
	local freqtbl = {}
	local ln = 0
	local expected_len = 0
	local f = io.open (rv["file"], "r")

	if not f then
		LuaDialog.Message ("Scala to MTS", "File Not Found", LuaDialog.MessageType.Error, LuaDialog.ButtonType.Close):run ()
		goto out
	end

	-- http://www.huygens-fokker.org/scala/scl_format.html
	freqtbl[1] = 0.0
	for line in f:lines () do
		line = string.gsub (line, "%s", "") -- remove all whitespace
		if line:sub(0,1) == '!' then goto nextline end -- comment
		ln = ln + 1
		if ln < 2 then goto nextline end -- name
		if ln < 3 then
			expected_len = tonumber (line)
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

	assert (expected_len + 2 == ln)
	assert (expected_len > 0)

	-- last entry should be an octave
	assert (freqtbl[expected_len + 1] == 1200)

	-- TODO consider kbm or make these configurable
	-- http://www.huygens-fokker.org/scala/help.htm#mappings
	local ref_root = 60 -- middle C
	local ref_note = 69 -- A4
	local ref_freq = 440.0

	-- calc frequency at ref_root
	local ref_base = ref_freq * 2.0 ^ ((ref_root - ref_note) / 12);

	local async_midi_port = rv["port"] -- reference to port
	local parser = ARDOUR.RawMidiParser () -- construct a MIDI parser

	-- show progress dialog
	local pdialog = LuaDialog.ProgressWindow ("Scala to MIDI Tuning", true)
	pdialog:progress (0, "Tuning");

	for nn = 0, 127 do
		if pdialog:canceled () then break end

		local delta = nn - ref_root
		local delta_octv = math.floor (delta / expected_len)
		local delta_note = delta % expected_len

		local fq = calc_freq (ref_base, freqtbl [ delta_note + 1 ], delta_octv)
		local base, cent = freq_to_mts (fq)

		local cc = math.floor (163.83 * cent + 0.5) | 0

		--print ("MIDI Note:", nn, "scale-note:", delta_note, "Octave:", delta_octv, "-> Freq:", fq, "= note:", base, "+", cent, "cent (", cc, ")")

		local cent_lsb = (cc >> 7) & 127
		local cent_msb = cc & 127

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

		for b = 1, 12 do
			if parser:process_byte (syx:byte (b)) then
				async_midi_port:write (parser:midi_buffer (), parser:buffer_size (), 0)
				-- Physical MIDI is sent at 31.25kBaud.
				-- Every message is sent as 10bit message on the wire,
				-- so every MIDI byte needs 320usec.
				ARDOUR.LuaAPI.usleep (400 * parser:buffer_size ())
			end
		end

		pdialog:progress (nn / 127, string.format ("Note %d freq: %.2f (%d + %d)", nn, fq, base, cc))
		if pdialog:canceled () then break end
	end

	-- hide modal progress dialog and destroy it
	pdialog:done ();

	::out::
end end
