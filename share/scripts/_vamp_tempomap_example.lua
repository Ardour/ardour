ardour { ["type"] = "Snippet", name = "Vamp TempoMap Test" }

function factory () return function ()

	-- get Editor selection
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Editor
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:Selection
	local sel = Editor:get_selection ()

	-- Instantiate the QM BarBeat Tracker
	-- see http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:LuaAPI:Vamp
	-- http://vamp-plugins.org/plugin-doc/qm-vamp-plugins.html#qm-barbeattracker
	local vamp = ARDOUR.LuaAPI.Vamp("libardourvampplugins:qm-barbeattracker", Session:nominal_sample_rate())

	-- prepare table to hold results
	local beats = {}
	local bars = {}

	-- for each selected region
	-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:RegionSelection
	for r in sel.regions:regionlist ():iter () do
		-- "r" is-a http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Region

		-- prepare lua table to hold results for the given region (by name)
		beats[r:name ()] = {}
		bars[r:name ()] = {}

		-- callback to handle Vamp-Plugin analysis results
		function callback (feats)
			-- "feats" is-a http://manual.ardour.org/lua-scripting/class_reference/#Vamp:Plugin:FeatureSet

			-- get the first output. here: Beats, estimated beat locations & beat-number
			local fl = feats:table()[0]
			-- "fl" is-a http://manual.ardour.org/lua-scripting/class_reference/#Vamp:Plugin:FeatureList
			-- which may be empty or not nil
			if fl then
				-- iterate over returned features
				for f in fl:iter () do
					-- "f" is-a  http://manual.ardour.org/lua-scripting/class_reference/#Vamp:Plugin:Feature
					if f.hasTimestamp then
						local fn = Vamp.RealTime.realTime2Frame (f.timestamp, 48000)
						table.insert (beats[r:name ()], {pos = fn, beat = f.label})
					end
				end
			end

			-- get the 2nd output. here: estimated bar locations
			local fl = feats:table()[1]
			if fl then
				for f in fl:iter () do
					if f.hasTimestamp then
						local fn = Vamp.RealTime.realTime2Frame (f.timestamp, 48000)
						table.insert (bars[r:name ()],  fn)
					end
				end
			end
			return false -- continue, !cancel
		end

		vamp:plugin ():setParameter ("Beats Per Bar", 4); -- TODO ask

		-- run the plugin, analyze the first channel of the audio-region
		vamp:analyze (r:to_readable (), 0, callback)
		-- get remaining features (end of analyis)
		callback (vamp:plugin ():getRemainingFeatures ())
		-- reset the plugin (prepare for next iteration)
		vamp:reset ()
	end

	-- print results (for now)
	-- TODO: calculate and set tempo-map
	for n,o in pairs(bars) do
		print ("Bar analysis for region:", n)
		for _,t in ipairs(o) do
			print ("-", t)
		end
	end
	for n,o in pairs(beats) do
		print ("Beat analysis for region:", n)
		for _,t in ipairs(o) do
			print ("-", t['pos'], t['beat'])
		end
	end

end end
