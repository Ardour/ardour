ardour { ["type"] = "EditorAction", name = "Delete xrun markers", author = "Ardour Team", description = [[Delete all xrun markers in the current session]] }

function factory () return function ()
	for l in Session:locations():list():iter() do
		if l:is_mark() and string.find (l:name(), "^xrun%d*$") then
			Session:locations():remove (l);
		end
	end
end end
