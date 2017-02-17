ardour {
	["type"]    = "EditorAction",
	name        = "Rewind",
	author      = "Ardour Lua Task Force",
	description = [[An Example Ardour Editor Action Script.]]
}

function factory (params)
	return function ()
		Session:goto_start()
	end
end
