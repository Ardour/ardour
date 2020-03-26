ardour { ["type"] = "Snippet", name = "Vamp Audio Transcription Example" }

function factory () return function ()

	-- get Editor selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Editor
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()
	local sr = Session:nominal_sample_rate ()

	-- Instantiate a Vamp Plugin
	-- see http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:LuaAPI:Vamp
	local vamp = ARDOUR.LuaAPI.Vamp ("libardourvampplugins:qm-transcription", sr)

	-- prepare progress dialog
	local progress_total = 0;
	local progress_part = 0
	local pdialog = LuaDialog.ProgressWindow ("Audio to MIDI", true)
	function cb (_, pos)
		return pdialog:progress ((pos + progress_part) / progress_total, "Analyzing")
	end

	-- calculate max progress
	for r in sel.regions:regionlist ():iter () do
		progress_total = progress_total + r:to_readable ():readable_length ()
	end

	-- for each selected region
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:RegionSelection
	for r in sel.regions:regionlist ():iter () do
		-- "r" is-a http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Region

		vamp:analyze (r:to_readable (), 0, cb)

		if pdialog:canceled () then
			goto out
		end

		progress_part = progress_part + r:to_readable ():readable_length ()
		pdialog:progress (progress_part / progress_total, "Post Processing")

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

	::out::
	-- hide modal progress dialog and destroy it
	pdialog:done ();
end end
