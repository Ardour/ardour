ardour { ["type"] = "Snippet", name = "Dialog Test" }

function factory () return function ()
	local md = LuaDialog.Message ("title", "hello", LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close)
	print (md:run())

	md = nil
	collectgarbage ()

	-----------------------------------------------------------------
	function basic_serialize (o)
		if type(o) == "number" then
			return tostring(o)
		else
			return string.format("%q", o)
		end
	end

	function serialize (name, value)
		local rv = name .. ' = '
		collectgarbage()
		if type(value) == "number" or type(value) == "string" or type(value) == "nil" or type (value) == "boolean" then
			return rv .. basic_serialize(value) .. ' '
		elseif type(value) == "table" then
			rv = rv .. '{} '
			for k,v in pairs(value) do
				local fieldname = string.format("%s[%s]", name, basic_serialize(k))
				rv = rv .. serialize(fieldname, v) .. ' '
			end
			return rv
		elseif type(value) == "function" then
			--return rv .. string.format("%q", string.dump(value, true))
			return rv .. "(function)"
		else
			error('cannot serialize a ' .. type(value))
		end
	end
	-----------------------------------------------------------------

	function func () print "Hello" end

	local dialog_options = {
		{ type = "checkbox", key = "onoff", default = true, title = "OnOff" },

		{ type = "entry", key = "text", default = "changeme", title = "Text Entry" },

		{
			type = "radio", key = "select", title = "RadioBtn", values =
			{
				["Option 1"] = 1, ["Option 2"] = "2", ["Option A"] = 'A'
			},
			default = "Option 1"
		},

		{
			type = "dropdown", key = "dropdown", title = "Menu", values =
			{
				["Option 1"] = 1, ["Option 2"] = "2", ["Callback"] = func,
				["Option 4"] =
				{
					["Option 4a"] = "test", ["Option 4b"] = 4.2
				}
			},
			default = "Option 2"
		},

		{ type = "fader", key = "gain", title = "Level",  default = -10 }, -- unit = 'dB"

		{
			type = "slider", key = "freq", title = "Frequency", min = 20, max = 20000, scalepoints =
			{
				[20] = "20", [200] = "nice", [2000] = "2k", [10000] = "too much"
			},
			default = 500
		},

		{ type = "heading", title = "Heading" },

		{ type = "number", key = "number", title = "Whatever",  min = 0, max = 10, step = 1, digits = 2 },

		{ type = "file", key = "file", title = "Select a File",  path = ARDOUR.LuaAPI.build_filename (Session:path (), Session:name () .. ".ardour") },

		{ type = "folder", key = "folder", title = "Select a Folder",  path = Session:path() }
	}

	local od = LuaDialog.Dialog ("title", dialog_options)
	local rv = od:run()
	if (rv) then
		print (serialize ("dialog", rv))
	end
end end
