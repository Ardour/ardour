ardour { ["type"] = "Snippet", name = "Vamp Plugin Example" }

function factory () return function ()

	-- get a list of all available plugins
	-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:LuaAPI:Vamp
	-- returns a http://manual.ardour.org/lua-scripting/class_reference/#C:StringVector
	local plugins = ARDOUR.LuaAPI.Vamp.list_plugins ();
	for id in plugins:iter () do
		print ("--", id)
	end

	local sel = Editor:get_selection ()

	-- load the Vamp Plugin with Id "libardourvampplugins:dBTP"
	-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:LuaAPI:Vamp
	local vamp = ARDOUR.LuaAPI.Vamp("libardourvampplugins:dBTP", Session:nominal_sample_rate())
	print (vamp:plugin():getName())

	-- for each selected region
	for r in sel.regions:regionlist ():iter () do
		print ("Region:", r:name ())

		-- run the plugin, analyze the first channel of the audio-region
		vamp:analyze (r:to_readable (), 0, nil)

		-- get analysis results
		local f = vamp:plugin ():getRemainingFeatures ()

		-- f is-a Vamp::Plugin::FeatureSet  aka  std::map<int, Vamp::Plugin::FeatureList>
		-- http://manual.ardour.org/lua-scripting/class_reference/#Vamp:Plugin:FeatureSet
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
		-- http://manual.ardour.org/lua-scripting/class_reference/#Vamp:Plugin:Feature
		local feature = featurelist:at(0)
		-- ..and the values of the feature, which is-a std::vector<float>
		local values = feature.values
		-- iterate over the std::vector<float>
		for val in values:iter () do
			print ("*", val)
		end

		-- access the first element of Vamp::Plugin::Feature's "values" vector
		-- http://manual.ardour.org/lua-scripting/class_reference/#C:FloatVector
		local value = values:at(0)
		-- in case of libardourvampplugins:dBTP that's the true-peak (signal value)
		local dbtp = 20 * math.log (value) / math.log(10) -- convert it to dB
		print (string.format ("Region '%s': %.2f dBTP", r:name (), dbtp))

		-- reset the plugin for the next iteration
		vamp:reset ()
	end
end end
