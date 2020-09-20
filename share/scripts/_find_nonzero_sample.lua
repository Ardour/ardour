ardour {
	["type"] = "EditorAction",
	name = "Find non zero audio sample",
	author = "Ardour Team",
	description = [[Find the position of first non-zero audio sample in selected regions.]]
}

function factory () return function ()
	-- get Editor GUI Selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()

	-- allocate a buffer (float* in C)
	-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:DSP:DspShm
	local cmem = ARDOUR.DSP.DspShm (8192)
	local msg = ""

	-- iterate over selected regions
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:RegionSelection
	for r in sel.regions:regionlist ():iter () do
		-- test if it's an audio region
		if r:to_audioregion ():isnil () then
			goto next
		end

		-- to read the Region data, we use the Readable interface of the Region
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Readable
		local a = r.to_audioregion()
		local rd = a:to_readable ()

		local n_samples = rd:readable_length ()
		local n_channels = rd:n_channels ()

		local nonzeropos = -1

		-- iterate over all channels in Audio Region
		for c = 0, n_channels -1 do
			local pos = 0
			repeat
				-- read at most 8K samples of channel 'c' starting at 'pos'
				local s = rd:read (cmem:to_float (0), pos, 8192, c)
				-- access the raw audio data
				-- http://manual.ardour.org/lua-scripting/class_reference/#C:FloatArray
				local d = cmem:to_float (0):array()
				-- iterate over the audio sample data
				for i = 1, s do
					if math.abs (d[i]) > 0 then
						if (nonzeropos < 0 or pos + i < nonzeropos) then
							nonzeropos = pos + i - 1
						end
						break
					end
				end
				pos = pos + s
				if (nonzeropos >= 0 and pos > nonzeropos) then
					break
				end
			until s < 8192
		end

		if (nonzeropos >= 0) then
			msg = msg .. string.format("%s: %d\n", r:name (), nonzeropos + r:position())
		else
			msg = msg .. "Region: '%s' is silent\n"
		end

		::next::
	end

	if (msg ~= "") then
		local md = LuaDialog.Message ("First Non Zero Sample", msg, LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close)
		md:run()
	end

end end
