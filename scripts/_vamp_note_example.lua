ardour { ["type"] = "Snippet", name = "Vamp Audio Transcription Example" }

function factory () return function ()

	-- simple progress information print()ing
	--[[
	local progress_total;
	local progress_last;
	function cb (_f, pos)
		local progress = 100 * pos / progress_total;
		if progress - progress_last > 5 then
			progress_last = progress;
			print ("Progress: ", progress)
		end
	end
	--]]

	-- get Editor selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Editor
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()
	local sr = Session:nominal_sample_rate ()

	-- Instantiate a Vamp Plugin
	-- see http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:LuaAPI:Vamp
	local vamp = ARDOUR.LuaAPI.Vamp ("libardourvampplugins:qm-transcription", sr)

	-- for each selected region
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:RegionSelection
	for r in sel.regions:regionlist ():iter () do
		-- "r" is-a http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Region

		--[[
		progress_total = r:to_readable ():readable_length ()
		progress_last = 0
		--]]
		vamp:analyze (r:to_readable (), 0, nil --[[cb--]])
		print ("-- Post Processing: ", r:name ())

		-- post-processing takes longer than actually parsing the data :(
		local f = vamp:plugin ():getRemainingFeatures ()

		local fl = f:table ()[0]
		print (" Time (sample) |  Len  | Midi-Note");
		if fl then for f in fl:iter () do
			assert (f.hasTimestamp and f.hasDuration);
			local ft = Vamp.RealTime.realTime2Frame (f.timestamp, sr)
			local fd = Vamp.RealTime.realTime2Frame (f.duration, sr)
			local fn = f.values:at (0) -- midi note number
			print (string.format (" %14d %7d %d", ft, fd, fn))
		end end

		-- reset the plugin (prepare for next iteration)
		vamp:reset ()
	end

end end
