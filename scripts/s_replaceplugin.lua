ardour { ["type"] = "Snippet", name = "Replace Plugin" }

function factory () return function ()

	route = Session:route_by_remote_id(1)
	old = route:nth_plugin(0)
	new = ARDOUR.LuaAPI.new_plugin(Session, "http://gareus.org/oss/lv2/fil4#stereo", ARDOUR.PluginType.LV2, "");
	route:replace_processor (old, new, nil)
	old = nil new = nil -- explicitly drop references (unless they're local vars)

end end
