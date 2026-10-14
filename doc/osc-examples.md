# Ardour OSC Configuration Examples

This document provides practical examples for configuring and using Ardour's Open Sound Control (OSC) interface.

## Quick Start

### Enabling OSC in Ardour

1. Launch Ardour: `just run-osc-debug`
2. Navigate to: `Edit > Preferences > Control Surfaces`
3. Enable: `Open Sound Control (OSC)`
4. Click: `Show Protocol Settings`

### Recommended OSC Settings

```
Port:                   3819
Reply Port:             Auto (or specify if needed)
Bank Size:              0 (unlimited)
Strip Types:            All enabled
Feedback:               Enable for interactive control
Gainmode:               dB
Default Strip:          Audio Tracks
Debug Mode:             Enable during development
```

## Testing OSC Connection

### Method 1: Using Command Line Tools

```bash
# Test if port is listening
just test-osc-connection

# Or manually with netcat
nc -zv -u 127.0.0.1 3819
```

### Method 2: Send Test OSC Message

Install `liblo-tools`:
```bash
# Ubuntu/Debian
sudo apt-get install liblo-tools

# macOS
brew install liblo
```

Send a test command:
```bash
# Start playback
oscsend localhost 3819 /transport_play

# Stop playback
oscsend localhost 3819 /transport_stop

# Get session info
oscsend localhost 3819 /strip/list
```

### Method 3: Python Script

```python
#!/usr/bin/env python3
# test_osc.py - Test Ardour OSC connection

from pythonosc import udp_client
import time

# Create OSC client
client = udp_client.SimpleUDPClient("127.0.0.1", 3819)

print("Testing Ardour OSC connection...")

# Test transport controls
print("Starting playback...")
client.send_message("/transport_play", [])
time.sleep(2)

print("Stopping playback...")
client.send_message("/transport_stop", [])
time.sleep(1)

print("Rewinding to start...")
client.send_message("/goto_start", [])

print("Test complete!")
```

Install dependencies:
```bash
pip install python-osc
```

Run:
```bash
python3 test_osc.py
```

## Common OSC Commands

### Transport Control

```bash
# Start/Stop
oscsend localhost 3819 /transport_play
oscsend localhost 3819 /transport_stop

# Recording
oscsend localhost 3819 /rec_enable_toggle
oscsend localhost 3819 /rec_enable i 1    # Enable recording
oscsend localhost 3819 /rec_enable i 0    # Disable recording

# Navigation
oscsend localhost 3819 /goto_start
oscsend localhost 3819 /goto_end
oscsend localhost 3819 /rewind
oscsend localhost 3819 /ffwd

# Loop
oscsend localhost 3819 /loop_toggle
oscsend localhost 3819 /set_loop_range     # Set loop to current selection
```

### Track/Strip Control

```bash
# Gain control (ssid = strip ID, starts at 1)
oscsend localhost 3819 /strip/gain if 1 0.5       # Set track 1 gain to 50%
oscsend localhost 3819 /strip/gain if 1 1.0       # Set track 1 gain to 100%
oscsend localhost 3819 /strip/gain if 1 0.0       # Set track 1 gain to 0%

# Fader control (in dB)
oscsend localhost 3819 /strip/fader if 1 0.0      # Set track 1 to 0dB
oscsend localhost 3819 /strip/fader if 1 -6.0     # Set track 1 to -6dB
oscsend localhost 3819 /strip/fader if 1 -inf     # Set track 1 to -infinity

# Pan control (-1.0 = full left, 0.0 = center, 1.0 = full right)
oscsend localhost 3819 /strip/pan_stereo_position if 1 0.0   # Center
oscsend localhost 3819 /strip/pan_stereo_position if 1 -1.0  # Full left
oscsend localhost 3819 /strip/pan_stereo_position if 1 1.0   # Full right

# Mute/Solo
oscsend localhost 3819 /strip/mute if 1 1         # Mute track 1
oscsend localhost 3819 /strip/mute if 1 0         # Unmute track 1
oscsend localhost 3819 /strip/solo if 1 1         # Solo track 1
oscsend localhost 3819 /strip/solo if 1 0         # Unsolo track 1

# Record enable
oscsend localhost 3819 /strip/recenable if 1 1    # Enable recording on track 1
oscsend localhost 3819 /strip/recenable if 1 0    # Disable recording on track 1

# Monitor
oscsend localhost 3819 /strip/monitor_input if 1 1
oscsend localhost 3819 /strip/monitor_disk if 1 1
```

### Session Information

```bash
# Get track list
oscsend localhost 3819 /strip/list

# Get track name
oscsend localhost 3819 /strip/name if 1

# Get session frame rate
oscsend localhost 3819 /session/frame_rate

# Get current time
oscsend localhost 3819 /session/timecode
```

### Master Bus Control

```bash
# Master gain
oscsend localhost 3819 /master/gain f 1.0

# Master mute
oscsend localhost 3819 /master/mute i 1

# Master fader (dB)
oscsend localhost 3819 /master/fader f 0.0
```

## Python Examples

### Complete Control Script

```python
#!/usr/bin/env python3
# ardour_control.py - Comprehensive Ardour control example

from pythonosc import udp_client
import time
import sys

class ArdourOSC:
    def __init__(self, host="127.0.0.1", port=3819):
        self.client = udp_client.SimpleUDPClient(host, port)

    def transport_play(self):
        """Start playback"""
        self.client.send_message("/transport_play", [])

    def transport_stop(self):
        """Stop playback"""
        self.client.send_message("/transport_stop", [])

    def goto_start(self):
        """Jump to session start"""
        self.client.send_message("/goto_start", [])

    def set_track_gain(self, track_id, gain):
        """Set track gain (0.0 to 1.0)"""
        self.client.send_message("/strip/gain", [track_id, gain])

    def set_track_fader(self, track_id, db):
        """Set track fader in dB"""
        self.client.send_message("/strip/fader", [track_id, db])

    def mute_track(self, track_id, mute=True):
        """Mute or unmute a track"""
        self.client.send_message("/strip/mute", [track_id, 1 if mute else 0])

    def solo_track(self, track_id, solo=True):
        """Solo or unsolo a track"""
        self.client.send_message("/strip/solo", [track_id, 1 if solo else 0])

    def set_pan(self, track_id, position):
        """Set pan position (-1.0 to 1.0)"""
        self.client.send_message("/strip/pan_stereo_position", [track_id, position])

    def rec_enable_track(self, track_id, enable=True):
        """Enable/disable recording on a track"""
        self.client.send_message("/strip/recenable", [track_id, 1 if enable else 0])

def main():
    # Create controller
    ardour = ArdourOSC()

    print("Ardour OSC Control Demo")
    print("-" * 40)

    # Example workflow
    print("1. Going to start...")
    ardour.goto_start()
    time.sleep(1)

    print("2. Setting up track 1...")
    ardour.set_track_fader(1, -6.0)  # Set to -6dB
    ardour.set_pan(1, 0.0)           # Center pan
    ardour.mute_track(1, False)      # Ensure unmuted
    time.sleep(1)

    print("3. Starting playback...")
    ardour.transport_play()
    time.sleep(5)

    print("4. Stopping playback...")
    ardour.transport_stop()

    print("\nDemo complete!")

if __name__ == "__main__":
    main()
```

### Batch Track Setup

```python
#!/usr/bin/env python3
# setup_mix.py - Batch configure multiple tracks

from pythonosc import udp_client

def setup_standard_mix():
    """Set up a standard rock band mix"""
    client = udp_client.SimpleUDPClient("127.0.0.1", 3819)

    # Define mix template
    # Format: (track_id, gain_db, pan, name)
    mix_template = [
        (1, -6.0, 0.0, "Kick"),        # Center, moderate level
        (2, -8.0, 0.0, "Snare"),       # Center, slightly lower
        (3, -10.0, -0.3, "Tom 1"),     # Left of center
        (4, -10.0, 0.3, "Tom 2"),      # Right of center
        (5, -6.0, -0.8, "OH Left"),    # Hard left
        (6, -6.0, 0.8, "OH Right"),    # Hard right
        (7, -4.0, 0.0, "Bass"),        # Center, loud
        (8, -6.0, -0.5, "Guitar L"),   # Left
        (9, -6.0, 0.5, "Guitar R"),    # Right
        (10, -3.0, 0.0, "Vocal"),      # Center, loudest
    ]

    print("Setting up standard rock mix...")
    for track_id, gain_db, pan, name in mix_template:
        print(f"  Configuring {name} (Track {track_id})")
        client.send_message("/strip/fader", [track_id, gain_db])
        client.send_message("/strip/pan_stereo_position", [track_id, pan])

    print("Mix setup complete!")

if __name__ == "__main__":
    setup_standard_mix()
```

### OSC Monitor

```python
#!/usr/bin/env python3
# osc_monitor.py - Monitor OSC messages from Ardour

from pythonosc import dispatcher
from pythonosc import osc_server
import argparse

def print_handler(unused_addr, *args):
    """Print all received OSC messages"""
    print(f"[OSC] {unused_addr}: {args}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ip", default="127.0.0.1", help="Listen IP")
    parser.add_argument("--port", type=int, default=3820, help="Listen port")
    args = parser.parse_args()

    # Set up dispatcher to catch all messages
    disp = dispatcher.Dispatcher()
    disp.set_default_handler(print_handler)

    # Create server
    server = osc_server.ThreadingOSCUDPServer((args.ip, args.port), disp)

    print(f"OSC Monitor listening on {args.ip}:{args.port}")
    print("Configure Ardour to send feedback to this port")
    print("Press Ctrl+C to exit\n")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")

if __name__ == "__main__":
    main()
```

## Node.js Examples

### Basic OSC Client

```javascript
// ardour-osc.js - Node.js OSC client for Ardour

const osc = require('node-osc');

class ArdourOSC {
    constructor(host = '127.0.0.1', port = 3819) {
        this.client = new osc.Client(host, port);
    }

    send(address, ...args) {
        this.client.send(address, ...args);
    }

    play() {
        this.send('/transport_play');
    }

    stop() {
        this.send('/transport_stop');
    }

    setTrackGain(trackId, gain) {
        this.send('/strip/gain', trackId, gain);
    }

    setTrackFader(trackId, db) {
        this.send('/strip/fader', trackId, db);
    }

    muteTrack(trackId, mute = true) {
        this.send('/strip/mute', trackId, mute ? 1 : 0);
    }
}

// Example usage
const ardour = new ArdourOSC();

console.log('Testing Ardour connection...');
ardour.play();
setTimeout(() => ardour.stop(), 3000);
```

Install dependencies:
```bash
npm install node-osc
```

## Shell Script Examples

### Quick Control Script

```bash
#!/bin/bash
# ardour-ctl.sh - Shell script for Ardour control

OSC_HOST="localhost"
OSC_PORT="3819"

osc_send() {
    oscsend "$OSC_HOST" "$OSC_PORT" "$@"
}

case "$1" in
    play)
        echo "Starting playback..."
        osc_send /transport_play
        ;;
    stop)
        echo "Stopping playback..."
        osc_send /transport_stop
        ;;
    start)
        echo "Jumping to start..."
        osc_send /goto_start
        ;;
    mute)
        if [ -z "$2" ]; then
            echo "Usage: $0 mute <track_id>"
            exit 1
        fi
        echo "Muting track $2..."
        osc_send /strip/mute if "$2" 1
        ;;
    unmute)
        if [ -z "$2" ]; then
            echo "Usage: $0 unmute <track_id>"
            exit 1
        fi
        echo "Unmuting track $2..."
        osc_send /strip/mute if "$2" 0
        ;;
    *)
        echo "Usage: $0 {play|stop|start|mute|unmute} [args]"
        exit 1
        ;;
esac
```

Make executable:
```bash
chmod +x ardour-ctl.sh
```

Usage:
```bash
./ardour-ctl.sh play
./ardour-ctl.sh mute 1
./ardour-ctl.sh stop
```

## OSC Message Reference

### Path Format

```
/strip/<command> [ssid] [arguments]
```

Where:
- `ssid` = Strip/track ID (starts at 1)
- Arguments vary by command

### Complete Command List

See the [Ardour OSC documentation](https://manual.ardour.org/using-control-surfaces/controlling-ardour-with-osc/osc-control/) for the complete list of supported messages.

Common patterns:
```
/strip/[parameter]  if  [ssid] [value]
/[global_command]
/master/[parameter] [value]
```

## Troubleshooting

### No Response from Ardour

1. Verify OSC is enabled in preferences
2. Check port number (default: 3819)
3. Ensure Ardour is running
4. Test with debug output: `ARDOUR_DEBUG=OSC just run`

### Messages Not Received

1. Check firewall settings
2. Verify correct IP address (use 127.0.0.1 for local)
3. Ensure UDP protocol (not TCP)
4. Test with `oscdump` to monitor traffic

### Permission Issues

```bash
# Check port permissions
sudo lsof -i :3819

# Allow local OSC traffic
sudo ufw allow from 127.0.0.1 to any port 3819
```

## Best Practices

1. **Always use localhost (127.0.0.1)** for development
2. **Add error handling** in scripts
3. **Add delays** between rapid commands (10-50ms)
4. **Test commands individually** before batch operations
5. **Enable debug mode** during development
6. **Document your OSC workflows** for team members
7. **Use strip names** instead of IDs when possible (via feedback)

## Next Steps

1. Read the complete guide: `doc/ardour-mcp-integration.md`
2. Set up ardour-mcp server
3. Try the examples above
4. Create your own control scripts
5. Share your workflows with the community

## Resources

- OSC Specification: http://opensoundcontrol.org/
- python-osc: https://pypi.org/project/python-osc/
- node-osc: https://www.npmjs.com/package/node-osc
- Ardour Manual: https://manual.ardour.org/

---

**Last Updated**: 2025-11-06
