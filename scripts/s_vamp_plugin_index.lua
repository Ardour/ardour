ardour { ["type"] = "Snippet", name = "Vamp Plugin List" }
function factory () return function ()

	local plugins = ARDOUR.LuaAPI.Vamp.list_plugins ();
	for id in plugins:iter () do
		local vamp = ARDOUR.LuaAPI.Vamp(id, Session:nominal_sample_rate())
		local vp = vamp:plugin ()
		print (" --- VAMP Plugin ---")
		print ("Id:", vp:getIdentifier ())
		print ("Name:", vp:getName ())
		print ("Description:", vp:getDescription ())

		local progs = vp:getPrograms();
		if not progs:empty () then
			print ("Preset(s):")
			for p in progs:iter () do
				print (" *", p)
			end
		end

		local params = vp:getParameterDescriptors ()
		if not params:empty () then
			print ("Parameters(s):")
			for p in params:iter () do
				-- http://manual.ardour.org/lua-scripting/class_reference/#Vamp:PluginBase:ParameterDescriptor
				print (" * Id:", p.identifier, "Name:", p.name, "Desc:", p.description)
				local i = 0; for vn in p.valueNames:iter() do
					print ("   ^^  ", i, " -> ", vn)
					i = i + 1
				end
			end
		end

		local feats = vp:getOutputDescriptors ()
		if not feats:empty () then
			print ("Output(s):")
			for p in feats:iter () do
				-- http://manual.ardour.org/lua-scripting/class_reference/#Vamp:Plugin:OutputDescriptor
				print (" * Id:", p.identifier, "Name:", p.name, "Desc:", p.description)
			end
		end

	end
end end

