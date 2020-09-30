ardour {
	["type"]    = "EditorHook",
	name        = "Callback Example",
	author      = "Ardour Team",
	description = "Rewind On Solo Change, Write a file when regions are moved",
}

function signals ()
	s = LuaSignal.Set()
	--s:add ({[LuaSignal.SoloActive] = true, [LuaSignal.RegionPropertyChanged] = true})
	s:add (
		{
			[LuaSignal.SoloActive] = true,
			[LuaSignal.RegionPropertyChanged] = true
		}
	)
	--for k,v in pairs (s:table()) do print (k, v) end
	return s
end

function factory (params)
	return function (signal, ref, ...)
		print (signal, ref, ...)

		if (signal == LuaSignal.SoloActive) then
			Session:goto_start()
		end

		if (signal == LuaSignal.RegionPropertyChanged) then
			obj,pch = ...
			file = io.open ("/tmp/test" ,"a")
			io.output (file)
			io.write (string.format ("Region: '%s' pos-changed: %s, length-changed: %s\n",
				obj:name (),
				tostring (pch:containsSamplePos (ARDOUR.Properties.Start)),
				tostring (pch:containsSamplePos (ARDOUR.Properties.Length))
				))
			io.close (file)
		end
	end
end
