ardour { ["type"] = "Snippet", name = "Set color transparency" }

function factory () return function ()
  -- See Ardour - Preferences -> Appearance -> Colors -> Transparency for modifier names
  ArdourUI.config():set_modifier("editable region", "= alpha:0.7")
  ArdourUI.config():set_modifier("ghost track midi fill", "= alpha:0.7")
end end
