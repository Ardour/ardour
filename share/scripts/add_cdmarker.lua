ardour { ["type"] = "EditorAction", name = "Add CD marker" }
function 
factory () 
	return function () 
		Editor:add_location_mark (Temporal.timepos_t (Session:transport_sample()), ARDOUR.LocationFlags.IsCDMarker, 0)
	end 
end
