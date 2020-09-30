ardour {
	["type"] = "EditorAction",
	name = "File Name Test",
	author = "Ardour Team",
	description = [[Example Plugin to show to to select a file and remember the most recently used file.]]
}

function factory ()
	local file_name_testscript_last_filename -- this acts as "global" variable, use a unique name
	return function ()
		print (file_name_testscript_last_filename) -- debug

		--set filename to most recently used, fall back to use a default
		local fn = file_name_testscript_last_filename or ARDOUR.LuaAPI.build_filename (Session:path (), Session:name () .. ".ardour")

		-- prepare a dialog
		local dialog_options = {
			{ type = "file", key = "file", title = "Select a File",  path = fn }
		}

		-- show dialog
		local od = LuaDialog.Dialog ("title", dialog_options)
		local rv = od:run()

		if rv then
			-- remember most recently selected file
			file_name_testscript_last_filename = rv['file']
			LuaDialog.Message ("title", "set path to " .. file_name_testscript_last_filename, LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run()
		else
			-- unset most recently used filename on dialog "cancel"
			file_name_testscript_last_filename = nil
		end
	end
end
