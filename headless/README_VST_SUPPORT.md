# VST Plugin Support for Ardour Headless Export

## Overview

This implementation adds VST plugin support to Ardour's headless export functionality (`hardour`). The headless export tool can now load and process VST plugins during audio export, enabling automated audio processing workflows with VST plugins.

## Features

- **VST Plugin Loading**: Support for Windows VST, Linux VST, and Mac VST plugins
- **Plugin Processing**: Full audio processing through VST plugin chains during export
- **Error Handling**: Robust error handling with plugin blacklisting and retry mechanisms
- **Timeout Protection**: Configurable timeout mechanisms to prevent hanging on problematic plugins
- **Configuration**: Flexible configuration through command-line options and configuration files

## Command-Line Options

### New Options for VST Support

- `-E, --enable-plugins`: Enable VST plugin processing during export
- `-T, --plugin-timeout <ms>`: Plugin processing timeout in milliseconds (default: 30000)
- `-S, --strict-plugins`: Exit on plugin loading failure (default: continue with disabled plugins)
- `-V, --vst-path <path>`: Additional VST plugin search path
- `-B, --plugin-blacklist <f>`: Plugin blacklist file

### Example Usage

```bash
# Basic export with VST plugins enabled
./hardour -E /path/to/session session_name

# Export with custom timeout and strict plugin loading
./hardour -E -T 60000 -S /path/to/session session_name

# Export with additional VST path and blacklist
./hardour -E -V /custom/vst/path -B /path/to/blacklist.txt /path/to/session session_name
```

## Configuration

### Configuration File

The headless export tool supports a configuration file located at `~/.config/ardour/headless_config`:

```ini
# Ardour Headless Configuration
enable_plugins=true
plugin_timeout_ms=30000
strict_plugin_loading=false
vst_path=/custom/vst/path
plugin_blacklist_file=/path/to/blacklist.txt
plugin_memory_limit_mb=1024
plugin_threads=1
```

### Environment Variables

The following environment variables can override configuration file settings:

- `ARDOUR_HEADLESS_ENABLE_PLUGINS`: Enable/disable plugin support (true/false)
- `ARDOUR_HEADLESS_PLUGIN_TIMEOUT`: Plugin timeout in milliseconds
- `ARDOUR_HEADLESS_STRICT_PLUGINS`: Strict plugin loading mode (true/false)
- `ARDOUR_HEADLESS_VST_PATH`: Additional VST plugin search path
- `ARDOUR_HEADLESS_PLUGIN_BLACKLIST`: Plugin blacklist file path
- `ARDOUR_HEADLESS_PLUGIN_MEMORY_LIMIT`: Memory limit for plugins in MB
- `ARDOUR_HEADLESS_PLUGIN_THREADS`: Number of plugin processing threads

## Plugin Blacklisting

Plugins that fail to load or cause crashes are automatically blacklisted after 3 consecutive failures. The blacklist is maintained across sessions and can be manually edited.

### Blacklist File Format

```
# Plugin blacklist file
# One plugin name per line
problematic_plugin_1
problematic_plugin_2
```

## Error Handling

### Plugin Loading Failures

- **Retry Mechanism**: Plugins are retried up to 3 times with 5-second timeouts
- **Graceful Degradation**: Failed plugins are disabled and export continues
- **Strict Mode**: With `-S` flag, export fails on first plugin loading error
- **Blacklisting**: Repeatedly failing plugins are automatically blacklisted

### Timeout Protection

- **Configurable Timeouts**: Plugin operations have configurable timeouts
- **Async Processing**: Plugin operations run in separate threads with timeout protection
- **Cancellation**: Long-running plugin operations can be cancelled

## Technical Implementation

### Architecture

1. **Plugin Manager Initialization**: Enhanced plugin manager with full discovery
2. **VST Host Environment**: Headless VST host contexts for each platform
3. **Plugin Loading**: Robust plugin loading with error handling and timeouts
4. **Export Processing**: Integration with export processing pipeline
5. **Error Handling**: Comprehensive error handling and recovery mechanisms

### Platform Support

- **Windows**: FST-based VST support with headless initialization
- **Linux**: LXVST support with headless X11 context
- **macOS**: MacVST support with headless Core Graphics context

### Files Added/Modified

#### New Files

- `libs/ardour/fst_headless.cc`: Windows VST headless support
- `libs/ardour/linux_vst_support_headless.cc`: Linux VST headless support
- `libs/ardour/mac_vst_support_headless.cc`: Mac VST headless support
- `headless/plugin_loader.cc`: Plugin loading with timeout support
- `headless/plugin_error_handler.cc`: Plugin error handling and blacklisting
- `headless/plugin_timeout_wrapper.h`: Timeout protection utilities
- `headless/headless_config.cc`: Configuration management
- `headless/README_VST_SUPPORT.md`: This documentation

#### Modified Files

- `headless/load_session.cc`: Enhanced with VST plugin support
- `headless/wscript`: Updated build configuration
- `libs/ardour/wscript`: Added new VST headless source files
- `libs/fst/fst.h`: Added headless function declarations
- `libs/ardour/ardour/linux_vst_support.h`: Added headless function declarations
- `libs/ardour/ardour/mac_vst_support.h`: Added headless function declarations

## Building

The VST headless support is automatically included when building with VST support enabled:

```bash
# Build with VST support
./waf configure --vst
./waf build
```

## Testing

### Basic Functionality Test

```bash
# Test basic VST plugin loading
./hardour -E -T 10000 /path/to/test/session test_session
```

### Error Handling Test

```bash
# Test with problematic plugins
./hardour -E -S /path/to/session/with/bad/plugins test_session
```

### Performance Test

```bash
# Test with large plugin chains
./hardour -E -T 60000 /path/to/session/with/many/plugins test_session
```

## Limitations

1. **GUI Plugins**: Plugins that require GUI interaction may not work properly
2. **Platform Dependencies**: VST support depends on platform-specific libraries
3. **Memory Usage**: Large plugin chains may consume significant memory
4. **Performance**: Plugin processing adds overhead to export time

## Troubleshooting

### Common Issues

1. **Plugin Loading Failures**: Check plugin compatibility and blacklist
2. **Timeout Errors**: Increase timeout value or check for problematic plugins
3. **Memory Issues**: Reduce plugin memory limit or use fewer plugins
4. **Build Errors**: Ensure VST support is properly configured

### Debug Information

Enable debug output to troubleshoot plugin issues:

```bash
./hardour -E -D plugin /path/to/session session_name
```

## Future Enhancements

1. **Plugin State Export**: Export plugin states for offline processing
2. **Plugin Preset Management**: Support for plugin preset loading
3. **Advanced Error Recovery**: More sophisticated error recovery mechanisms
4. **Performance Optimization**: Optimized plugin processing for headless operation
5. **Plugin Validation**: Pre-export plugin validation and testing

## Contributing

When contributing to VST headless support:

1. Test on multiple platforms (Windows, Linux, macOS)
2. Test with various VST plugin types
3. Ensure proper error handling and resource cleanup
4. Update documentation for new features
5. Add appropriate tests for new functionality
