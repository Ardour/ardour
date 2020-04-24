ardour {
	["type"]    = "EditorAction",
	name        = "Shortcut",
	license     = "MIT",
	author      = "me",
	description = [[Trigger a keyboard shortcut. You will be prompted for the shortcut's action in the next step.]]
}

function action_params ()
	local actionlist = {
		{
			type = "dropdown", key = "action", title = "Action", values = ArdourUI:actionlist(),
			default = "Save"
		}
	}

	local rv = LuaDialog.Dialog ("Select Action", actionlist):run ()
	if not rv then -- user cancelled
		return { ["x-script-abort"] = { title = "", preseeded = true} }
	end

	local action = rv["action"]
	local name = "Shortcut - " .. action
	return {
		["action"] = { title = "Action to trigger", default = action, preseeded = true},
		["x-script-name"] = { title = "Unique Script name", default = name, preseeded = true},
	}
end

function factory (params) return function ()
	local p = params or { }
	local as = assert (p["action"])
	local sp = assert (as:find('/'))
	local group = assert (as:sub(0, sp - 1))
	local item = assert (as:sub(1 + sp))
	Editor:access_action (group, item)
end end
