ardour { ["type"] = "Snippet", name = "Vamp Plugin Example" }

function factory () return function ()
	local sel = Editor:get_selection ()

	local vamp = ARDOUR.LuaAPI.Vamp("libardourvampplugins:dBTP", Session:nominal_frame_rate())
	print (vamp:plugin():getName())

	-- for each selected region
	for r in sel.regions:regionlist ():iter () do
		print ("Region:", r:name ())

		-- run the plugin, analyze the first channel of the audio-region
		vamp:analyze (r:to_readable (), 0, nil)

		-- get analysis results
		local f = vamp:plugin ():getRemainingFeatures ()

		-- f is-a Vamp::Plugin::FeatureSet  aka  std::map<int, Vamp::Plugin::FeatureList>
		for id, featlist in f:iter () do
			print (id, featlist)
		end

		-- get the first FeatureList
		local featurelist = f:table()[0]
		-- Vamp::Plugin::FeatureList is a typedef for std::vector<Feature>
		for feat in featurelist:iter () do
			print ("-", feat.label)
		end

		-- get the first feature..
		local feature = featurelist:at(0)
		-- ..and the values of the feature, which is-a std::vector<float>
		local values = feature.values
		-- iterate over the std::vector<float>
		for val in values:iter () do
			print ("*", val)
		end

		-- access the first element of Vamp::Plugin::Feature's "values" vector
		local value = values:at(0)
		-- in case of libardourvampplugins:dBTP that's the true-peak
		local dbtp = 20 * math.log (value) / math.log(10)
		print (string.format ("Region '%s': %.2f dBTP", r:name (), dbtp))

		-- reset the plugin
		vamp:reset ()
	end
end end
