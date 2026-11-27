# Ardour OSC & MCP Quick Start

This directory contains documentation for using Ardour with OSC (Open Sound Control) and MCP (Model Context Protocol) for AI-assisted audio production.

## What's This About?

The ardour-mcp project enables AI assistants (like Claude) to control Ardour through OSC, creating powerful AI-assisted workflows for audio production, mixing, and automation.

## Quick Start (5 Minutes)

```bash
# 1. Build Ardour with OSC support
just setup-mcp

# 2. Run Ardour with OSC debugging
just run-osc-debug

# 3. In Ardour: Edit > Preferences > Control Surfaces
#    - Enable "Open Sound Control (OSC)"
#    - Set port to 3819

# 4. Test the connection
just test-osc-connection

# 5. Set up and run ardour-mcp server (in sibling directory)
cd ../ardour-mcp
# (follow ardour-mcp setup instructions)
```

## Documentation

- **[ardour-mcp-integration.md](./ardour-mcp-integration.md)** - Complete integration guide
  - What is MCP and why use it with Ardour
  - Full setup instructions
  - Architecture overview
  - Troubleshooting guide

- **[osc-examples.md](./osc-examples.md)** - Practical OSC examples
  - Quick configuration guide
  - OSC command reference
  - Python, Node.js, and shell script examples
  - Testing and monitoring tools

## Common Commands (Using Just)

```bash
# Get help
just help

# Development workflow
just dev                  # Configure, build, run (debug mode)
just osc-dev             # Build with OSC + run with OSC debug

# Information
just osc-info            # Show OSC configuration info
just mcp-info            # Show MCP integration info

# Building
just configure-osc       # Configure with OSC support
just build               # Build with parallel jobs
just rebuild             # Clean and rebuild

# Running
just run                 # Run from build directory
just run-osc-debug       # Run with OSC debugging
just run-debug-all       # Run with all debug output

# Testing
just check-osc           # Verify OSC library was built
just test-osc-connection # Test if OSC port is responding
just watch-logs          # Monitor Ardour logs in real-time
```

## Use Cases

- **AI-Assisted Mixing**: Get intelligent suggestions for EQ, compression, and mix balance
- **Automation**: Automate repetitive tasks through natural language commands
- **Session Analysis**: AI analysis of your session structure and routing
- **Learning**: Interactive guidance for learning Ardour features
- **Scripting**: Generate Lua scripts for custom automation

## Example Interactions with Claude

Once set up, you can interact with Ardour through Claude:

```
You: "Show me all the tracks in my current session"
Claude: [Uses MCP to query Ardour via OSC]
        "Your session has 12 tracks: Kick, Snare, Toms, OH-L, OH-R,
        Bass, Guitar-L, Guitar-R, Keys, Vocal-Lead, Vocal-BG, Master"

You: "Mute all the drum tracks"
Claude: [Sends OSC commands to mute tracks 1-6]
        "Muted 6 drum tracks"

You: "Set up a parallel compression bus for the drums"
Claude: [Creates aux track, configures routing, suggests plugin settings]
        "Created 'Drums Parallel' bus with routing from tracks 1-6..."
```

## Architecture

```
Claude Code/AI Assistant
         ↕ MCP Protocol
    ardour-mcp Server
         ↕ OSC Protocol (UDP Port 3819)
       Ardour DAW
```

## Requirements

- Ardour 8.x+ (built from source)
- Python 3.8+ or Node.js 14+ (for ardour-mcp)
- Just command runner (optional but recommended)
- ardour-mcp server (sibling project)

## Troubleshooting

### OSC Not Working
```bash
# Check if OSC is enabled
just osc-info

# Verify OSC library exists
just check-osc

# Test connection
just test-osc-connection

# Run with debug output
just run-osc-debug
```

### Need More Help?

1. Check the detailed guides linked above
2. Look at [osc-examples.md](./osc-examples.md) for working code
3. Watch logs: `just watch-logs`
4. Ask in Ardour forums: https://discourse.ardour.org/

## Security Note

OSC has no authentication. Keep port 3819 accessible only from localhost (127.0.0.1). Never expose it to untrusted networks.

## Contributing

Found a useful workflow? Share it!
- Add examples to osc-examples.md
- Contribute to ardour-mcp documentation
- Share templates and scripts
- Report issues and improvements

## Resources

- [Ardour Manual - OSC](https://manual.ardour.org/using-control-surfaces/controlling-ardour-with-osc/)
- [OSC Protocol Specification](https://opensoundcontrol.stanford.edu/)
- [Model Context Protocol](https://modelcontextprotocol.io/)
- [Ardour Development](https://ardour.org/development.html)

## Quick Reference Card

| Task | Command |
|------|---------|
| Complete setup | `just setup-mcp` |
| Build & run with OSC | `just osc-dev` |
| Test OSC | `just test-osc-connection` |
| Show OSC info | `just osc-info` |
| Show MCP info | `just mcp-info` |
| Watch logs | `just watch-logs` |
| Help | `just help` |

---

**Ready to start?** Run `just setup-mcp` and follow the prompts!
