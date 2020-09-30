ardour {
	["type"]    = "EditorAction",
	name        = "Rewind",
	author      = "Ardour Team",
	description = [[An Example Ardour Editor Action Script.]]
}

function factory (params)
	return function ()
		Session:goto_start()
	end
end
