#!/usr/bin/env bash
#
# Import a (multi-track) MIDI file into a fresh Ardour session, splitting it
# into one track per channel, launch the Ardour GUI on it under PipeWire/JACK,
# connect the master output to the laptop speakers, and enable Ardour's MCP
# server so an AI assistant can drive the session live.
#
# Usage:  ./import_and_play.sh [file.mid] [session-dir]
#
# Env options:
#   SF2=<path>          General-MIDI soundfont (default FluidR3_GM.sf2)
#   SPLIT_AT_MARKERS=0  keep each track as one region (by default every track
#                       is cut into one region per section marker, named after
#                       it: Part A, Part B, ...)
#   MCP=0               do not enable the MCP HTTP server (enabled by default)
#   MCP_PORT=<port>     MCP server port (default 4820)
#
# Phase 1 runs headless (ardour-lua, dummy backend) with an ISOLATED config so
# it cannot change your real Ardour audio settings. Phase 2 launches the GUI
# with your normal config (JACK via pw-jack). Phase 3 wires master -> speakers.
set -eo pipefail   # not -u: ardev_common_waf.sh references unset vars

ARDOUR_TREE="$(cd "$(dirname "$0")" && pwd)"   # repo root = where this script lives
MIDI="${1:-$HOME/perso/musique/Morceaux/rond_saint-vincent/anneaux_d_or/all/anneaux_d_or.mid}"
NAME="$(basename "$MIDI" .mid)"
SESSION_DIR="${2:-$HOME/ardour-sessions/$NAME}"
RATE=48000
SPEAKERS="Built-in Audio Analog Stereo"      # the default PipeWire sink (laptop speakers)
# General-MIDI soundfont for a-fluidsynth. Swap freely, e.g.:
#   FluidR3_GM.sf2            (148 MB, classic GM)
#   MuseScore_General_Full.sf2 (489 MB, higher quality)
#   TimGM6mb.sf2             (6 MB, fast to load)
SF2="${SF2:-/usr/share/sounds/sf2/FluidR3_GM.sf2}"
# Split each track into one region per section marker? (default yes)
case "${SPLIT_AT_MARKERS:-1}" in 0|false|no|off) SPLIT_AT_MARKERS=false;; *) SPLIT_AT_MARKERS=true;; esac
# Enable the MCP HTTP control surface in the session? (default yes)
case "${MCP:-1}" in 0|false|no|off) MCP=false;; *) MCP=true;; esac
MCP_PORT="${MCP_PORT:-4820}"
MCP_SURFACE="MCP HTTP Server (Experimental)"   # the surface's protocol name

[ -f "$MIDI" ] || { echo "No such MIDI file: $MIDI" >&2; exit 1; }

# ---------------------------------------------------------------------------
# Phase 1 — headless create + import + split  (isolated config)
# ---------------------------------------------------------------------------
echo ">>> Phase 1: importing & splitting $(basename "$MIDI") ..."
rm -rf "$SESSION_DIR"
mkdir -p "$(dirname "$SESSION_DIR")"

LUA="$(mktemp "${TMPDIR:-/tmp}/import_XXXXXX.lua")"
cat > "$LUA" <<EOF
local s = create_session("$SESSION_DIR", "$NAME", $RATE)
assert(s, "session creation failed")
-- import_midi(session, path, with_tempo_map, with_markers, split_channels, split_at_markers)
local tr = ARDOUR.LuaAPI.import_midi(s, "$MIDI", true, true, true, $SPLIT_AT_MARKERS)
print(string.format("IMPORTED %d MIDI track(s)", #tr:table()))

-- Put ACE a-fluidsynth at the top of each track so MIDI -> GM audio. The
-- soundfont can't be set from headless Lua (a-fluidsynth loads it in its
-- process callback, which doesn't run here), so we only INSTANTIATE it here;
-- the soundfont is injected into the saved plugin state in Phase 1b below.
local SYNTH = "urn:ardour:a-fluidsynth"
local nsynth = 0
for t in tr:iter() do
    local synth = ARDOUR.LuaAPI.new_plugin(s, SYNTH, ARDOUR.PluginType.LV2, "")
    if synth and 0 == t:add_processor_by_index(synth, 0, nil, true) then
        nsynth = nsynth + 1
    else
        print("WARN: could not add synth to " .. t:name())
    end
end
print(string.format("SYNTH added to %d/%d track(s)", nsynth, #tr:table()))

s:save_state("")
tr = nil; s = nil; collectgarbage("collect")
close_session()
EOF

ISOCFG="$(mktemp -d "${TMPDIR:-/tmp}/ardcfg_XXXXXX")"
(
  cd "$ARDOUR_TREE"
  export TOP="$PWD"
  . build/gtk2_ardour/ardev_common_waf.sh
  # ardev_common_waf.sh does NOT set LV2_PATH; a-fluidsynth lives in /opt (and
  # /usr/lib/lv2). If LV2_PATH is set lilv searches ONLY these, so be explicit.
  XDG_CONFIG_HOME="$ISOCFG" \
  LV2_PATH="/opt/Ardour-9.2.0/lib/LV2:$PWD/build/libs/LV2" \
      build/luasession/luasession "$LUA"
)
rm -f "$LUA"; rm -rf "$ISOCFG"
echo ">>> Session ready: $SESSION_DIR/$NAME.ardour"

# ---------------------------------------------------------------------------
# Phase 1b — bake the soundfont into each a-fluidsynth's saved LV2 state.
# Headless luasession never runs the plugin's process callback, so load_preset
# queues but never persists the .sf2 path. We inject it into state.ttl (and a
# symlink) so the GUI restores the soundfont on load. Same format Ardour writes.
# ---------------------------------------------------------------------------
for st in "$SESSION_DIR"/plugins/*/state*/state.ttl; do
  [ -f "$st" ] || continue
  grep -q "urn:ardour:a-fluidsynth" "$st" || continue
  grep -q "sf2file" "$st" && continue            # already has a soundfont
  d="$(dirname "$st")"
  ln -sf "$SF2" "$d/$(basename "$SF2")"
  python3 - "$st" "$(basename "$SF2")" <<'PY'
import sys
path, sf = sys.argv[1], sys.argv[2]
txt = open(path).read()
old = "\tlv2:appliesTo <urn:ardour:a-fluidsynth> ."
new = ("\tlv2:appliesTo <urn:ardour:a-fluidsynth> ;\n"
       "\tstate:state [\n"
       "\t\t<urn:ardour:a-fluidsynth:sf2file> <%s>\n"
       "\t] ." % sf)
open(path, "w").write(txt.replace(old, new))
PY
  echo ">>> soundfont baked into $(basename "$(dirname "$d")")"
done

# ---------------------------------------------------------------------------
# Phase 1c — activate the MCP server in the session's control-protocol state,
# which Ardour applies when it loads the session. An entry without a "config"
# attribute is ignored, hence the explicit empty one.
# ---------------------------------------------------------------------------
if $MCP; then
  python3 - "$SESSION_DIR/$NAME.ardour" "$MCP_SURFACE" "$MCP_PORT" <<'PY'
import sys, xml.etree.ElementTree as ET
path, surface, port = sys.argv[1:]
tree = ET.parse(path)
cps = tree.getroot().find("ControlProtocols")
if cps is None:
    cps = ET.SubElement(tree.getroot(), "ControlProtocols")
for p in cps.findall("Protocol"):
    if p.get("name") == surface:
        cps.remove(p)
ET.SubElement(cps, "Protocol", {"name": surface, "active": "1", "config": "",
                                "feedback": "0", "port": port, "debug-level": "0"})
tree.write(path, encoding="UTF-8", xml_declaration=True)
PY
  echo ">>> MCP server enabled on port $MCP_PORT"
fi

# ---------------------------------------------------------------------------
# Phase 2 — launch the GUI under PipeWire/JACK (detached, survives this script)
# ---------------------------------------------------------------------------
echo ">>> Phase 2: launching Ardour GUI ..."
GUI_LOG="${TMPDIR:-/tmp}/ardour_${NAME}.log"
(
  cd "$ARDOUR_TREE"
  setsid pw-jack ./gtk2_ardour/ardev-opt "$SESSION_DIR/$NAME.ardour" \
      >"$GUI_LOG" 2>&1 < /dev/null &
)
echo ">>> GUI log: $GUI_LOG"

# ---------------------------------------------------------------------------
# Phase 3 — wait for Ardour's JACK ports, connect master -> speakers
# ---------------------------------------------------------------------------
echo ">>> Phase 3: connecting master -> \"$SPEAKERS\" ..."
for _ in $(seq 1 90); do
  if pw-jack jack_lsp 2>/dev/null | grep -q "Master/audio_out 1"; then break; fi
  sleep 1
done

mapfile -t MOUT < <(pw-jack jack_lsp 2>/dev/null | grep "Master/audio_out" | sort)
if [ "${#MOUT[@]}" -ge 2 ]; then
  pw-jack jack_connect "${MOUT[0]}" "$SPEAKERS:playback_FL" 2>/dev/null || true
  pw-jack jack_connect "${MOUT[1]}" "$SPEAKERS:playback_FR" 2>/dev/null || true
  echo ">>> Connected:"
  echo "      ${MOUT[0]}  ->  $SPEAKERS:playback_FL"
  echo "      ${MOUT[1]}  ->  $SPEAKERS:playback_FR"
else
  echo "!!! Could not find Ardour master output ports (is the GUI up?)." >&2
fi

if $MCP; then
  MCP_URL="http://127.0.0.1:$MCP_PORT/mcp"
  MCP_UP=false
  for _ in $(seq 1 30); do
    if curl -s -m 2 -X POST -H 'Content-Type: application/json' \
         -d '{"jsonrpc":"2.0","id":1,"method":"ping"}' "$MCP_URL" | grep -q result; then
      MCP_UP=true; break
    fi
    sleep 1
  done
  if $MCP_UP; then
    echo ">>> MCP server answering at $MCP_URL"
    echo "      register it once with: claude mcp add --transport http ardour $MCP_URL"
  else
    echo "!!! MCP server not answering at $MCP_URL (see the GUI log)." >&2
  fi
fi
echo ">>> Done. Ardour is running with the session open."
