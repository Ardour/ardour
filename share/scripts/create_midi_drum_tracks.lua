ardour {
	["type"] = "EditorAction",
	name = "Create MIDI Drum Tracks V2",
	author = "Toni Link",
	description = [[Creates 12 new tracks with representative drum names and color. Based on the original script by PSmith.]]
}


function factory () return function ()
		local names = {
			"Kick",
			"Snare",
			"Rim",
			"Clap",
			"CHh",
			"OHh",
			"Cymb",
			"Crash",
			"LoTo",
			"HiTo",
			"FlTo",
			"Shaker"
		}

		local color = 0xff8800ff  --orange

		local i = 1
		while names[i] do
		  local tl = Session:new_midi_track(
			ARDOUR.ChanCount(ARDOUR.DataType ("midi"), 1),
								 ARDOUR.ChanCount(ARDOUR.DataType ("midi"), 1),
								 true,
						   ARDOUR.PluginInfo(), nil,
								 ARDOUR.RouteGroup(),
								 1, names[i],
						   ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal, true)

			for track in tl:iter () do
				track:presentation_info_ptr ():set_color (color)
			end

			i = i + 1
		end --foreach track

end end -- function factory

function icon (params)
return function (ctx, width, height)
-- 1. Fundo da Drum Machine
ctx:set_source_rgb (0.15, 0.15, 0.15)
ctx:rectangle (0, 0, width, height)
ctx:fill ()

-- 2. Configurações da Grade
local pad_size = width * 0.125 -- 2 pixels
local color_active = {0.9, 0.9, 0.9}
local color_inactive = {0.35, 0.35, 0.35}

-- 3. Desenho da Grade (4 colunas x 3 linhas) - Centralizada
for row = 0, 2 do
  for col = 0, 3 do
	local is_active = false

	-- Padrão rítmico mantido
	if (row == 0 and (col == 0 or col == 2)) then
	  is_active = true -- Bumbo
	  elseif (row == 1 and (col == 1 or col == 3)) then
		is_active = true -- Caixa
		elseif (row == 2) then
		  is_active = true -- Chimbal
		  end

		  if is_active then
			ctx:set_source_rgb (color_active[1], color_active[2], color_active[3])
			else
			  ctx:set_source_rgb (color_inactive[1], color_inactive[2], color_inactive[3])
			  end

			  -- X: Margem de 1px + (coluna * 4px de passo)
			  local px = width * 0.0625 + (col * width * 0.25)

			  -- Y: Margem de 3px (0.1875) para centralizar verticalmente
			  -- (3 linhas de 2px + 2 espaços de 2px = 10px ocupados)
			  local py = height * 0.1875 + (row * height * 0.25)

			  ctx:rectangle (px, py, pad_size, pad_size)
			  ctx:fill ()
			  end
			  end
			  end
			  end
