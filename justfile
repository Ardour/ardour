# Ardour Build and Development Helper
# Uses just (https://github.com/casey/just)

# Default recipe - show available commands
default:
    @just --list

# Configure build with default options
configure:
    python3 ./waf configure

# Configure with debugging symbols (useful for development)
configure-debug:
    python3 ./waf configure --debug-symbols

# Configure with optimizations (for production builds)
configure-optimized:
    python3 ./waf configure --optimize

# Configure with tests enabled
configure-test:
    python3 ./waf configure --test --run-tests

# Configure with OSC surface support explicitly enabled
configure-osc:
    python3 ./waf configure --surfaces

# Build Ardour (parallel build with 4 jobs)
build jobs="4":
    python3 ./waf build -j{{jobs}}

# Build and show verbose output
build-verbose:
    python3 ./waf build -v

# Clean build artifacts (keeps configuration)
clean:
    python3 ./waf clean

# Complete clean (removes configuration too)
distclean:
    python3 ./waf distclean

# Install Ardour to system
install:
    python3 ./waf install

# Run Ardour from build directory (development mode)
run:
    ./gtk2_ardour/ardev

# Run Ardour with OSC debug output enabled
run-osc-debug:
    ARDOUR_DEBUG=OSC ./gtk2_ardour/ardev

# Run Ardour with all debug output enabled
run-debug-all:
    ARDOUR_DEBUG=all ./gtk2_ardour/ardev

# Run Ardour with specific debug flags (e.g., just --set debug="AudioEngine,Processor")
run-debug debug="OSC":
    ARDOUR_DEBUG={{debug}} ./gtk2_ardour/ardev

# Full rebuild (clean, configure, build)
rebuild: clean configure build

# Full fresh build (distclean, configure, build)
fresh: distclean configure build

# Quick build workflow for development
dev: configure-debug build run

# Build with OSC support and run with OSC debugging
osc-dev: configure-osc build run-osc-debug

# Run tests (requires configure-test first)
test:
    cd libs/evoral && ./run-tests.sh

# Setup test environment variables
test-env:
    #!/usr/bin/env bash
    export EVORAL_TEST_PATH="libs/evoral/test/testdata"
    export LD_LIBRARY_PATH="build/libs/evoral:build/libs/pbd:$LD_LIBRARY_PATH"
    echo "Test environment configured"

# Check OSC surface library was built
check-osc:
    @ls -lh build/libs/surfaces/osc/osc.so 2>/dev/null || echo "OSC surface not built. Run: just configure-osc build"

# Display OSC configuration info
osc-info:
    @echo "OSC Surface Information:"
    @echo "------------------------"
    @echo "Default OSC Port: 3819"
    @echo "Protocol: UDP"
    @echo "Documentation: https://manual.ardour.org/using-control-surfaces/controlling-ardour-with-osc/"
    @echo ""
    @echo "To enable OSC in Ardour:"
    @echo "1. Start Ardour: just run"
    @echo "2. Go to: Edit > Preferences > Control Surfaces"
    @echo "3. Enable 'Open Sound Control (OSC)'"
    @echo "4. Click 'Show Protocol Settings' to configure port and options"
    @echo ""
    @echo "For ardour-mcp integration, see: doc/ardour-mcp-integration.md"

# Show MCP integration information
mcp-info:
    @echo "Ardour-MCP Integration:"
    @echo "----------------------"
    @echo "The ardour-mcp project provides a Model Context Protocol server"
    @echo "that allows AI assistants (like Claude) to control Ardour via OSC."
    @echo ""
    @echo "Setup steps:"
    @echo "1. Build and run Ardour: just osc-dev"
    @echo "2. Enable OSC in Ardour (port 3819)"
    @echo "3. Run ardour-mcp server in the sibling directory"
    @echo "4. Connect Claude Code to the MCP server"
    @echo ""
    @echo "See doc/ardour-mcp-integration.md for detailed instructions"

# Test OSC connection (requires netcat)
test-osc-connection port="3819":
    @echo "Testing OSC connection on port {{port}}..."
    @nc -zv -u 127.0.0.1 {{port}} 2>&1 || echo "Port {{port}} not responding. Is Ardour running with OSC enabled?"

# Show build info
info:
    @echo "Ardour Build Information:"
    @echo "------------------------"
    @python3 ./waf --version
    @echo ""
    @git describe --tags --always 2>/dev/null || echo "Version: unknown (not a git repo)"
    @echo ""
    @echo "Build directory: $(pwd)/build"
    @echo "Main executable: build/gtk2_ardour/ardour-*"

# Show current build configuration
config-info:
    @cat build/c4che/_cache.py 2>/dev/null | grep -E "(PREFIX|ARDOUR|SURFACES)" || echo "Not configured. Run: just configure"

# Create OSC configuration template
create-osc-config:
    @mkdir -p ~/.config/ardour8
    @echo "Creating OSC configuration template..."
    @echo "Manual configuration required in Ardour UI"
    @echo "Config location: ~/.config/ardour8/"

# Show log files location
logs:
    @echo "Ardour log files are typically in:"
    @echo "~/.config/ardour8/ardour.log"
    @ls -lh ~/.config/ardour*/ardour.log 2>/dev/null || echo "No logs found yet"

# Watch Ardour logs in real-time
watch-logs:
    tail -f ~/.config/ardour*/ardour.log

# Complete setup for MCP development
setup-mcp: configure-osc build osc-info mcp-info
    @echo ""
    @echo "Setup complete! Next steps:"
    @echo "1. Run: just run-osc-debug"
    @echo "2. Enable OSC in Ardour (Edit > Preferences > Control Surfaces)"
    @echo "3. Start ardour-mcp server"

# Quick reference card
help:
    @echo "Ardour Development Quick Reference"
    @echo "==================================="
    @echo ""
    @echo "Common Workflows:"
    @echo "  just dev              - Configure, build, and run (debug mode)"
    @echo "  just osc-dev          - Build with OSC support and run with OSC debug"
    @echo "  just setup-mcp        - Complete setup for MCP integration"
    @echo ""
    @echo "Building:"
    @echo "  just configure        - Configure build"
    @echo "  just build            - Build with 4 parallel jobs"
    @echo "  just rebuild          - Clean and rebuild"
    @echo "  just fresh            - Complete fresh build"
    @echo ""
    @echo "Running:"
    @echo "  just run              - Run Ardour from build directory"
    @echo "  just run-osc-debug    - Run with OSC debugging enabled"
    @echo "  just run-debug-all    - Run with all debug output"
    @echo ""
    @echo "OSC & MCP:"
    @echo "  just osc-info         - Show OSC configuration info"
    @echo "  just mcp-info         - Show MCP integration info"
    @echo "  just check-osc        - Verify OSC library was built"
    @echo "  just test-osc-connection - Test OSC port connectivity"
    @echo ""
    @echo "Maintenance:"
    @echo "  just clean            - Clean build artifacts"
    @echo "  just distclean        - Complete clean"
    @echo "  just info             - Show build information"
    @echo ""
    @echo "For full command list: just --list"
