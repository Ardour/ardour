#!/usr/bin/lua5.3

top = arg[1] or "./"

function scripts ()
	local out = io.popen (
		string.format ("find '%s' -maxdepth 1 -type f -iname \"[^_]*.lua\" 2>/dev/null | grep -v '/s_'", top .. "share/scripts/")
	)
	return function()
		for file in out:lines() do
			return file
		end 
		return nil
	end
end

function list_script_types (h, t)
	function ardour (v)
		if v['type'] == t then
			local desc = string.gsub (v['description'], "\n", " ")
			desc = string.gsub (desc, "\t", " ")
			desc = string.gsub (desc, "  *", " ")
			desc = string.gsub (desc, " $", "")
			print ("<dt>" .. v['name'] .. "<dt><dd>" .. desc .. "</dd>")
		end
	end

	print ("<h2>" .. h .. "</h2>")
	print ("<dl>")
	for script in scripts () do
		loadfile (script)()
	end
	print ("</dl>")
end

list_script_types ("DSP Plugins", "dsp")
list_script_types ("Editor Actions", "EditorAction")
list_script_types ("Editor Callbacks", "EditorHook")
list_script_types ("Session Wide Realtime", "session")
list_script_types ("Session Templates", "SessionInit")
