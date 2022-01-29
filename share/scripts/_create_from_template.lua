ardour {
	["type"]    = "EditorAction",
	name        = "Create Track/Bus From Template",
	license     = "MIT",
	author      = "Vincent Tassy",
	description = [[Creates a  Track/Bus based on template]]
}

function factory () return function ()
	Session:new_route_from_template (1, ARDOUR.PresentationInfo.max_order, "/home/user/.config/ardour6/route_templates/Drums:Kick.template", "Kick", ARDOUR.PlaylistDisposition.NewPlaylist);
end end
