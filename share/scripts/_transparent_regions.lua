ardour {
	["type"]    = "EditorHook",
	name        = "Make all Regions Transparent",
	author      = "Ardour Team",
	description = "While the transport is looping, all regions become transparent.",
}

function signals ()
	return LuaSignal.Set():add (
		{
			[LuaSignal.TransportStateChange] = true,
			[LuaSignal.TransportLooped] = true,
		}
	)
end

function factory ()
	return function (signal, ref, ...)
	local all_regions = ARDOUR.RegionFactory.regions()
	for _, r in all_regions:iter() do
		local ar = r:to_audioregion ();
		if ar:isnil () then goto next end
		if ar:opaque () then 
			ar:set_opaque (false)
		end
		::next::
	end
end end
