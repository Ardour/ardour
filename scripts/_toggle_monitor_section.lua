ardour { ["type"] = "Snippet", name = "Toggle Monitor Section" }

function factory () return function () 
	if Session:monitor_out():isnil() then
		Session:add_monitor_section ()
		ARDOUR.config():set_use_monitor_bus (true)
	else
		Session:remove_monitor_section ()
		ARDOUR.config():set_use_monitor_bus (false)
		collectgarbage ()
	end
end end
