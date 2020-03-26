ardour { ["type"] = "Snippet", name = "Readable Test" }

function factory () return function ()

	local file = "/tmp/reverbs/Large Wide Echo Hall.wav"
	local rl = ARDOUR.Readable.load (Session, file)
	local cmem = ARDOUR.DSP.DspShm (8192)

	local d = cmem:to_float (0):array()
	for i = 1, 8192 do d[i] = 0 end
	d[1] = .5

	local ar = ARDOUR.AudioRom.new_rom (cmem:to_float (0), 8192)

	rl:push_back (ar)

	local c = 1
	for rd in rl:iter () do
		local n_samples = rd:readable_length ()
		assert (rd:n_channels () == 1)

		local peak = 0
		local pos = 0
		repeat
			-- read at most 8K samples starting at 'pos'
			local s = rd:read (cmem:to_float (0), pos, 8192, 0)
			pos = pos + s
			-- access the raw audio data
			-- http://manual.ardour.org/lua-scripting/class_reference/#C:FloatArray
			local d = cmem:to_float (0):array()
			-- iterate over the audio sample data
			for i = 1, s do
				if math.abs (d[i]) > peak then
					peak = math.abs (d[i])
				end
			end
		until s < 8192
		assert (pos == n_samples)

		if (peak > 0) then
			print ("File:", file, "channel", c, "peak:", 20 * math.log (peak) / math.log(10), "dBFS")
		else
			print ("File:", file, "channel", c, " is silent")
		end
		c = c + 1;
	end
end end
