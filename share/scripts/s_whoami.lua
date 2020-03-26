ardour { ["type"] = "Snippet", name = "Who Am I?" }

function factory() return function()

function whoami()
	--pcall is the lua equivalent
	--of try: ... catch: ...
	if not pcall(function() local first_check = Session:get_mixbus(0) end) then
		return "Ardour"
	else
		local second_check = Session:get_mixbus(11)
		if second_check:isnil() then
			return "Mixbus"
		else
			return "32C"
		end
	end
end

print(whoami())

end end
