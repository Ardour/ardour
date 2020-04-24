ardour {
	["type"]    = "EditorAction",
	name        = "Mute All Tracks",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Mute all tracks in the session]]
}

function factory () return function ()
	local ctrls = ARDOUR.ControlListPtr () -- create a list of controls to change

	for r in Session:get_tracks ():iter () do -- iterate over all tracks in the session
		ctrls:push_back (r:mute_control()) -- add the track's mute-control to the list of of controls to be changed
	end

	-- of more than one control is to be changed..
	if ctrls:size() > 0 then
		-- do it (queue change in realtime-context) set to "1" ; here 'muted'
		Session:set_controls (ctrls, 1, PBD.GroupControlDisposition.NoGroup)
	end
end end
