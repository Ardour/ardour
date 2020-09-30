ardour {
	["type"]    = "EditorHook",
	name        = "Load Session Hook Example",
	author      = "Ardour Team",
	description = "Display some dialogs during session load and execute actions",
}

-- subscribe to signals
-- http://manual.ardour.org/lua-scripting/class_reference/#LuaSignal.LuaSignal
function signals ()
	s = LuaSignal.Set()
	s:add ({[LuaSignal.SetSession] = true})
	return s
end

-- create callback functions
function factory () return function (signal, ...)
	assert (signal == LuaSignal.SetSession)
	local md = LuaDialog.Message ("Set Session", "Loading Session:" .. Session:name(), LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close)
	md:run()

	local dialog_options = {
		{ type = "checkbox", key = "tempo", default = true, title = "Show Tempo Ruler" },
		{ type = "checkbox", key = "meter", default = true, title = "Show Meter Ruler" },
	}
	local dlg = LuaDialog.Dialog ("Tweak Rulers", dialog_options)
	local rv = dlg:run()
	if (rv) then
		Editor:set_toggleaction ("Rulers", "toggle-tempo-ruler",  rv['tempo'])
		Editor:set_toggleaction ("Rulers", "toggle-meter-ruler",  rv['meter'])
	end
end end
