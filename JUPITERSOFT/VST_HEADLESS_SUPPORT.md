# VST Headless Support

## Overview

VST (Virtual Studio Technology) headless support enables Ardour to load and process VST plugins without requiring a graphical user interface. This is essential for server-side audio processing, automated workflows, and environments where GUI is not available or desired.

## Architecture

### Headless Components

```
headless/                    # Headless-specific code
├── plugin_loader.cc         # VST plugin loading and management
├── plugin_error_handler.cc  # Error handling for plugin failures
├── plugin_timeout_wrapper.h # Timeout protection for plugin operations
├── headless_config.cc       # Configuration management
└── test_vst_support.sh      # Testing script for VST functionality
```

### Core Libraries

```
libs/ardour/
├── fst_headless.cc          # FST (Free VST) headless implementation
├── linux_vst_support_headless.cc  # Linux-specific VST support
└── mac_vst_support_headless.cc    # macOS-specific VST support
```

## Platform Support

### macOS

- **VST2**: Supported via FST (Free VST) wrapper
- **VST3**: Native support through VST3 SDK
- **AU**: Audio Units support (macOS native)
- **Architecture**: ARM64 (Apple Silicon) and x86_64

### Linux

- **VST2**: Supported via FST wrapper
- **VST3**: Native support through VST3 SDK
- **LV2**: Native Linux VST alternative
- **Architecture**: x86_64 and ARM64

### Windows

- **VST2**: Native support
- **VST3**: Native support through VST3 SDK
- **Architecture**: x86_64

## Implementation Details

### Plugin Loading Process

1. **Discovery**: Scan plugin directories for VST files
2. **Validation**: Check plugin compatibility and architecture
3. **Loading**: Load plugin binary into memory
4. **Initialization**: Initialize plugin with host information
5. **Configuration**: Set up audio processing parameters

### Error Handling

```cpp
// Example from plugin_error_handler.cc
class PluginErrorHandler {
public:
    static void handle_plugin_crash(const std::string& plugin_name);
    static void handle_timeout(const std::string& plugin_name);
    static void handle_memory_error(const std::string& plugin_name);
};
```

### Timeout Protection

```cpp
// Example from plugin_timeout_wrapper.h
template<typename Func>
class TimeoutWrapper {
public:
    static bool execute_with_timeout(Func func, int timeout_ms);
    static void set_default_timeout(int ms);
};
```

## Configuration

### Plugin Directories

```bash
# Default VST search paths
/usr/lib/vst/              # System VST directory
/usr/local/lib/vst/        # Local VST directory
~/Library/Audio/Plug-Ins/VST/  # macOS user VST directory
```

### Environment Variables

```bash
# VST plugin paths
export VST_PATH="/path/to/vst/plugins"
export VST3_PATH="/path/to/vst3/plugins"

# Headless configuration
export ARDOUR_HEADLESS=1
export ARDOUR_VST_TIMEOUT=5000  # 5 second timeout
```

## Usage

### Command Line Interface

```bash
# Load session with VST plugins
./ardour --headless --session /path/to/session

# Process audio file with VST chain
./ardour --headless --input input.wav --output output.wav --vst-chain "plugin1,plugin2"

# Test VST plugin compatibility
./test_vst_support.sh /path/to/plugin.vst
```

### API Usage

```cpp
// Example: Loading VST plugin in headless mode
#include "ardour/plugin_manager.h"
#include "ardour/plugin.h"

ARDOUR::PluginManager pm;
std::shared_ptr<ARDOUR::Plugin> plugin = pm.load_plugin("/path/to/plugin.vst");

if (plugin) {
    plugin->activate();
    // Process audio...
    plugin->deactivate();
}
```

## Testing

### Automated Testing

```bash
# Run VST support tests
./test_vst_support.sh

# Test specific plugin
./test_vst_support.sh /path/to/plugin.vst

# Test plugin directory
./test_vst_support.sh /path/to/plugin/directory
```

### Manual Testing

1. **Plugin Discovery**: Verify plugins are found in search paths
2. **Loading**: Test plugin loading without crashes
3. **Processing**: Verify audio processing works correctly
4. **Error Handling**: Test timeout and error recovery
5. **Performance**: Measure processing overhead

## Known Issues

### 1. Plugin Compatibility

**Issue**: Some VST plugins require GUI for initialization
**Status**: 🔄 In Progress - Working on headless initialization
**Workaround**: Use plugins that support headless operation

### 2. Architecture Mismatch

**Issue**: x86_64 plugins on ARM64 systems
**Status**: ✅ Fixed - Rosetta 2 translation on macOS
**Impact**: Performance overhead on Apple Silicon

### 3. Memory Management

**Issue**: Plugin memory leaks in headless mode
**Status**: 🔄 Ongoing - Improved cleanup procedures
**Impact**: Potential memory growth over time

### 4. Timeout Handling

**Issue**: Some plugins hang during initialization
**Status**: ✅ Fixed - Timeout wrapper implementation
**Impact**: Prevents system hangs

## Development Guidelines

### Adding New VST Support

1. **Platform Detection**: Use `#ifdef __APPLE__` for macOS-specific code
2. **Error Handling**: Always implement proper error handling
3. **Timeout Protection**: Use timeout wrapper for plugin operations
4. **Memory Management**: Ensure proper cleanup of plugin resources
5. **Testing**: Add tests for new functionality

### Code Style

```cpp
// Use consistent naming conventions
class VSTHeadlessManager {
public:
    static std::shared_ptr<Plugin> load_plugin(const std::string& path);
    static void unload_plugin(std::shared_ptr<Plugin> plugin);

private:
    static bool validate_plugin(const std::string& path);
    static void handle_plugin_error(const std::string& plugin_name, const std::string& error);
};
```

### Error Handling Patterns

```cpp
// Always use try-catch for plugin operations
try {
    auto plugin = load_plugin(plugin_path);
    if (!plugin) {
        throw std::runtime_error("Failed to load plugin: " + plugin_path);
    }
    // Process plugin...
} catch (const std::exception& e) {
    PluginErrorHandler::handle_plugin_error(plugin_path, e.what());
}
```

## Performance Considerations

### Optimization Strategies

1. **Lazy Loading**: Load plugins only when needed
2. **Caching**: Cache plugin metadata and configurations
3. **Parallel Processing**: Process multiple plugins in parallel
4. **Memory Pooling**: Reuse memory buffers for audio processing

### Monitoring

```cpp
// Example: Performance monitoring
class PluginPerformanceMonitor {
public:
    static void start_timing(const std::string& plugin_name);
    static void end_timing(const std::string& plugin_name);
    static void report_performance();
};
```

## Security Considerations

### Plugin Sandboxing

- **Isolation**: Run plugins in separate processes when possible
- **Validation**: Validate plugin binaries before loading
- **Permissions**: Limit plugin access to system resources
- **Monitoring**: Monitor plugin behavior for suspicious activity

### Best Practices

1. **Validate Paths**: Always validate plugin file paths
2. **Limit Permissions**: Run with minimal required permissions
3. **Monitor Resources**: Track memory and CPU usage
4. **Log Operations**: Log all plugin operations for debugging

## Future Development

### Planned Features

1. **VST3 Full Support**: Complete VST3 implementation
2. **Plugin Presets**: Save and load plugin configurations
3. **Batch Processing**: Process multiple files with VST chains
4. **Remote Processing**: Support for remote VST processing

### Integration Goals

1. **Session Management**: Full integration with Ardour sessions
2. **Automation**: Support for plugin parameter automation
3. **MIDI Support**: MIDI control of VST plugins
4. **Real-time Processing**: Low-latency real-time processing

## Resources

### Documentation

- [VST SDK Documentation](https://developer.steinberg.help/)
- [FST Documentation](https://github.com/FreeStudio/fst)
- [Ardour Plugin Development](https://ardour.org/development.html)

### Community

- [Ardour Plugin Development Forum](https://discourse.ardour.org/c/development/plugins)
- [VST Development Community](https://www.kvraudio.com/forum/)

### Tools

- [VST Plugin Validator](https://github.com/steinbergmedia/vst3_pluginterfaces)
- [Plugin Testing Framework](https://github.com/Ardour/ardour/tree/master/headless)
