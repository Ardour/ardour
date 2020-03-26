ardour { ["type"] = "Snippet", name = "Toggle Monitor Section" }

function factory () return function () 
	if Session:monitor_out():isnil() then
		ARDOUR.config():set_use_monitor_bus (true)
	else
		ARDOUR.config():set_use_monitor_bus (false)
	end
end end
