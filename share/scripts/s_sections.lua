ardour { ["type"] = "Snippet", name = "List Sections" }
function factory () return function ()

	local s = Temporal.timepos_t(0)
	local e = Temporal.timepos_t(0)
	local loc = Session:locations ()

	local l = nil
	local cnt = 0
	repeat
		l, rv = loc:next_section (l, s, e)
		if l ~= nil then
			print (l:name (), rv[2], rv[3]);
		end
		cnt = cnt + 1
	until (l == nil or cnt > 10)

end end
