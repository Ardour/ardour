ardour { ["type"] = "Snippet", name = "Vamp Onset Detection Example" }

function factory () return function ()

	-- get Editor selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Editor
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()

	-- Instantiate a Vamp Plugin
	-- see http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:LuaAPI:Vamp
	--
	-- here: the "Queen Mary Note Onset Detector" Vamp plugin (which comes with Ardour)
	-- http://vamp-plugins.org/plugin-doc/qm-vamp-plugins.html#qm-onsetdetector
	local vamp = ARDOUR.LuaAPI.Vamp("libardourvampplugins:qm-onsetdetector", Session:nominal_sample_rate())

	-- prepare table to hold results
	local onsets = {}

	-- for each selected region
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:RegionSelection
	for r in sel.regions:regionlist ():iter () do
		-- "r" is-a http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Region

		-- prepare lua table to hold results for the given region (by name)
		onsets[r:name ()] = {}

		-- callback to handle Vamp-Plugin analysis results
		function callback (feats)
			-- "feats" is-a http://manual.ardour.org/lua-scripting/class_reference/#Vamp:Plugin:FeatureSet
			-- get the first output. here: Detected note onset times
			local fl = feats:table()[0]
			-- "fl" is-a http://manual.ardour.org/lua-scripting/class_reference/#Vamp:Plugin:FeatureList
			-- which may be empty or not nil
			if fl then
				-- iterate over returned features
				for f in fl:iter () do
					-- "f" is-a  http://manual.ardour.org/lua-scripting/class_reference/#Vamp:Plugin:Feature
					if f.hasTimestamp then
						local fn = Vamp.RealTime.realTime2Frame (f.timestamp, 48000)
						--print ("-", f.timestamp:toString(), fn)
						table.insert (onsets[r:name ()],  fn)
					end
				end
			end
			return false -- continue, !cancel
		end

		-- Configure Vamp plugin
		--
		-- One could query the Parameter and Program List:
		--   http://manual.ardour.org/lua-scripting/class_reference/#Vamp:Plugin
		-- but since the Plugin is known, we can directly refer to the plugin doc:
		-- http://vamp-plugins.org/plugin-doc/qm-vamp-plugins.html#qm-onsetdetector
		vamp:plugin ():setParameter ("dftype", 3);
		vamp:plugin ():setParameter ("sensitivity", 50);
		vamp:plugin ():setParameter ("whiten", 0);
		-- in this case the above (3, 50, 0) is equivalent to
		--vamp:plugin ():selectProgram ("General purpose");

		-- run the plugin, analyze the first channel of the audio-region
		--
		-- This uses a "high-level" convenience wrapper provided by Ardour
		-- which reads raw audio-data from the region and and calls
		--     f = vamp:plugin ():process (); callback (f)
		vamp:analyze (r:to_readable (), 0, callback)

		-- get remaining features (end of analyis)
		callback (vamp:plugin ():getRemainingFeatures ())

		-- reset the plugin (prepare for next iteration)
		vamp:reset ()
	end

	-- print results
	for n,o in pairs(onsets) do
		print ("Onset analysis for region:", n)
		for _,t in ipairs(o) do
			print ("-", t)
		end
	end

end end
