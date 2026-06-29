# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Ardour is a professional digital audio workstation (DAW) for Linux, macOS, and Windows. It supports multi-track audio and MIDI recording, editing, mixing, and mastering with support for various plugin formats (LV2, VST2, VST3, AU).

Official website: https://ardour.org/
Development guide: https://ardour.org/development.html

## Build System

Ardour uses **Waf** (Python-based build system). All build commands use `python3 ./waf`.

### Essential Build Commands

```bash
# Set PKG_CONFIG_PATH for Homebrew/custom library paths (if needed)
export PKG_CONFIG_PATH="/home/linuxbrew/.linuxbrew/lib/pkgconfig:/home/linuxbrew/.linuxbrew/share/pkgconfig:/usr/lib/pkgconfig"

# Configure the build
python3 ./waf configure

# Build with parallel jobs
python3 ./waf build -j4

# Install (optional)
python3 ./waf install

# Clean build
python3 ./waf clean

# Complete clean (removes configuration)
python3 ./waf distclean
```

### Build Options

```bash
# Configure with optimizations
python3 ./waf configure --optimize

# Build with debugging symbols
python3 ./waf configure --debug-symbols

# Build with tests
python3 ./waf configure --test

# Run tests after build
python3 ./waf configure --run-tests

# Build specific backends (JACK, ALSA, PulseAudio, etc.)
python3 ./waf configure --with-backends=jack,alsa,dummy

# Build without VST support
python3 ./waf configure --no-lxvst --no-vst3

# Force C++17 mode
python3 ./waf configure --cxx17
```

### Build Outputs

- Main executable: `build/gtk2_ardour/ardour-<VERSION>`
- Libraries: `build/libs/*/`
- Built-in LV2 plugins: `build/libs/plugins/*/`

### Running Tests

Individual library tests can be found in `libs/*/test/` directories:

```bash
# Build tests
python3 ./waf configure --test
python3 ./waf build

# Run evoral tests (example)
cd libs/evoral
./run-tests.sh

# Set up environment for test data
export EVORAL_TEST_PATH="libs/evoral/test/testdata"
export LD_LIBRARY_PATH="build/libs/evoral:build/libs/pbd:$LD_LIBRARY_PATH"
```

## Architecture Overview

### Core Architecture Layers

Ardour is structured as a layered system with clear separation between the audio engine, session management, and UI:

1. **Audio Engine Layer** (`libs/ardour/`)
   - Session management (libs/ardour/ardour/session.h)
   - Audio/MIDI routing and processing
   - Real-time audio graph execution
   - Plugin hosting (LV2, VST2, VST3, AU)
   - Export/import functionality

2. **Low-Level Libraries**
   - `libs/pbd/` - Portable Build Dependencies (threading, signals, file utilities)
   - `libs/temporal/` - Time and tempo handling (beats, bars, superclock)
   - `libs/evoral/` - Event-based MIDI sequencing and control automation
   - `libs/audiographer/` - Audio processing graph for export/analysis

3. **Backend Layer** (`libs/backends/`)
   - Audio backend abstractions: JACK, ALSA, PulseAudio, CoreAudio, PortAudio, Dummy
   - Each backend implements the `AudioBackend` interface

4. **UI Layer** (`gtk2_ardour/`)
   - GTK2-based UI (despite the name, uses GTK3+ in modern versions)
   - Canvas rendering system (`libs/canvas/`)
   - Custom widgets (`libs/widgets/`, `libs/gtkmm2ext/`)
   - Waveform rendering (`libs/waveview/`)

5. **Control Surfaces** (`libs/surfaces/`)
   - Hardware controller support: Mackie Control, Faderport, Push2, OSC, etc.
   - Generic MIDI control
   - WebSocket interface for remote control

6. **Plugin System**
   - Built-in plugins in `libs/plugins/` (a-comp, a-delay, a-eq, etc.)
   - Plugin discovery and scanning (`libs/auscan/` for AU)
   - VST support (`libs/fst/`, `libs/vst3/`)

### Key Concepts

- **Session**: The top-level container for a project. Manages routes, sources, regions, playlists, and the processing graph.
- **Route**: Abstract base for tracks and busses. Handles signal flow, processors, and automation.
- **Region**: A section of audio or MIDI data with position and length.
- **Processor**: Audio/MIDI processing element (plugins, sends, inserts).
- **Temporal**: Ardour uses multiple time domains (audio samples, musical beats, wall-clock time). The `libs/temporal/` library handles conversions.

### Signal Flow

```
AudioBackend → AudioEngine → Session → Routes → Processors → BufferSet
```

Audio flows through:
1. Backend captures audio from hardware
2. AudioEngine distributes to session
3. Session routes through the processing graph
4. Each Route processes through its processor chain
5. Output buffers are sent back to the backend

### Thread Model

- **Process thread**: Real-time audio processing (JACK process callback or equivalent)
- **Butler thread**: Disk I/O operations (reading/writing audio files)
- **GUI thread**: User interface updates (GTK main loop)
- **Background threads**: File scanning, analysis, exports

Communication between threads uses lock-free structures and message queues to maintain real-time safety.

## Lua Scripting

Ardour has extensive Lua scripting support for automation and extending functionality.

- Scripts location: `share/scripts/`
- Documentation: https://manual.ardour.org/lua-scripting/
- Interactive scripting console: Window > Scripting in the UI

Script naming conventions:
- `_*.lua` - Example scripts (not installed)
- `__*.lua` - Excluded from unit tests
- `s_*.lua` - Code snippets for interactive use
- `_-*.lua` - Ignored by git (local dev scripts)

## Development Environment

### Running from Build Directory

Use the `ardev` wrapper scripts to run Ardour from the build directory without installing:

```bash
# Linux
./gtk2_ardour/ardev

# Set up environment variables manually (if needed)
source gtk2_ardour/ardev_common.sh
```

The `ardev` script sets up:
- `ARDOUR_SURFACES_PATH` - Control surface modules
- `ARDOUR_BACKEND_PATH` - Audio backend modules
- `ARDOUR_DATA_PATH` - UI themes, icons, templates
- `ARDOUR_MIDI_PATCH_PATH` - MIDI instrument definitions

### Debugging

```bash
# Build with debug symbols
python3 ./waf configure --debug-symbols

# Run with gdb
gdb --args ./build/gtk2_ardour/ardour-<VERSION>

# Enable Ardour debug flags (set before running)
export ARDOUR_DEBUG="all"  # or specific flags like "AudioEngine,Processor"
```

### Session Utilities

Command-line tools for batch processing sessions (in `session_utils/`):

```bash
# Example utilities
./build/session_utils/new_session       # Create new session from command line
./build/session_utils/copy-mixer        # Copy mixer settings
./build/session_utils/export            # Batch export
```

## Important Files and Directories

- `wscript` - Main build configuration
- `libs/ardour/` - Core audio engine
- `gtk2_ardour/` - Main UI implementation
- `libs/backends/` - Audio backend implementations
- `libs/surfaces/` - Control surface support
- `libs/plugins/` - Built-in audio plugins (LV2 format)
- `share/` - Runtime data (templates, scripts, themes)
- `tools/` - Development and packaging utilities

## Coding Patterns

### Memory Management

- Modern C++ with extensive use of `std::shared_ptr` and smart pointers
- RCU (Read-Copy-Update) pattern for lock-free data structures (`pbd/rcu.h`)
- Real-time safe allocators for audio thread (`pbd/reallocpool.h` or `pbd/tlsf.h`)

### Signal/Slot System

Uses `pbd/signals.h` (not Boost.Signals) for decoupled communication between components.

### State Management

Most objects inherit from `PBD::Stateful` for serialization to XML. Session state is saved in `.ardour` XML files.

## Common Development Workflows

### Adding a New Built-in Plugin

1. Create plugin directory in `libs/plugins/`
2. Implement LV2 plugin interface
3. Add to `libs/plugins/wscript`
4. Rebuild: `python3 ./waf build`

### Modifying the UI

1. UI code is in `gtk2_ardour/`
2. Canvas rendering uses `libs/canvas/`
3. Custom widgets in `libs/widgets/`
4. Themes are in `gtk2_ardour/themes/`

### Working with Sessions

1. Session code in `libs/ardour/session*.cc`
2. Session state format defined by XML serialization
3. Use `session_utils/` tools for batch operations

## Platform-Specific Notes

### Linux
- Primary development platform
- Requires JACK, ALSA, or PulseAudio
- Package dependencies: see https://ardour.org/development.html

### macOS
- Requires macOS 10.13+
- Uses CoreAudio backend
- AU (Audio Units) plugin support

### Windows
- MinGW cross-compilation or MSVC
- ASIO support via PortAudio backend

## Git Workflow

- Main branch: `master`
- Requires full git clone (uses `git describe` for versioning)
- GitHub release tarballs are NOT supported - clone the repository

## Important Notes

- Ardour version is maintained via git tags
- The build depends on `git describe` output for version information
- Pre-commit hooks may modify files (auto-formatting, version updates)
- When committing, always check for hook-modified files before finalizing
