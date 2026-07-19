# Ardour-MCP Integration Guide

## Overview

The **ardour-mcp** project provides a Model Context Protocol (MCP) server that enables AI assistants (such as Claude) to control and interact with Ardour through its Open Sound Control (OSC) interface. This creates a powerful workflow where AI can help with audio production tasks, automation, mixing, and session management.

## What is MCP?

Model Context Protocol (MCP) is an open protocol that allows AI assistants to safely interact with external systems through well-defined interfaces. The ardour-mcp server acts as a bridge between Claude (or other MCP-compatible AI assistants) and Ardour's OSC control surface.

## Why Use Ardour with MCP?

### Benefits

1. **AI-Assisted Mixing**: Get AI help with EQ suggestions, compression settings, and mix balance
2. **Automated Workflows**: Automate repetitive tasks like track setup, routing, and plugin configuration
3. **Natural Language Control**: Control Ardour using conversational commands instead of clicking through menus
4. **Session Analysis**: Get insights about your session structure, track counts, routing complexity
5. **Learning Assistant**: Learn Ardour features through interactive guidance
6. **Scripting Help**: Generate Lua scripts for custom automation
7. **Documentation Access**: Quick access to Ardour documentation and best practices

### Use Cases

- **Quick Mix Setup**: "Set up a standard rock band mix with drums, bass, guitars, and vocals"
- **Automation Tasks**: "Create a volume fade out on the master bus from bar 64 to bar 68"
- **Plugin Management**: "Add a compressor to all drum tracks with conservative settings"
- **Session Organization**: "Show me all tracks that have no input connections"
- **Problem Solving**: "Why am I getting feedback in my monitoring setup?"
- **Export Automation**: "Export all tracks as individual stems with specific naming"

## Architecture

```
┌─────────────────┐
│  Claude Code /  │
│   AI Assistant  │
└────────┬────────┘
         │ MCP Protocol
         │
┌────────▼────────┐
│  ardour-mcp     │
│  MCP Server     │
└────────┬────────┘
         │ OSC Protocol (UDP)
         │ Port 3819
┌────────▼────────┐
│     Ardour      │
│  OSC Surface    │
└─────────────────┘
```

## Prerequisites

### System Requirements

- Ardour 8.x or later (built from source recommended for development)
- Python 3.8+ (for ardour-mcp server)
- Network connectivity (localhost for typical setup)
- Just command runner (optional, but recommended)

### Installing Dependencies

```bash
# Install just (command runner)
curl --proto '=https' --tlsv1.2 -sSf https://just.systems/install.sh | bash

# Or via package manager
# Ubuntu/Debian: cargo install just
# macOS: brew install just
# Arch: pacman -S just
```

## Setup Instructions

### Step 1: Build Ardour with OSC Support

Ardour's OSC support is typically enabled by default, but you can explicitly configure it:

```bash
# Navigate to Ardour source directory
cd /path/to/ardour

# Configure with OSC support
just configure-osc

# Build Ardour
just build

# Verify OSC library was built
just check-osc
```

### Step 2: Enable OSC in Ardour

1. **Start Ardour**:
   ```bash
   just run-osc-debug
   ```

2. **Enable OSC Control Surface**:
   - Go to `Edit > Preferences > Control Surfaces`
   - Check the box next to `Open Sound Control (OSC)`
   - Click `Show Protocol Settings`

3. **Configure OSC Settings**:
   - **Port**: 3819 (default, recommended)
   - **Protocol**: UDP
   - **Reply Mode**: Enable if you want feedback from Ardour
   - **Bank Size**: Set according to your needs (typically 0 for unlimited)

4. **Advanced Settings** (optional):
   - **Debug Mode**: Enable for troubleshooting
   - **Default Strip Types**: Configure which track types to expose
   - **Feedback**: Configure which parameters send updates

5. **Click OK** to save settings

### Step 3: Set Up ardour-mcp Server

```bash
# Navigate to ardour-mcp directory (sibling to ardour)
cd /path/to/ardour-mcp

# Install dependencies (if using Python)
pip install -r requirements.txt

# Or if using Node.js
npm install

# Configure MCP server (check ardour-mcp documentation)
# Typically involves setting OSC host and port

# Start the MCP server
# (refer to ardour-mcp specific instructions)
```

### Step 4: Connect Claude Code to MCP

1. **Configure MCP in Claude Code**:
   - Create or edit `~/.config/claude-code/mcp_settings.json`
   - Add ardour-mcp server configuration

2. **Example MCP Configuration**:
   ```json
   {
     "mcpServers": {
       "ardour": {
         "command": "python",
         "args": ["/path/to/ardour-mcp/server.py"],
         "env": {
           "ARDOUR_OSC_HOST": "127.0.0.1",
           "ARDOUR_OSC_PORT": "3819"
         }
       }
     }
   }
   ```

3. **Restart Claude Code** to load the MCP server

### Step 5: Test the Connection

1. **Verify OSC Port**:
   ```bash
   just test-osc-connection
   ```

2. **Test from Claude**:
   - Open Claude Code
   - Try a simple command: "What tracks are in my current Ardour session?"
   - Claude should use the MCP server to query Ardour via OSC

## OSC Protocol Reference

### Common OSC Messages

Ardour's OSC implementation supports hundreds of commands. Here are some commonly used ones:

#### Transport Control
```
/transport_play          - Start playback
/transport_stop          - Stop playback
/rewind                  - Rewind to start
/ffwd                    - Fast forward to end
/goto_start              - Jump to session start
/goto_end                - Jump to session end
```

#### Track/Strip Control
```
/strip/gain <ssid> <value>              - Set track gain
/strip/pan_stereo_position <ssid> <pos> - Set pan position
/strip/mute <ssid> <0|1>                - Mute/unmute track
/strip/solo <ssid> <0|1>                - Solo/unsolo track
/strip/recenable <ssid> <0|1>           - Enable/disable recording
```

#### Session Information
```
/session/list            - List all tracks
/strip/list              - Get strip list with details
/session/frame_rate      - Get sample rate
/session/timecode        - Get current timecode
```

#### Plugin Control
```
/strip/plugin/parameter <ssid> <plugin> <param> <value>
```

### OSC Query and Feedback

Ardour can send feedback about parameter changes:

```
/strip/gains             - Subscribe to gain changes for all strips
/strip/fader/<ssid>      - Feedback when fader moves
```

## Development Workflow

### Quick Start Development Cycle

```bash
# Terminal 1: Build and run Ardour with OSC debugging
cd /path/to/ardour
just osc-dev

# Terminal 2: Run ardour-mcp server
cd /path/to/ardour-mcp
# (start your MCP server)

# Terminal 3: Use Claude Code
claude-code
```

### Monitoring OSC Traffic

1. **Enable OSC Debug in Ardour**:
   ```bash
   ARDOUR_DEBUG=OSC just run
   ```

2. **Watch Ardour Logs**:
   ```bash
   just watch-logs
   ```

3. **Use OSC Monitor Tools**:
   - `oscdump` - Command-line OSC monitor
   - `OSCulator` (macOS) - GUI OSC monitor and router
   - `protokol` - Cross-platform OSC/MIDI monitor

### Testing OSC Commands Manually

You can test OSC commands using various tools:

```bash
# Using oscsend (from liblo-tools)
oscsend localhost 3819 /transport_play

# Using Python with python-osc
python3 << EOF
from pythonosc import udp_client
client = udp_client.SimpleUDPClient("127.0.0.1", 3819)
client.send_message("/transport_play", [])
EOF
```

## Troubleshooting

### OSC Not Responding

1. **Check OSC is enabled**:
   - Edit > Preferences > Control Surfaces
   - Verify "Open Sound Control (OSC)" is checked

2. **Verify port is not in use**:
   ```bash
   netstat -an | grep 3819
   # or
   lsof -i :3819
   ```

3. **Check firewall settings**:
   ```bash
   # Allow UDP port 3819 on localhost
   sudo ufw allow 3819/udp
   ```

4. **Test with debug output**:
   ```bash
   ARDOUR_DEBUG=OSC just run
   ```

### MCP Server Connection Issues

1. **Verify MCP server is running**:
   ```bash
   ps aux | grep ardour-mcp
   ```

2. **Check MCP server logs** (location depends on ardour-mcp implementation)

3. **Verify Claude Code MCP configuration**:
   ```bash
   cat ~/.config/claude-code/mcp_settings.json
   ```

4. **Test MCP server independently** (without Claude)

### Performance Issues

1. **Reduce OSC feedback**:
   - Disable unnecessary feedback in OSC settings
   - Use targeted queries instead of broad subscriptions

2. **Adjust OSC update rates** in Ardour preferences

3. **Use local connections only** (127.0.0.1, not 0.0.0.0)

## Security Considerations

### Network Exposure

- **Default configuration**: OSC listens on localhost only (127.0.0.1)
- **Remote access**: Be cautious enabling remote OSC access
- **Firewall**: Keep port 3819 blocked from external networks
- **No authentication**: OSC has no built-in authentication

### Best Practices

1. **Never expose OSC to untrusted networks**
2. **Use SSH tunneling** for remote access if needed
3. **Run ardour-mcp and Ardour on the same machine**
4. **Keep ardour-mcp updated** for security patches
5. **Monitor OSC traffic** in production environments

## Advanced Topics

### Custom OSC Scripts

You can create custom OSC interaction scripts:

```python
# Example: Automated mixing script
from pythonosc import udp_client

client = udp_client.SimpleUDPClient("127.0.0.1", 3819)

# Set consistent levels for drum bus
drum_tracks = [1, 2, 3, 4]  # Kick, snare, toms, OH
for track in drum_tracks:
    client.send_message(f"/strip/gain", [track, -6.0])
    client.send_message(f"/strip/solo", [track, 0])
```

### Integrating with Lua Scripts

Ardour's Lua scripting can complement OSC control:

- Use OSC for external control and monitoring
- Use Lua for complex internal automation
- Combine both for powerful workflows

### Multiple OSC Clients

Ardour can handle multiple OSC clients simultaneously:

- Different control surfaces
- Monitoring tools
- Multiple MCP servers
- Mix and match control paradigms

## Resources

### Documentation

- [Ardour OSC Manual](https://manual.ardour.org/using-control-surfaces/controlling-ardour-with-osc/)
- [OSC Protocol Specification](https://opensoundcontrol.stanford.edu/)
- [Ardour Development Guide](https://ardour.org/development.html)

### Community

- Ardour Forums: https://discourse.ardour.org/
- Ardour IRC: #ardour on libera.chat
- GitHub Issues: https://github.com/Ardour/ardour

### Example Projects

- TouchOSC templates for Ardour
- Open Stage Control layouts
- Ardour OSC Python examples in `share/scripts/`

## Contributing

If you develop useful MCP tools or workflows:

1. Share your OSC command patterns
2. Contribute to ardour-mcp documentation
3. Create example sessions/templates
4. Write tutorials and blog posts
5. Report bugs and enhancement requests

## Appendix: Complete Setup Checklist

- [ ] Ardour built from source with OSC support
- [ ] Just command runner installed
- [ ] Ardour running: `just run-osc-debug`
- [ ] OSC enabled in Ardour preferences
- [ ] OSC port 3819 confirmed open: `just test-osc-connection`
- [ ] ardour-mcp server installed and configured
- [ ] ardour-mcp server running
- [ ] Claude Code MCP settings configured
- [ ] Test connection from Claude successful
- [ ] OSC debug output visible in Ardour logs
- [ ] Ready to use AI-assisted audio production!

## Quick Reference

```bash
# Complete setup in one command
just setup-mcp

# Get OSC information
just osc-info

# Get MCP integration information
just mcp-info

# Run Ardour with OSC debugging
just run-osc-debug

# Test OSC connection
just test-osc-connection

# Watch Ardour logs
just watch-logs
```

---

**Last Updated**: 2025-11-06
**Ardour Version**: 8.x
**OSC Protocol Version**: 1.0
