ardour { ["type"] = "Snippet", name = "VCA Slave Examples",
	license     = "MIT",
	author      = "Ardour Team",
}

function factory () return function ()
	-- find possible masters & slave, allow selection in dropdown menu
	local targets = {}
	local sources = {}
	local have_masters = false
	local have_slaves = false

	for v in Session:vca_manager ():vcas() :iter () do -- for each VCA
		sources [v:name ()] = v
		have_masters = true
	end

	for s in Session:get_stripables ():iter () do -- for every track/bus/vca
		if s:is_monitor () or s:is_auditioner () then goto nextroute end -- skip special routes
		targets [s:name ()] = s
		have_slaves = true;
		::nextroute::
	end

	-- bail out if there are no parameters
	if not have_slaves then
		LuaDialog.Message ("VCA Slave Example", "No Slavables found", LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run ()
		sources = nil
		collectgarbage ()
		return
	end
	if not have_masters then
		LuaDialog.Message ("VCA Slave Example", "No VCA masters found", LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run ()
		targets = nil
		collectgarbage ()
		return
	end

	-- create a dialog, ask user which master to assign to which slave
	local dialog_options = {
		{ type = "dropdown", key = "master", title = "Control Master", values = sources },
		{ type = "dropdown", key = "slave", title = "Control Slave", values = targets }
	}
	local rv = LuaDialog.Dialog ("Select VCA assignment", dialog_options):run ()

	targets = nil -- drop references (the table holds shared-pointer references to all strips)
	collectgarbage () -- and release the references immediately

	if not rv then return end -- user canceled the operation

	-- parse user response
	local mst = rv["master"]
	local slv = rv["slave"]
	assert (not slv:to_slavable ():isnil ())

	-- test if mst is already controlled by slv (directly or indirectly)
	-- if so, don't allow the connection
	if (not slv:to_slavable ():assigned_to (Session:vca_manager(), mst)) then
		-- if slv controlled by master, disconnect it
		if (slv:slaved_to (mst)) then
			slv:to_slavable ():unassign (mst)
		else
			slv:to_slavable ():assign (mst)
		end
	else
		LuaDialog.Message ("VCA Slave Example", "Recursive VCA assignment ignored", LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run ()
	end
end end
