ardour {
	["type"]    = "EditorAction",
	name        = "Rewind",
}

function factory (params)
	return function ()
		Session:goto_start()
	end
end
