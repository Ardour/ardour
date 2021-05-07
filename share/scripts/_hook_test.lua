ardour {
	["type"]    = "EditorHook",
	name        = "Callback Example",
	author      = "Ardour Team",
	description = "Rewind On Solo Change, Write a file when regions are moved",
}

function signals ()
	s = LuaSignal.Set()
	--s:add ({[LuaSignal.SoloActive] = true, [LuaSignal.RegionsPropertyChanged] = true})
	s:add (
		{
			[LuaSignal.SoloActive] = true,
			[LuaSignal.RegionsPropertyChanged] = true
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

		if (signal == LuaSignal.RegionsPropertyChanged) then
			rl,pch = ...
			file = io.open ("/tmp/test" ,"a")
			io.output (file)
			for region in rl:iter() do
				io.write (string.format ("Region: '%s' pos-changed: %s, length-changed: %s\n",
					region:name (),
					tostring (pch:containsSamplePos (ARDOUR.Properties.Position)),
					tostring (pch:containsSamplePos (ARDOUR.Properties.Length))
					))
			end
			io.close (file)
		end
	end
end
