ardour { ["type"] = "Snippet", name = "Set Region Gain" }

function factory () return function ()
	-- get Editor GUI Selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()

	-- allocate a buffer (float* in C)
	-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:DSP:DspShm
	local cmem = ARDOUR.DSP.DspShm (8192)

	-- prepare undo operation
	Session:begin_reversible_command ("Lua Region Gain")

	-- iterate over selected regions
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:RegionSelection
	for r in sel.regions:regionlist ():iter () do
		-- test if it's an audio region
		local ar = r:to_audioregion ()
		if ar:isnil () then
			goto next
		end

		-- to read the Region data, we use the Readable interface of the Region
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:AudioReadable
		local rd = ar:to_readable ()

		local n_samples = rd:readable_length ()
		local n_channels = rd:n_channels ()

		local peak = 0 -- the audio peak to be calculated

		-- iterate over all channels in Audio Region
		for c = 0, n_channels -1 do
			local pos = 0
			repeat
				-- read at most 8K samples of channel 'c' starting at 'pos'
				local s = rd:read (cmem:to_float (0), pos, 8192, c)
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
		end

		if (peak > 0) then
			print ("Region:", r:name (), "peak:", 20 * math.log (peak) / math.log(10), "dBFS")
		else
			print ("Region:", r:name (), " is silent")
		end

		-- normalize region
		if (peak > 0) then
			-- prepare for undo
			r:to_stateful ():clear_changes ()
			-- apply gain
			r:to_audioregion (): set_scale_amplitude (1 / peak)
			-- save changes (if any) to undo command
			Session:add_stateful_diff_command (r:to_statefuldestructible ())
		end

		::next::
	end

	if not Session:abort_empty_reversible_command () then
		Session:commit_reversible_command (nil)
	end

end end
