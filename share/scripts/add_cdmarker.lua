ardour { ["type"] = "EditorAction", name = "Add CD marker" }
function 
factory () 
	return function () 
		Editor:mouse_add_new_marker (Temporal.timepos_t (Session:transport_sample()), ARDOUR.LocationFlags.IsCDMarker, 0)
	end 
end
