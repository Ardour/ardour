# Ardour MCP HTTP Surface (Experimental)

This control surface exposes Ardour actions through an MCP server over HTTP.

The goal is practical AI-assisted workflow control inside Ardour:
- session and transport actions
- track/bus creation and control
- routing and sends
- plugin discovery and manipulation
- marker/range management
- region and MIDI editing

This is an experimental surface intended for early real-world testing and feedback.

## Quick Start

1. Build Ardour with the `mcp_http` surface enabled.
2. Start Ardour and open:
   `Edit > Preferences > Control Surfaces`
3. Enable `MCPHttp`.
4. Open the MCPHttp settings dialog and confirm:
   - listen port (default is `4820`)
   - debug level (optional)
5. Use endpoint:
   `http://127.0.0.1:<port>/mcp`

The server applies port changes immediately after committing the setting.

## Transport and Naming

- Transport is HTTP (streamable MCP endpoint), not SSE.
- MCP tool names are canonical slash form, for example:
  - `tracks/list`
  - `transport/loop_toggle`
  - `midi_note/import_json`
- Compatibility aliases are accepted for many clients:
  - underscore form: `tracks_list`
  - dotted form: `tracks.list`

## Method Overview

## Session
- `session/get_info`
- `session/save`
- `session/undo`
- `session/redo`
- `session/rename`
- `session/quick_snapshot`
- `session/store_mixer_scene`
- `session/recall_mixer_scene`

## Transport
- `transport/get_state`
- `transport/locate`
- `transport/goto_start`
- `transport/goto_end`
- `transport/prev_marker`
- `transport/next_marker`
- `transport/loop_toggle`
- `transport/loop_location`
- `transport/set_record_enable`
- `transport/set_speed`
- `transport/play`
- `transport/stop`

## Tracks and Buses
- `tracks/list`
- `tracks/add`
- `buses/add`
- `track/select`
- `track/rename`
- `track/get_info`
- `track/get_fader`
- `track/set_fader`
- `track/set_pan`
- `track/set_mute`
- `track/set_solo`
- `track/set_rec_enable`
- `track/set_rec_safe`

## Sends and Routing
- `track/add_send`
- `track/set_send_level`
- `track/set_send_position`
- `track/remove_send`

## Plugins
- `plugin/list_available`
- `plugin/add`
- `plugin/remove`
- `plugin/reorder`
- `plugin/set_position`
- `plugin/get_description`
- `plugin/set_enabled`
- `plugin/set_parameter`

## Markers and Ranges
- `markers/list`
- `markers/add`
- `markers/add_range`
- `markers/delete`
- `markers/rename`
- `markers/set_auto_loop`
- `markers/hide_auto_loop`
- `markers/set_auto_punch`
- `markers/hide_auto_punch`

## Regions (Audio and MIDI)
- `track/get_regions`
- `region/get_info`
- `region/move`
- `region/copy`
- `region/resize`
- `region/split`
- `region/set_gain`
- `region/normalize`

## MIDI Regions and Notes
- `midi_region/add`
- `midi_note/add`
- `midi_note/list`
- `midi_note/edit`
- `midi_note/get_json`
- `midi_note/import_json`

## Bulk MIDI JSON Notes

Use the JSON tools for fast note generation and round-trip editing:

- `midi_note/import_json`: bulk insert or layer many notes in one call.
- `midi_note/get_json`: export region notes in the same JSON shape for reuse or editing.
- Drum mode (`is_drum_mode: true`) imports per-hit notes as zero-length events (note-on and note-off at the same timestamp).
- Non-drum mode uses normal note durations from on/off pairs.

## Prompt Cookbook

Use these with your MCP-capable assistant to get started quickly.

## Session and Transport
- "Show transport state and current marker list."
- "Set loop from bar 32 for 8 bars and enable loop."
- "Go to marker Verse 2 and start playback."
- "Arm global record, then go to start."

## Track Setup
- "Create tracks for a live band: lead vocal, two backing vocals, two guitars, bass, stereo drums, stereo keys."
- "Pan backing vocals left/right and center the lead vocal."
- "Create a bus called Vocal verb and route sends from lead and backing vocals to it."

## Plugin Tasks
- "List available EQ plugins and add one to Rose."
- "Disable the compressor plugin on Rose."
- "Move EQ to post-fader on Rose."
- "Set send level from Frank to Vocal verb to -12 dB."

## Marker and Arrangement Tasks
- "Add marker Frank at bar 7 beat 3.5."
- "Rename marker Frank to Verse Lead-In."
- "Add arrangement section Solo 2 at bar 40."
- "Set punch range from bar 34 for 4 bars and hide it."

## Region and MIDI Tasks
- "Create a MIDI region on MCP MIDI Test from bar 3 for 2 bars."
- "Insert a C major up/down scale using 8th notes with velocity around 80."
- "Add 4 bars of steady drum groove in the existing drum MIDI region."
- "In bar 4, add solid quarter-note chords in the key of F on each beat."
- "Transpose dot-prefixed chord markers from G to A."
- "Move region Test Part A by 2.5 beats later on the same track."
- "Copy this region four times every two bars."

## Client Configuration

Client config formats change over time. Use these as quick examples and adjust to your CLI version.

## Codex CLI

Add from terminal:

```bash
codex mcp add ardour_http --url http://127.0.0.1:4820/mcp
```

File example (equivalent): `~/.codex/config.toml`

```toml
[mcp_servers.ardour_http]
url = "http://127.0.0.1:4820/mcp"
```

## Gemini CLI

Add from terminal:

```bash
gemini mcp add ardour-http http://127.0.0.1:4820/mcp --type http
```

Project file example (equivalent):

```json
{
  "mcpServers": {
    "ardour-http": {
      "url": "http://127.0.0.1:4820/mcp",
      "type": "http"
    }
  }
}
```

## Claude Desktop

In Claude Desktop, open `Settings > Developer` and add:

```json
{
  "mcpServers": {
    "ardour-http": {
      "command": "npx",
      "args": [
        "mcp-remote@latest",
        "http://127.0.0.1:4820/mcp"
      ]
    }
  }
}
```

## Copilot CLI

Copilot CLI currently uses an interactive flow.

In Copilot CLI:

1. Run:

```text
/mcp add
```

2. Fill the form with:

- name: `ardour-http`
- URL: `http://127.0.0.1:4820/mcp`
- HTTP transport (not SSE)

## Notes for Client Compatibility

- Return payloads include both:
  - `structuredContent` for clients that consume structured MCP output
  - JSON-serialized `content[].text` fallback for text-only clients
- This is intentional for broader interoperability across CLI clients.

## Known Limitations

- Tooling is currently ID-based for most edit operations.
- Selected-region targeting is not implemented yet.
- This surface is experimental and will evolve based on user feedback.

## Safety

- For destructive edits (delete, move, normalize, overwrite), test first in duplicate sessions or snapshots.
- Use `session/save` and/or `session/quick_snapshot` before large automated edits.
