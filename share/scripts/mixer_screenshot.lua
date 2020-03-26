ardour {
	["type"]    = "EditorAction",
	name        = "Mixer Screenshot",
	author      = "Ardour Team",
	description = [[Save a screenshot of the complete mixer-window, regardless of scrollbars or visible screen area]]
}

function factory () return function ()
	local rv = LuaDialog.Dialog ("Save Mixer Screenshot",
	{
		{ type = "createfile", key = "file", title = "", path = ARDOUR.LuaAPI.build_filename(Session:path(), "export", "mixer.png") },
	}):run()

	if (rv) then
		if (ARDOUR.LuaAPI.file_test (rv['file'], ARDOUR.LuaAPI.FileTest.Exists)) then
			local ok = LuaDialog.Message ("File Exists", "File '".. rv['file'] .. "' exists.\nReplace?", LuaDialog.MessageType.Question, LuaDialog.ButtonType.Yes_No):run ()
			if ok ~= LuaDialog.Response.Yes then
				return
			end
		end
		ArdourUI.mixer_screenshot (rv['file'])
	end
end end

function icon (params) return function (ctx, width, height, fg)
	local wh = math.min (width, height) * .5
	ctx:translate (math.floor (width * .5 - wh), math.floor (height * .5 - wh))

	ctx:rectangle (wh * .6, wh * .6, wh * .8, wh * .8)
	ctx:set_source_rgba (.1, .1, .1, 1)
	ctx:fill ()

	ctx:set_line_width (1)
	ctx:set_source_rgba (.9, .9, .9, 1)

	ctx:move_to (wh * 0.3, wh * 0.6)
	ctx:line_to (wh * 1.5, wh * 0.6)
	ctx:line_to (wh * 1.5, wh * 1.7)
	ctx:stroke ()

	ctx:move_to (wh * 0.6, wh * 0.3)
	ctx:line_to (wh * 0.6, wh * 1.4)
	ctx:line_to (wh * 1.8, wh * 1.4)
	ctx:stroke ()
end end
