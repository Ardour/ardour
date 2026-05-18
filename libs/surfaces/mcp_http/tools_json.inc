R"mcp(
{
  "tools": [
    {
      "name": "hello_world",
      "title": "Hello World",
      "description": "Return a greeting from Ardour.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "name": {
            "type": "string"
          }
        },
        "additionalProperties": false
      }
    },
    {
      "name": "session_get_info",
      "title": "Session Info",
      "description": "Return basic session and transport info.",
      "inputSchema": {
        "type": "object",
        "properties": {},
        "additionalProperties": false
      }
    },
    {
      "name": "session_save",
      "title": "Save Session",
      "description": "Save current session state, optionally to a snapshot name.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "snapshotName": {
            "type": "string"
          },
          "switchToSnapshot": {
            "type": "boolean"
          }
        },
        "additionalProperties": false
      }
    },
    {
      "name": "session_undo",
      "title": "Undo",
      "description": "Undo one or more operations from session history.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "count": {
            "type": "integer",
            "minimum": 1
          }
        },
        "additionalProperties": false
      }
    },
    {
      "name": "session_redo",
      "title": "Redo",
      "description": "Redo one or more operations from session history.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "count": {
            "type": "integer",
            "minimum": 1
          }
        },
        "additionalProperties": false
      }
    },
    {
      "name": "session_rename",
      "title": "Rename Session",
      "description": "Rename current session.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "newName": {
            "type": "string"
          }
        },
        "required": [
          "newName"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "session_quick_snapshot",
      "title": "Quick Snapshot",
      "description": "Trigger quick snapshot action; optionally switch to the new snapshot.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "switchToSnapshot": {
            "type": "boolean"
          }
        },
        "additionalProperties": false
      }
    },
    {
      "name": "session_store_mixer_scene",
      "title": "Store Mixer Scene",
      "description": "Store mixer scene by index.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "index": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "index"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "session_recall_mixer_scene",
      "title": "Recall Mixer Scene",
      "description": "Recall mixer scene by index.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "index": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "index"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "transport_get_state",
      "title": "Transport State",
      "description": "Return transport state (rolling, speed, sample, state).",
      "inputSchema": {
        "type": "object",
        "properties": {},
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "rolling",
          "speed",
          "sample",
          "state"
        ],
        "properties": {
          "rolling": {
            "type": "boolean"
          },
          "speed": {
            "type": "number"
          },
          "sample": {
            "type": "integer"
          },
          "state": {
            "type": "string"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "transport_locate_sample",
      "title": "Transport Locate By Sample",
      "description": "Move playhead to an absolute sample position.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "sample": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "sample"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "requestedSample",
          "requestedBbt",
          "transport"
        ],
        "properties": {
          "requestedSample": {
            "type": "integer"
          },
          "requestedBbt": {
            "type": "object"
          },
          "transport": {
            "type": "object"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "transport_locate_bbt",
      "title": "Transport Locate By Bar Beat",
      "description": "Move playhead to an absolute musical location using {bar, beat}. beat may be fractional.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "bar": {
            "type": "integer",
            "minimum": 1
          },
          "beat": {
            "type": "number",
            "minimum": 1
          }
        },
        "required": [
          "bar",
          "beat"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "requestedSample",
          "requestedBbt",
          "transport"
        ],
        "properties": {
          "requestedSample": {
            "type": "integer"
          },
          "requestedBbt": {
            "type": "object"
          },
          "transport": {
            "type": "object"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "transport_goto_start",
      "title": "Transport Go To Start",
      "description": "Move playhead to session start. Optional andRoll starts playback after locate.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "andRoll": {
            "type": "boolean"
          }
        },
        "additionalProperties": false
      }
    },
    {
      "name": "transport_goto_end",
      "title": "Transport Go To End",
      "description": "Move playhead to session end.",
      "inputSchema": {
        "type": "object",
        "properties": {},
        "additionalProperties": false
      }
    },
    {
      "name": "transport_prev_marker",
      "title": "Transport Previous Marker",
      "description": "Move playhead to the previous visible marker/range boundary.",
      "inputSchema": {
        "type": "object",
        "properties": {},
        "additionalProperties": false
      }
    },
    {
      "name": "transport_next_marker",
      "title": "Transport Next Marker",
      "description": "Move playhead to the next visible marker/range boundary.",
      "inputSchema": {
        "type": "object",
        "properties": {},
        "additionalProperties": false
      }
    },
    {
      "name": "transport_loop_toggle",
      "title": "Transport Loop Toggle",
      "description": "Toggle loop playback, or set explicit loop state. Shows loop range if available.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "enabled": {
            "type": "boolean"
          }
        },
        "additionalProperties": false
      }
    },
    {
      "name": "transport_loop_location_samples",
      "title": "Transport Loop Location By Samples",
      "description": "Set loop range using absolute sample positions.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "startSample": {
            "type": "integer",
            "minimum": 0
          },
          "endSample": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "startSample",
          "endSample"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "properties": {
          "kind": {
            "type": "string"
          },
          "locationId": {
            "type": "string"
          },
          "name": {
            "type": "string"
          },
          "startSample": {
            "type": "integer"
          },
          "endSample": {
            "type": "integer"
          },
          "startBbt": {
            "type": "object"
          },
          "endBbt": {
            "type": "object"
          },
          "lengthSamples": {
            "type": "integer"
          },
          "hidden": {
            "type": "boolean"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "transport_loop_location_bbt",
      "title": "Transport Loop Location By Bar Beat",
      "description": "Set loop range using musical positions.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "startBar": {
            "type": "integer",
            "minimum": 1
          },
          "startBeat": {
            "type": "number",
            "minimum": 1
          },
          "endBar": {
            "type": "integer",
            "minimum": 1
          },
          "endBeat": {
            "type": "number",
            "minimum": 1
          }
        },
        "required": [
          "startBar",
          "startBeat",
          "endBar",
          "endBeat"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "properties": {
          "kind": {
            "type": "string"
          },
          "locationId": {
            "type": "string"
          },
          "name": {
            "type": "string"
          },
          "startSample": {
            "type": "integer"
          },
          "endSample": {
            "type": "integer"
          },
          "startBbt": {
            "type": "object"
          },
          "endBbt": {
            "type": "object"
          },
          "lengthSamples": {
            "type": "integer"
          },
          "hidden": {
            "type": "boolean"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "transport_set_record_enable",
      "title": "Set Global Record Enable",
      "description": "Set session record-enable (global rec arm) state.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "enabled": {
            "type": "boolean"
          }
        },
        "required": [
          "enabled"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "transport_set_speed",
      "title": "Set Transport Speed",
      "description": "Set transport speed (as in OSC set_transport_speed).",
      "inputSchema": {
        "type": "object",
        "properties": {
          "speed": {
            "type": "number"
          }
        },
        "required": [
          "speed"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "transport_play",
      "title": "Transport Play",
      "description": "Start transport playback.",
      "inputSchema": {
        "type": "object",
        "properties": {},
        "additionalProperties": false
      }
    },
    {
      "name": "transport_stop",
      "title": "Transport Stop",
      "description": "Stop transport playback.",
      "inputSchema": {
        "type": "object",
        "properties": {},
        "additionalProperties": false
      }
    },
    {
      "name": "markers_list",
      "title": "Markers List",
      "description": "List all session markers regardless of marker subtype.",
      "inputSchema": {
        "type": "object",
        "properties": {},
        "additionalProperties": false
      }
    },
    {
      "name": "markers_add",
      "title": "Add Marker",
      "description": "Add one marker at current playhead/audible position or a specific bar+beat. If name is omitted or empty, Ardour assigns a default name. Optional type: mark, section (arrangement), scene.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "name": {
            "type": "string"
          },
          "type": {
            "type": "string",
            "enum": [
              "mark",
              "section",
              "scene",
              "arrangement"
            ]
          },
          "bar": {
            "type": "integer",
            "minimum": 1
          },
          "beat": {
            "type": "number",
            "minimum": 1
          }
        },
        "additionalProperties": false
      }
    },
    {
      "name": "markers_add_range_samples",
      "title": "Add Range Marker By Samples",
      "description": "Add one range marker using absolute sample positions.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "name": {
            "type": "string"
          },
          "startSample": {
            "type": "integer",
            "minimum": 0
          },
          "endSample": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "startSample",
          "endSample"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "markers_add_range_bbt",
      "title": "Add Range Marker By Bar Beat",
      "description": "Add one range marker using musical positions.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "name": {
            "type": "string"
          },
          "startBar": {
            "type": "integer",
            "minimum": 1
          },
          "startBeat": {
            "type": "number",
            "minimum": 1
          },
          "endBar": {
            "type": "integer",
            "minimum": 1
          },
          "endBeat": {
            "type": "number",
            "minimum": 1
          }
        },
        "required": [
          "startBar",
          "startBeat",
          "endBar",
          "endBeat"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "markers_set_auto_loop_samples",
      "title": "Set Auto Loop Range By Samples",
      "description": "Set auto-loop range using absolute sample positions.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "startSample": {
            "type": "integer",
            "minimum": 0
          },
          "endSample": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "startSample",
          "endSample"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "markers_set_auto_loop_bbt",
      "title": "Set Auto Loop Range By Bar Beat",
      "description": "Set auto-loop range using musical positions.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "startBar": {
            "type": "integer",
            "minimum": 1
          },
          "startBeat": {
            "type": "number",
            "minimum": 1
          },
          "endBar": {
            "type": "integer",
            "minimum": 1
          },
          "endBeat": {
            "type": "number",
            "minimum": 1
          }
        },
        "required": [
          "startBar",
          "startBeat",
          "endBar",
          "endBeat"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "markers_hide_auto_loop",
      "title": "Hide Auto Loop Range",
      "description": "Hide or show the current auto-loop range without changing its endpoints.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "hidden": {
            "type": "boolean"
          }
        },
        "additionalProperties": false
      }
    },
    {
      "name": "markers_set_auto_punch_samples",
      "title": "Set Auto Punch Range By Samples",
      "description": "Set auto-punch range using absolute sample positions.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "startSample": {
            "type": "integer",
            "minimum": 0
          },
          "endSample": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "startSample",
          "endSample"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "markers_set_auto_punch_bbt",
      "title": "Set Auto Punch Range By Bar Beat",
      "description": "Set auto-punch range using musical positions.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "startBar": {
            "type": "integer",
            "minimum": 1
          },
          "startBeat": {
            "type": "number",
            "minimum": 1
          },
          "endBar": {
            "type": "integer",
            "minimum": 1
          },
          "endBeat": {
            "type": "number",
            "minimum": 1
          }
        },
        "required": [
          "startBar",
          "startBeat",
          "endBar",
          "endBeat"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "markers_hide_auto_punch",
      "title": "Hide Auto Punch Range",
      "description": "Hide or show the current auto-punch range without changing its endpoints.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "hidden": {
            "type": "boolean"
          }
        },
        "additionalProperties": false
      }
    },
    {
      "name": "markers_delete_by_location_id",
      "title": "Delete Marker By Location ID",
      "description": "Delete one marker or range by locationId.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "locationId": {
            "type": "string"
          }
        },
        "required": [
          "locationId"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "markers_delete_by_name",
      "title": "Delete Marker By Name",
      "description": "Delete one marker or range by name. Optional sample disambiguates duplicates.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "name": {
            "type": "string"
          },
          "sample": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "name"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "markers_rename_by_location_id",
      "title": "Rename Marker By Location ID",
      "description": "Rename one marker or range by locationId.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "locationId": {
            "type": "string"
          },
          "newName": {
            "type": "string"
          }
        },
        "required": [
          "locationId",
          "newName"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "markers_rename_by_name",
      "title": "Rename Marker By Name",
      "description": "Rename one marker or range by name. Optional sample disambiguates duplicates.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "name": {
            "type": "string"
          },
          "sample": {
            "type": "integer",
            "minimum": 0
          },
          "newName": {
            "type": "string"
          }
        },
        "required": [
          "name",
          "newName"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "tracks_list",
      "title": "Tracks List",
      "description": "List session tracks and buses. includeHidden=true includes hidden routes.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "includeHidden": {
            "type": "boolean"
          }
        },
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "tracks"
        ],
        "properties": {
          "tracks": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "tracks_add",
      "title": "Add Track",
      "description": "Add one or more tracks (audio or MIDI). insert can be end/before/after; when before/after and relativeToId is omitted, selected route is used if available.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "type": {
            "type": "string",
            "enum": [
              "audio",
              "midi"
            ]
          },
          "count": {
            "type": "integer",
            "minimum": 1
          },
          "name": {
            "type": "string"
          },
          "inputChannels": {
            "type": "integer",
            "minimum": 1
          },
          "outputChannels": {
            "type": "integer",
            "minimum": 1
          },
          "strictIo": {
            "type": "boolean"
          },
          "insert": {
            "type": "string",
            "enum": [
              "end",
              "before",
              "after"
            ]
          },
          "relativeToId": {
            "type": "string"
          }
        },
        "additionalProperties": false
      }
    },
    {
      "name": "buses_add",
      "title": "Add Bus",
      "description": "Add one or more buses (audio or MIDI). insert can be end/before/after; when before/after and relativeToId is omitted, selected route is used if available.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "type": {
            "type": "string",
            "enum": [
              "audio",
              "midi"
            ]
          },
          "count": {
            "type": "integer",
            "minimum": 1
          },
          "name": {
            "type": "string"
          },
          "inputChannels": {
            "type": "integer",
            "minimum": 1
          },
          "outputChannels": {
            "type": "integer",
            "minimum": 1
          },
          "strictIo": {
            "type": "boolean"
          },
          "insert": {
            "type": "string",
            "enum": [
              "end",
              "before",
              "after"
            ]
          },
          "relativeToId": {
            "type": "string"
          }
        },
        "additionalProperties": false
      }
    },
    {
      "name": "track_get_info",
      "title": "Get Route Info",
      "description": "Get one route info (fader, pan, rec, mute, solo, sends, plugins). Pan uses Ardour convention: 0.0=right, 0.5=center, 1.0=left.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          }
        },
        "required": [
          "id"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "id",
          "name",
          "type"
        ],
        "properties": {
          "id": {
            "type": "string"
          },
          "name": {
            "type": "string"
          },
          "type": {
            "type": "string"
          },
          "sends": {
            "type": "array"
          },
          "plugins": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "track_get_regions",
      "title": "Get Track Regions",
      "description": "List regions in one track playlist. includeHidden=true includes hidden regions.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "includeHidden": {
            "type": "boolean"
          }
        },
        "required": [
          "id"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "id",
          "regions"
        ],
        "properties": {
          "id": {
            "type": "string"
          },
          "regions": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_get_info",
      "title": "Get Region Info",
      "description": "Get one region info. If regionId is omitted, resolves to selected track's top region at playhead. includeAnalysis=true adds audio peak/RMS analysis.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "includeAnalysis": {
            "type": "boolean"
          }
        },
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "type",
          "startSample",
          "endSample",
          "isAudio"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "resolvedVia": {
            "type": "string"
          },
          "type": {
            "type": "string"
          },
          "isAudio": {
            "type": "boolean"
          },
          "audio": {
            "type": [
              "object",
              "null"
            ]
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_set_gain_linear",
      "title": "Set Region Gain Linear",
      "description": "Set one audio region gain using linear gain. Optional invertPolarity toggles polarity.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "linear": {
            "type": "number",
            "minimum": 0
          },
          "invertPolarity": {
            "type": "boolean"
          }
        },
        "required": [
          "linear"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "updated",
          "gain"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "updated": {
            "type": "boolean"
          },
          "gain": {
            "type": "object"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_set_gain_db",
      "title": "Set Region Gain dB",
      "description": "Set one audio region gain using dB. Optional invertPolarity toggles polarity.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "db": {
            "type": "number"
          },
          "invertPolarity": {
            "type": "boolean"
          }
        },
        "required": [
          "db"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "updated",
          "gain"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "updated": {
            "type": "boolean"
          },
          "gain": {
            "type": "object"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_normalize",
      "title": "Normalize Region",
      "description": "Normalize one audio region to targetDb (default 0.0 dBFS).",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "targetDb": {
            "type": "number"
          }
        },
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "targetDb",
          "peakAmplitude",
          "updated",
          "gain"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "targetDb": {
            "type": "number"
          },
          "peakAmplitude": {
            "type": "number"
          },
          "updated": {
            "type": "boolean"
          },
          "gain": {
            "type": "object"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_split",
      "title": "Split Region",
      "description": "Split one region (audio or MIDI) at sample, bar+beat, or current playhead if no split point is provided. If regionId is omitted, uses selected track's top region at playhead.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "sample": {
            "type": "integer",
            "minimum": 0
          },
          "bar": {
            "type": "integer",
            "minimum": 1
          },
          "beat": {
            "type": "number",
            "minimum": 1
          }
        },
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "split",
          "splitSample",
          "createdRegions"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "split": {
            "type": "boolean"
          },
          "splitSample": {
            "type": "integer"
          },
          "createdRegions": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_resize_samples",
      "title": "Resize Region By Samples",
      "description": "Resize one region (audio or MIDI) by updating startSample and/or endSample. If regionId is omitted, uses selected track's top region at playhead.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "startSample": {
            "type": "integer",
            "minimum": 0
          },
          "endSample": {
            "type": "integer",
            "minimum": 0
          }
        },
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "startSample",
          "endSample"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "resolvedVia": {
            "type": "string"
          },
          "startSample": {
            "type": "integer"
          },
          "endSample": {
            "type": "integer"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_resize_bbt",
      "title": "Resize Region By Bar Beat",
      "description": "Resize one region (audio or MIDI) by updating startBar/startBeat and/or endBar/endBeat. If regionId is omitted, uses selected track's top region at playhead.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "startBar": {
            "type": "integer",
            "minimum": 1
          },
          "startBeat": {
            "type": "number",
            "minimum": 1
          },
          "endBar": {
            "type": "integer",
            "minimum": 1
          },
          "endBeat": {
            "type": "number",
            "minimum": 1
          }
        },
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "startSample",
          "endSample"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "resolvedVia": {
            "type": "string"
          },
          "startSample": {
            "type": "integer"
          },
          "endSample": {
            "type": "integer"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_copy_to_sample",
      "title": "Copy Region To Sample",
      "description": "Copy one region (audio or MIDI) to an absolute sample position. Optional trackId copies to another track. If regionId is omitted, uses selected track's top region at playhead.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "trackId": {
            "type": "string"
          },
          "sample": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "sample"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "sourceRegionId",
          "startSample",
          "endSample"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "sourceRegionId": {
            "type": "string"
          },
          "resolvedVia": {
            "type": "string"
          },
          "startSample": {
            "type": "integer"
          },
          "endSample": {
            "type": "integer"
          },
          "crossTrack": {
            "type": "boolean"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_copy_to_bbt",
      "title": "Copy Region To Bar Beat",
      "description": "Copy one region (audio or MIDI) to an absolute musical position. Optional trackId copies to another track. If regionId is omitted, uses selected track's top region at playhead.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "trackId": {
            "type": "string"
          },
          "bar": {
            "type": "integer",
            "minimum": 1
          },
          "beat": {
            "type": "number",
            "minimum": 1
          }
        },
        "required": [
          "bar",
          "beat"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "sourceRegionId",
          "startSample",
          "endSample"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "sourceRegionId": {
            "type": "string"
          },
          "resolvedVia": {
            "type": "string"
          },
          "startSample": {
            "type": "integer"
          },
          "endSample": {
            "type": "integer"
          },
          "crossTrack": {
            "type": "boolean"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_copy_by_samples",
      "title": "Copy Region By Sample Delta",
      "description": "Copy one region (audio or MIDI) by a sample delta. Optional trackId copies to another track. If regionId is omitted, uses selected track's top region at playhead.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "trackId": {
            "type": "string"
          },
          "deltaSamples": {
            "type": "integer"
          }
        },
        "required": [
          "deltaSamples"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "sourceRegionId",
          "startSample",
          "endSample"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "sourceRegionId": {
            "type": "string"
          },
          "resolvedVia": {
            "type": "string"
          },
          "startSample": {
            "type": "integer"
          },
          "endSample": {
            "type": "integer"
          },
          "crossTrack": {
            "type": "boolean"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_copy_by_beats",
      "title": "Copy Region By Beat Delta",
      "description": "Copy one region (audio or MIDI) by a beat delta. deltaBeats may be fractional. Optional trackId copies to another track. If regionId is omitted, uses selected track's top region at playhead.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "trackId": {
            "type": "string"
          },
          "deltaBeats": {
            "type": "number"
          }
        },
        "required": [
          "deltaBeats"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "sourceRegionId",
          "startSample",
          "endSample"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "sourceRegionId": {
            "type": "string"
          },
          "resolvedVia": {
            "type": "string"
          },
          "startSample": {
            "type": "integer"
          },
          "endSample": {
            "type": "integer"
          },
          "crossTrack": {
            "type": "boolean"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_move_to_sample",
      "title": "Move Region To Sample",
      "description": "Move one region (audio or MIDI) to an absolute sample position. Optional trackId moves to another track. If regionId is omitted, uses selected track's top region at playhead.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "trackId": {
            "type": "string"
          },
          "sample": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "sample"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "sourceRegionId",
          "startSample",
          "endSample"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "sourceRegionId": {
            "type": "string"
          },
          "resolvedVia": {
            "type": "string"
          },
          "startSample": {
            "type": "integer"
          },
          "endSample": {
            "type": "integer"
          },
          "crossTrack": {
            "type": "boolean"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_move_to_bbt",
      "title": "Move Region To Bar Beat",
      "description": "Move one region (audio or MIDI) to an absolute musical position. Optional trackId moves to another track. If regionId is omitted, uses selected track's top region at playhead.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "trackId": {
            "type": "string"
          },
          "bar": {
            "type": "integer",
            "minimum": 1
          },
          "beat": {
            "type": "number",
            "minimum": 1
          }
        },
        "required": [
          "bar",
          "beat"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "sourceRegionId",
          "startSample",
          "endSample"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "sourceRegionId": {
            "type": "string"
          },
          "resolvedVia": {
            "type": "string"
          },
          "startSample": {
            "type": "integer"
          },
          "endSample": {
            "type": "integer"
          },
          "crossTrack": {
            "type": "boolean"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_move_by_samples",
      "title": "Move Region By Sample Delta",
      "description": "Move one region (audio or MIDI) by a sample delta. Optional trackId moves to another track. If regionId is omitted, uses selected track's top region at playhead.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "trackId": {
            "type": "string"
          },
          "deltaSamples": {
            "type": "integer"
          }
        },
        "required": [
          "deltaSamples"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "sourceRegionId",
          "startSample",
          "endSample"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "sourceRegionId": {
            "type": "string"
          },
          "resolvedVia": {
            "type": "string"
          },
          "startSample": {
            "type": "integer"
          },
          "endSample": {
            "type": "integer"
          },
          "crossTrack": {
            "type": "boolean"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "region_move_by_beats",
      "title": "Move Region By Beat Delta",
      "description": "Move one region (audio or MIDI) by a beat delta. deltaBeats may be fractional. Optional trackId moves to another track. If regionId is omitted, uses selected track's top region at playhead.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "trackId": {
            "type": "string"
          },
          "deltaBeats": {
            "type": "number"
          }
        },
        "required": [
          "deltaBeats"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "regionId",
          "sourceRegionId",
          "startSample",
          "endSample"
        ],
        "properties": {
          "regionId": {
            "type": "string"
          },
          "sourceRegionId": {
            "type": "string"
          },
          "resolvedVia": {
            "type": "string"
          },
          "startSample": {
            "type": "integer"
          },
          "endSample": {
            "type": "integer"
          },
          "crossTrack": {
            "type": "boolean"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "plugin_get_description",
      "title": "Plugin Description",
      "description": "Get plugin descriptor and parameter metadata for a route plugin.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "id",
          "pluginIndex"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "plugin_set_parameter_by_index_value",
      "title": "Set Plugin Parameter By Index Value",
      "description": "Set one plugin parameter identified by parameterIndex using its native/internal value range.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer",
            "minimum": 0
          },
          "parameterIndex": {
            "type": "integer",
            "minimum": 0
          },
          "value": {
            "type": "number"
          }
        },
        "required": [
          "id",
          "pluginIndex",
          "parameterIndex",
          "value"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "id",
          "pluginIndex",
          "parameterIndex",
          "controlId",
          "value",
          "interface"
        ],
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer"
          },
          "parameterIndex": {
            "type": "integer"
          },
          "controlId": {
            "type": "integer"
          },
          "value": {
            "type": "number"
          },
          "interface": {
            "type": "number"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "plugin_set_parameter_by_index_interface",
      "title": "Set Plugin Parameter By Index Interface",
      "description": "Set one plugin parameter identified by parameterIndex using normalized/display-mapped interface value.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer",
            "minimum": 0
          },
          "parameterIndex": {
            "type": "integer",
            "minimum": 0
          },
          "interface": {
            "type": "number"
          }
        },
        "required": [
          "id",
          "pluginIndex",
          "parameterIndex",
          "interface"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "id",
          "pluginIndex",
          "parameterIndex",
          "controlId",
          "value",
          "interface"
        ],
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer"
          },
          "parameterIndex": {
            "type": "integer"
          },
          "controlId": {
            "type": "integer"
          },
          "value": {
            "type": "number"
          },
          "interface": {
            "type": "number"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "plugin_set_parameter_by_control_value",
      "title": "Set Plugin Parameter By Control Value",
      "description": "Set one plugin parameter identified by controlId using its native/internal value range.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer",
            "minimum": 0
          },
          "controlId": {
            "type": "integer",
            "minimum": 0
          },
          "value": {
            "type": "number"
          }
        },
        "required": [
          "id",
          "pluginIndex",
          "controlId",
          "value"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "id",
          "pluginIndex",
          "parameterIndex",
          "controlId",
          "value",
          "interface"
        ],
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer"
          },
          "parameterIndex": {
            "type": "integer"
          },
          "controlId": {
            "type": "integer"
          },
          "value": {
            "type": "number"
          },
          "interface": {
            "type": "number"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "plugin_set_parameter_by_control_interface",
      "title": "Set Plugin Parameter By Control Interface",
      "description": "Set one plugin parameter identified by controlId using normalized/display-mapped interface value.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer",
            "minimum": 0
          },
          "controlId": {
            "type": "integer",
            "minimum": 0
          },
          "interface": {
            "type": "number"
          }
        },
        "required": [
          "id",
          "pluginIndex",
          "controlId",
          "interface"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "id",
          "pluginIndex",
          "parameterIndex",
          "controlId",
          "value",
          "interface"
        ],
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer"
          },
          "parameterIndex": {
            "type": "integer"
          },
          "controlId": {
            "type": "integer"
          },
          "value": {
            "type": "number"
          },
          "interface": {
            "type": "number"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "plugin_set_enabled",
      "title": "Set Plugin Enabled",
      "description": "Enable or disable one route plugin by index.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer",
            "minimum": 0
          },
          "enabled": {
            "type": "boolean"
          }
        },
        "required": [
          "id",
          "pluginIndex",
          "enabled"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "plugin_remove",
      "title": "Remove Plugin",
      "description": "Remove one route plugin by index.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "id",
          "pluginIndex"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "removed",
          "id",
          "pluginIndex",
          "plugins"
        ],
        "properties": {
          "removed": {
            "type": "boolean"
          },
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer"
          },
          "plugins": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "plugin_reorder",
      "title": "Reorder Plugin",
      "description": "Move one route plugin from fromIndex to toIndex.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "fromIndex": {
            "type": "integer",
            "minimum": 0
          },
          "toIndex": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "id",
          "fromIndex",
          "toIndex"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "moved",
          "id",
          "fromIndex",
          "toIndex",
          "plugins"
        ],
        "properties": {
          "moved": {
            "type": "boolean"
          },
          "id": {
            "type": "string"
          },
          "fromIndex": {
            "type": "integer"
          },
          "toIndex": {
            "type": "integer"
          },
          "plugins": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "plugin_set_position",
      "title": "Set Plugin Position",
      "description": "Move one route plugin between pre-fader and post-fader sections.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer",
            "minimum": 0
          },
          "postFader": {
            "type": "boolean"
          }
        },
        "required": [
          "id",
          "pluginIndex",
          "postFader"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "moved",
          "id",
          "pluginIndex",
          "preFader",
          "postFader",
          "plugins"
        ],
        "properties": {
          "moved": {
            "type": "boolean"
          },
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer"
          },
          "preFader": {
            "type": "boolean"
          },
          "postFader": {
            "type": "boolean"
          },
          "plugins": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "plugin_list_available",
      "title": "List Available Plugins",
      "description": "List discovered plugins from Ardour's plugin manager. Returns stable pluginId values suitable for plugin_add_by_plugin_id.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "search": {
            "type": "string"
          },
          "type": {
            "type": "string"
          },
          "includeHidden": {
            "type": "boolean"
          },
          "includeInternal": {
            "type": "boolean"
          }
        },
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "count",
          "plugins"
        ],
        "properties": {
          "count": {
            "type": "integer"
          },
          "plugins": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "plugin_add_by_plugin_id",
      "title": "Add Plugin By Plugin ID",
      "description": "Add one plugin to a route by pluginId. Optional position inserts at a plugin index.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginId": {
            "type": "string"
          },
          "position": {
            "type": "integer",
            "minimum": 0
          },
          "enabled": {
            "type": "boolean"
          }
        },
        "required": [
          "id",
          "pluginId"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "id",
          "pluginIndex",
          "plugins"
        ],
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer"
          },
          "plugins": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "plugin_add_by_unique_id",
      "title": "Add Plugin By Unique ID",
      "description": "Add one plugin to a route by uniqueId and type. Optional position inserts at a plugin index.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "uniqueId": {
            "type": "string"
          },
          "type": {
            "type": "string"
          },
          "position": {
            "type": "integer",
            "minimum": 0
          },
          "enabled": {
            "type": "boolean"
          }
        },
        "required": [
          "id",
          "uniqueId",
          "type"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "id",
          "pluginIndex",
          "plugins"
        ],
        "properties": {
          "id": {
            "type": "string"
          },
          "pluginIndex": {
            "type": "integer"
          },
          "plugins": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "track_get_fader",
      "title": "Get Route Fader",
      "description": "Get current route fader as normalized position and dB.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          }
        },
        "required": [
          "id"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "track_select",
      "title": "Select Track",
      "description": "Select a route in Ardour.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          }
        },
        "required": [
          "id"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "track_rename",
      "title": "Rename Track",
      "description": "Rename one route by ID.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "newName": {
            "type": "string"
          }
        },
        "required": [
          "id",
          "newName"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "track_set_mute",
      "title": "Set Track Mute",
      "description": "Set route mute state.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "value": {
            "type": "boolean"
          }
        },
        "required": [
          "id",
          "value"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "track_set_solo",
      "title": "Set Track Solo",
      "description": "Set route solo state.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "value": {
            "type": "boolean"
          }
        },
        "required": [
          "id",
          "value"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "track_set_rec_enable",
      "title": "Set Record Enable",
      "description": "Set track record-enable state.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "value": {
            "type": "boolean"
          }
        },
        "required": [
          "id",
          "value"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "track_set_rec_safe",
      "title": "Set Record Safe",
      "description": "Set track record-safe state.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "value": {
            "type": "boolean"
          }
        },
        "required": [
          "id",
          "value"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "track_set_pan",
      "title": "Set Pan",
      "description": "Set route pan position (Ardour convention: 0.0=right, 0.5=center, 1.0=left).",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "position": {
            "type": "number",
            "minimum": 0,
            "maximum": 1
          }
        },
        "required": [
          "id",
          "position"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "track_set_send_level_position",
      "title": "Set Send Level Position",
      "description": "Set route send level by send index using normalized position (0.0 to 1.0).",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "sendIndex": {
            "type": "integer",
            "minimum": 0
          },
          "position": {
            "type": "number",
            "minimum": 0,
            "maximum": 1
          }
        },
        "required": [
          "id",
          "sendIndex",
          "position"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "track_set_send_level_db",
      "title": "Set Send Level dB",
      "description": "Set route send level by send index using dB. Valid range is -193.0 to +6.0, and -193.0 is silence floor.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "sendIndex": {
            "type": "integer",
            "minimum": 0
          },
          "db": {
            "type": "number",
            "minimum": -193.0,
            "maximum": 6.0
          }
        },
        "required": [
          "id",
          "sendIndex",
          "db"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "track_add_send",
      "title": "Add Send",
      "description": "Add one internal aux send from a source route to a target route. Optional level and enabled state are applied to the resolved send. For dB mode, valid range is -193.0 to +6.0, and -193.0 is silence floor.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "targetId": {
            "type": "string"
          },
          "position": {
            "type": "number",
            "minimum": 0,
            "maximum": 1
          },
          "db": {
            "type": "number",
            "minimum": -193.0,
            "maximum": 6.0
          },
          "enabled": {
            "type": "boolean"
          },
          "postFader": {
            "type": "boolean"
          }
        },
        "required": [
          "id",
          "targetId"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "created",
          "send",
          "sends"
        ],
        "properties": {
          "created": {
            "type": "boolean"
          },
          "send": {
            "type": "object"
          },
          "sends": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "track_set_send_position",
      "title": "Set Send Position",
      "description": "Set send pre/post-fader position for an existing internal aux send.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "sendIndex": {
            "type": "integer",
            "minimum": 0
          },
          "postFader": {
            "type": "boolean"
          }
        },
        "required": [
          "id",
          "sendIndex",
          "postFader"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "id",
          "routeName",
          "sendIndex",
          "preFader",
          "postFader"
        ],
        "properties": {
          "id": {
            "type": "string"
          },
          "routeName": {
            "type": "string"
          },
          "sendIndex": {
            "type": "integer"
          },
          "preFader": {
            "type": "boolean"
          },
          "postFader": {
            "type": "boolean"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "track_remove_send",
      "title": "Remove Send",
      "description": "Remove one route send by send index. Optional targetId can be used as a safety check for internal sends.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "sendIndex": {
            "type": "integer",
            "minimum": 0
          },
          "targetId": {
            "type": "string"
          }
        },
        "required": [
          "id",
          "sendIndex"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "removed",
          "routeId",
          "routeName",
          "sendIndex",
          "sends"
        ],
        "properties": {
          "removed": {
            "type": "boolean"
          },
          "routeId": {
            "type": "string"
          },
          "routeName": {
            "type": "string"
          },
          "sendIndex": {
            "type": "integer"
          },
          "sends": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "track_set_fader_position",
      "title": "Set Route Fader Position",
      "description": "Set route fader using normalized position (0.0 to 1.0).",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "position": {
            "type": "number",
            "minimum": 0,
            "maximum": 1
          }
        },
        "required": [
          "id",
          "position"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "track_set_fader_db",
      "title": "Set Route Fader dB",
      "description": "Set route fader using dB. Valid range is -193.0 to +6.0, and -193.0 is silence floor.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "id": {
            "type": "string"
          },
          "db": {
            "type": "number",
            "minimum": -193.0,
            "maximum": 6.0
          }
        },
        "required": [
          "id",
          "db"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "midi_region_add_samples",
      "title": "Add MIDI Region By Samples",
      "description": "Create an empty MIDI region on a MIDI track using absolute sample positions.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "trackId": {
            "type": "string"
          },
          "name": {
            "type": "string"
          },
          "startSample": {
            "type": "integer",
            "minimum": 0
          },
          "endSample": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "trackId",
          "startSample",
          "endSample"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "created"
        ],
        "properties": {
          "created": {
            "type": "object"
          },
          "usedDefaultName": {
            "type": "boolean"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "midi_region_add_bbt",
      "title": "Add MIDI Region By Bar Beat",
      "description": "Create an empty MIDI region on a MIDI track using musical positions.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "trackId": {
            "type": "string"
          },
          "name": {
            "type": "string"
          },
          "startBar": {
            "type": "integer",
            "minimum": 1
          },
          "startBeat": {
            "type": "number",
            "minimum": 1
          },
          "endBar": {
            "type": "integer",
            "minimum": 1
          },
          "endBeat": {
            "type": "number",
            "minimum": 1
          }
        },
        "required": [
          "trackId",
          "startBar",
          "startBeat",
          "endBar",
          "endBeat"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "created"
        ],
        "properties": {
          "created": {
            "type": "object"
          },
          "usedDefaultName": {
            "type": "boolean"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "midi_note_add_region_beat",
      "title": "Add MIDI Note By Region Beat",
      "description": "Add one MIDI note to an existing MIDI region using regionBeat relative to the region start.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "note": {
            "type": "integer",
            "minimum": 0,
            "maximum": 127
          },
          "velocity": {
            "type": "integer",
            "minimum": 0,
            "maximum": 127
          },
          "channel": {
            "type": "integer",
            "minimum": 1,
            "maximum": 16
          },
          "lengthBeats": {
            "type": "number",
            "exclusiveMinimum": 0
          },
          "regionBeat": {
            "type": "number",
            "minimum": 0
          }
        },
        "required": [
          "regionId",
          "note",
          "lengthBeats",
          "regionBeat"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "midi_note_add_sample",
      "title": "Add MIDI Note By Sample",
      "description": "Add one MIDI note to an existing MIDI region using an absolute sample position.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "note": {
            "type": "integer",
            "minimum": 0,
            "maximum": 127
          },
          "velocity": {
            "type": "integer",
            "minimum": 0,
            "maximum": 127
          },
          "channel": {
            "type": "integer",
            "minimum": 1,
            "maximum": 16
          },
          "lengthBeats": {
            "type": "number",
            "exclusiveMinimum": 0
          },
          "sample": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "regionId",
          "note",
          "lengthBeats",
          "sample"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "midi_note_add_bbt",
      "title": "Add MIDI Note By Bar Beat",
      "description": "Add one MIDI note to an existing MIDI region using an absolute musical position.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "note": {
            "type": "integer",
            "minimum": 0,
            "maximum": 127
          },
          "velocity": {
            "type": "integer",
            "minimum": 0,
            "maximum": 127
          },
          "channel": {
            "type": "integer",
            "minimum": 1,
            "maximum": 16
          },
          "lengthBeats": {
            "type": "number",
            "exclusiveMinimum": 0
          },
          "bar": {
            "type": "integer",
            "minimum": 1
          },
          "beat": {
            "type": "number",
            "minimum": 1
          }
        },
        "required": [
          "regionId",
          "note",
          "lengthBeats",
          "bar",
          "beat"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "midi_note_list",
      "title": "List MIDI Notes",
      "description": "List all MIDI notes in a MIDI region.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          }
        },
        "required": [
          "regionId"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "midi_note_edit",
      "title": "Edit MIDI Notes",
      "description": "Edit or delete MIDI notes by noteId in a MIDI region.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "edits": {
            "type": "array",
            "minItems": 1,
            "items": {
              "type": "object",
              "properties": {
                "noteId": {
                  "type": "integer"
                },
                "delete": {
                  "type": "boolean"
                },
                "note": {
                  "type": "integer",
                  "minimum": 0,
                  "maximum": 127
                },
                "deltaSemitones": {
                  "type": "integer"
                },
                "velocity": {
                  "type": "integer",
                  "minimum": 0,
                  "maximum": 127
                },
                "deltaVelocity": {
                  "type": "integer"
                },
                "channel": {
                  "type": "integer",
                  "minimum": 1,
                  "maximum": 16
                },
                "startBeats": {
                  "type": "number",
                  "minimum": 0
                },
                "deltaBeats": {
                  "type": "number"
                },
                "lengthBeats": {
                  "type": "number",
                  "exclusiveMinimum": 0
                }
              },
              "required": [
                "noteId"
              ],
              "additionalProperties": false
            }
          }
        },
        "required": [
          "regionId",
          "edits"
        ],
        "additionalProperties": false
      }
    },
    {
      "name": "midi_note_import_json_into_region",
      "title": "Import MIDI JSON Into Existing Region",
      "description": "Import MIDI JSON notes into an existing MIDI region. midi.midi_events is required. Event positions are relative to region start: bar 1 beat 1 = region start, not absolute session time. Each normal item is one note: bar, b, n, optional v, optional l, optional t, optional channel. b may be fractional, for example 1.5 for an off-beat 8th note in 4/4. l is note length in Ardour beats and may be fractional; omit l for 1 beat in melodic mode or 0 beats in drum mode. t is extra tick offset inside the beat using ticks_per_quarter. Repeat items use only bar and repeat; repeat is the source bar number to copy. Channel numbering is 1..16 by default (channel_base=\"one\"). For legacy 0..15 payloads set channel_base=\"zero\". By default is_drum_mode is false and omitted channel defaults to 1. Set is_drum_mode=true for drums; then omitted channel defaults to 10.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "midi": {
            "type": "object",
            "properties": {
              "channel_base": {
                "type": "string",
                "enum": [
                  "one",
                  "zero"
                ]
              },
              "channel": {
                "type": "integer",
                "minimum": 0,
                "maximum": 16
              },
              "is_drum_mode": {
                "type": "boolean"
              },
              "ticks_per_quarter": {
                "type": "integer",
                "minimum": 1,
                "maximum": 96000
              },
              "time_signature": {
                "type": "string"
              },
              "midi_events": {
                "type": "array",
                "minItems": 1,
                "items": {
                  "type": "object",
                  "properties": {
                    "bar": {
                      "type": "integer",
                      "minimum": 1,
                      "description": "Region-relative bar number. Bar 1 starts at the beginning of the region."
                    },
                    "repeat": {
                      "type": "integer",
                      "minimum": 1,
                      "description": "Optional source bar number to copy into this bar. Repeat items should only include bar and repeat."
                    },
                    "b": {
                      "type": "number",
                      "minimum": 1,
                      "description": "Beat number inside the bar. Decimal values are allowed; 1.5 is the off-beat eighth note after beat 1 in 4/4."
                    },
                    "t": {
                      "type": "integer",
                      "minimum": 0,
                      "description": "Optional additional tick offset inside the beat using ticks_per_quarter."
                    },
                    "n": {
                      "type": "integer",
                      "minimum": 0,
                      "maximum": 127,
                      "description": "MIDI note number."
                    },
                    "v": {
                      "type": "integer",
                      "minimum": 0,
                      "maximum": 127,
                      "description": "Optional velocity. Defaults to 64."
                    },
                    "l": {
                      "type": "number",
                      "minimum": 0,
                      "description": "Optional note length in Ardour beats. Decimal values are allowed; 0.5 is an eighth note in 4/4. Defaults to 1.0 for melodic MIDI and 0.0 for drum mode."
                    },
                    "channel": {
                      "type": "integer",
                      "minimum": 0,
                      "maximum": 16,
                      "description": "Optional per-note channel using midi.channel_base."
                    }
                  },
                  "required": [
                    "bar"
                  ],
                  "additionalProperties": false
                }
              }
            },
            "required": [
              "midi_events"
            ],
            "additionalProperties": false
          },
          "regionId": {
            "type": "string"
          },
          "name": {
            "type": "string"
          }
        },
        "required": [
          "midi",
          "regionId"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "createdRegion",
          "region",
          "summary"
        ],
        "properties": {
          "createdRegion": {
            "type": "boolean"
          },
          "region": {
            "type": "object"
          },
          "summary": {
            "type": "object"
          },
          "warnings": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "midi_note_import_json_to_new_region_samples",
      "title": "Import MIDI JSON To New Region By Samples",
      "description": "Create and populate a new MIDI region from MIDI JSON using absolute sample positions. midi.midi_events is required. Event positions are relative to the new region start: bar 1 beat 1 = region start, not absolute session time. Each normal item is one note: bar, b, n, optional v, optional l, optional t, optional channel. b may be fractional, for example 1.5 for an off-beat 8th note in 4/4. l is note length in Ardour beats and may be fractional; omit l for 1 beat in melodic mode or 0 beats in drum mode. t is extra tick offset inside the beat using ticks_per_quarter. Repeat items use only bar and repeat; repeat is the source bar number to copy. Channel numbering is 1..16 by default (channel_base=\"one\"). For legacy 0..15 payloads set channel_base=\"zero\". By default is_drum_mode is false and omitted channel defaults to 1. Set is_drum_mode=true for drums; then omitted channel defaults to 10.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "midi": {
            "type": "object",
            "properties": {
              "channel_base": {
                "type": "string",
                "enum": [
                  "one",
                  "zero"
                ]
              },
              "channel": {
                "type": "integer",
                "minimum": 0,
                "maximum": 16
              },
              "is_drum_mode": {
                "type": "boolean"
              },
              "ticks_per_quarter": {
                "type": "integer",
                "minimum": 1,
                "maximum": 96000
              },
              "time_signature": {
                "type": "string"
              },
              "midi_events": {
                "type": "array",
                "minItems": 1,
                "items": {
                  "type": "object",
                  "properties": {
                    "bar": {
                      "type": "integer",
                      "minimum": 1,
                      "description": "Region-relative bar number. Bar 1 starts at the beginning of the region."
                    },
                    "repeat": {
                      "type": "integer",
                      "minimum": 1,
                      "description": "Optional source bar number to copy into this bar. Repeat items should only include bar and repeat."
                    },
                    "b": {
                      "type": "number",
                      "minimum": 1,
                      "description": "Beat number inside the bar. Decimal values are allowed; 1.5 is the off-beat eighth note after beat 1 in 4/4."
                    },
                    "t": {
                      "type": "integer",
                      "minimum": 0,
                      "description": "Optional additional tick offset inside the beat using ticks_per_quarter."
                    },
                    "n": {
                      "type": "integer",
                      "minimum": 0,
                      "maximum": 127,
                      "description": "MIDI note number."
                    },
                    "v": {
                      "type": "integer",
                      "minimum": 0,
                      "maximum": 127,
                      "description": "Optional velocity. Defaults to 64."
                    },
                    "l": {
                      "type": "number",
                      "minimum": 0,
                      "description": "Optional note length in Ardour beats. Decimal values are allowed; 0.5 is an eighth note in 4/4. Defaults to 1.0 for melodic MIDI and 0.0 for drum mode."
                    },
                    "channel": {
                      "type": "integer",
                      "minimum": 0,
                      "maximum": 16,
                      "description": "Optional per-note channel using midi.channel_base."
                    }
                  },
                  "required": [
                    "bar"
                  ],
                  "additionalProperties": false
                }
              }
            },
            "required": [
              "midi_events"
            ],
            "additionalProperties": false
          },
          "trackId": {
            "type": "string"
          },
          "name": {
            "type": "string"
          },
          "startSample": {
            "type": "integer",
            "minimum": 0
          },
          "endSample": {
            "type": "integer",
            "minimum": 0
          }
        },
        "required": [
          "midi",
          "trackId",
          "startSample",
          "endSample"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "createdRegion",
          "region",
          "summary"
        ],
        "properties": {
          "createdRegion": {
            "type": "boolean"
          },
          "region": {
            "type": "object"
          },
          "summary": {
            "type": "object"
          },
          "warnings": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "midi_note_import_json_to_new_region_bbt",
      "title": "Import MIDI JSON To New Region By Bar Beat",
      "description": "Create and populate a new MIDI region from MIDI JSON using musical positions. midi.midi_events is required. Event positions are relative to the new region start: bar 1 beat 1 = region start, not absolute session time. Each normal item is one note: bar, b, n, optional v, optional l, optional t, optional channel. b may be fractional, for example 1.5 for an off-beat 8th note in 4/4. l is note length in Ardour beats and may be fractional; omit l for 1 beat in melodic mode or 0 beats in drum mode. t is extra tick offset inside the beat using ticks_per_quarter. Repeat items use only bar and repeat; repeat is the source bar number to copy. Channel numbering is 1..16 by default (channel_base=\"one\"). For legacy 0..15 payloads set channel_base=\"zero\". By default is_drum_mode is false and omitted channel defaults to 1. Set is_drum_mode=true for drums; then omitted channel defaults to 10.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "midi": {
            "type": "object",
            "properties": {
              "channel_base": {
                "type": "string",
                "enum": [
                  "one",
                  "zero"
                ]
              },
              "channel": {
                "type": "integer",
                "minimum": 0,
                "maximum": 16
              },
              "is_drum_mode": {
                "type": "boolean"
              },
              "ticks_per_quarter": {
                "type": "integer",
                "minimum": 1,
                "maximum": 96000
              },
              "time_signature": {
                "type": "string"
              },
              "midi_events": {
                "type": "array",
                "minItems": 1,
                "items": {
                  "type": "object",
                  "properties": {
                    "bar": {
                      "type": "integer",
                      "minimum": 1,
                      "description": "Region-relative bar number. Bar 1 starts at the beginning of the region."
                    },
                    "repeat": {
                      "type": "integer",
                      "minimum": 1,
                      "description": "Optional source bar number to copy into this bar. Repeat items should only include bar and repeat."
                    },
                    "b": {
                      "type": "number",
                      "minimum": 1,
                      "description": "Beat number inside the bar. Decimal values are allowed; 1.5 is the off-beat eighth note after beat 1 in 4/4."
                    },
                    "t": {
                      "type": "integer",
                      "minimum": 0,
                      "description": "Optional additional tick offset inside the beat using ticks_per_quarter."
                    },
                    "n": {
                      "type": "integer",
                      "minimum": 0,
                      "maximum": 127,
                      "description": "MIDI note number."
                    },
                    "v": {
                      "type": "integer",
                      "minimum": 0,
                      "maximum": 127,
                      "description": "Optional velocity. Defaults to 64."
                    },
                    "l": {
                      "type": "number",
                      "minimum": 0,
                      "description": "Optional note length in Ardour beats. Decimal values are allowed; 0.5 is an eighth note in 4/4. Defaults to 1.0 for melodic MIDI and 0.0 for drum mode."
                    },
                    "channel": {
                      "type": "integer",
                      "minimum": 0,
                      "maximum": 16,
                      "description": "Optional per-note channel using midi.channel_base."
                    }
                  },
                  "required": [
                    "bar"
                  ],
                  "additionalProperties": false
                }
              }
            },
            "required": [
              "midi_events"
            ],
            "additionalProperties": false
          },
          "trackId": {
            "type": "string"
          },
          "name": {
            "type": "string"
          },
          "startBar": {
            "type": "integer",
            "minimum": 1
          },
          "startBeat": {
            "type": "number",
            "minimum": 1
          },
          "endBar": {
            "type": "integer",
            "minimum": 1
          },
          "endBeat": {
            "type": "number",
            "minimum": 1
          }
        },
        "required": [
          "midi",
          "trackId",
          "startBar",
          "startBeat",
          "endBar",
          "endBeat"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "createdRegion",
          "region",
          "summary"
        ],
        "properties": {
          "createdRegion": {
            "type": "boolean"
          },
          "region": {
            "type": "object"
          },
          "summary": {
            "type": "object"
          },
          "warnings": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    },
    {
      "name": "midi_note_get_json",
      "title": "Export MIDI JSON",
      "description": "Export all notes from a MIDI region as import-compatible MIDI JSON using one note item per note. Each item includes bar, b, t, n, v, and l. Export uses channel_base=\"one\" and channels 1..16.",
      "inputSchema": {
        "type": "object",
        "properties": {
          "regionId": {
            "type": "string"
          },
          "ticksPerQuarter": {
            "type": "integer",
            "minimum": 1
          },
          "timeSignature": {
            "type": "string"
          }
        },
        "required": [
          "regionId"
        ],
        "additionalProperties": false
      },
      "outputSchema": {
        "type": "object",
        "required": [
          "region",
          "midi"
        ],
        "properties": {
          "region": {
            "type": "object"
          },
          "midi": {
            "type": "object",
            "required": [
              "channel_base",
              "channel",
              "is_drum_mode",
              "time_signature",
              "ticks_per_quarter",
              "midi_events"
            ],
            "properties": {
              "channel_base": {
                "type": "string",
                "enum": [
                  "one"
                ]
              },
              "channel": {
                "type": "integer"
              },
              "is_drum_mode": {
                "type": "boolean"
              },
              "time_signature": {
                "type": "string"
              },
              "ticks_per_quarter": {
                "type": "integer"
              },
              "midi_events": {
                "type": "array"
              }
            },
            "additionalProperties": true
          },
          "warnings": {
            "type": "array"
          }
        },
        "additionalProperties": true
      }
    }
  ]
}
)mcp"
