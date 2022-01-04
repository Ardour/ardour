ardour { ["type"] = "EditorAction", name = "Add CD marker" }
function 
factory () 
	return function () 
		Editor:mouse_add_new_marker (Session:transport_sample(), Location.IsCDMarker)
	end 
end
