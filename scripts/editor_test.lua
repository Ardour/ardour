ardour {
	["type"]    = "EditorAction",
	name        = "Action Test",
	license     = "MIT",
	author      = "Robin Gareus",
	email       = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[ An Example Ardour Editor Action Plugin.]]
}

function factory (params)
	return function ()
		for n in pairs(_G) do print(n) end
		print ("----")
	end
end
