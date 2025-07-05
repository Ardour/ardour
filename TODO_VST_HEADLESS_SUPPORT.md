# TODO: VST Plugin Support for Ardour Headless Export

## Overview

Currently, Ardour's headless export functionality (`hardour`) does not support VST plugin audio rendering. The headless export tool loads sessions and processes audio but bypasses or disables all plugins, resulting in 44-byte WAV files with no actual audio content.

This document outlines the implementation plan to add full VST plugin support to the headless export system.

## Current State Analysis

### Headless Export Architecture

The headless export system consists of:

- `headless/load_session.cc` - Main session loading and processing
- `headless/misc.cc` - Utility functions
- `headless/wscript` - Build configuration

### Current Limitations

1. **Plugin Bypassing**: The headless export uses `--disable-plugins` or `--bypass-plugins` flags
2. **No VST Loading**: VST plugins are not instantiated or processed during export
3. **Audio Engine Limitations**: The headless audio engine doesn't fully initialize plugin processing chains

### Plugin System Architecture

Ardour's plugin system is well-architected:

- `PluginManager` - Manages plugin discovery and caching
- `PluginInsert` - Handles plugin instantiation and processing
- `VSTPlugin`, `VST3Plugin` - VST-specific implementations
- Session state loading restores plugin configurations

## Implementation Plan

### Phase 1: Enable Plugin Loading in Headless Mode

#### 1.1 Modify Headless Session Loading

**File**: `headless/load_session.cc`

**Changes**:

- Remove default plugin bypassing/disabling
- Add command-line option to control plugin loading
- Ensure proper plugin manager initialization

```cpp
// Add to command-line options
{ "enable-plugins",     no_argument,       0, 'E' },
{ "plugin-timeout",     required_argument, 0, 'T' },

// Add to help text
<< "  -E, --enable-plugins        Enable VST plugin processing during export\n"
<< "  -T, --plugin-timeout <ms>   Plugin processing timeout in milliseconds\n"
```

#### 1.2 Update Build Configuration

**File**: `headless/wscript`

**Changes**:

- Ensure VST support libraries are linked
- Add VST-related compiler flags
- Include plugin manager dependencies

```python
# Add VST support to headless build
if bld.is_defined('WINDOWS_VST_SUPPORT'):
    obj.uselib += ' FST'
if bld.is_defined('LXVST_SUPPORT'):
    obj.uselib += ' LXVST'
if bld.is_defined('VST3_SUPPORT'):
    obj.uselib += ' VST3'
```

### Phase 2: Plugin Instantiation and Processing

#### 2.1 Modify Session Loading Process

**File**: `headless/load_session.cc`

**Changes**:

- Ensure plugin manager is properly initialized
- Load plugin cache and scan results
- Initialize VST host environments

```cpp
// After ARDOUR::init() call
if (enable_plugins) {
    // Initialize plugin manager
    PluginManager::instance().load_cache();

    // Initialize VST support
#ifdef WINDOWS_VST_SUPPORT
    if (Config->get_use_windows_vst() && fst_init(0)) {
        cerr << "Failed to initialize Windows VST support\n";
        exit(EXIT_FAILURE);
    }
#endif

#ifdef LXVST_SUPPORT
    if (Config->get_use_lxvst() && vstfx_init(0)) {
        cerr << "Failed to initialize Linux VST support\n";
        exit(EXIT_FAILURE);
    }
#endif
}
```

#### 2.2 Plugin State Restoration

**File**: `libs/ardour/session.cc`

**Changes**:

- Ensure plugin inserts are properly restored during session loading
- Handle plugin instantiation failures gracefully
- Add timeout mechanisms for plugin loading

```cpp
// In Session::load_state() or similar
if (enable_plugins) {
    // Set plugin loading timeout
    if (plugin_timeout_ms > 0) {
        // Implement timeout mechanism
    }

    // Ensure plugin inserts are instantiated
    // This may require modifying the session loading process
}
```

### Phase 3: Audio Processing Pipeline

#### 3.1 Modify Export Processing

**File**: `libs/ardour/session_export.cc`

**Changes**:

- Ensure plugin processing chains are active during export
- Handle plugin latency compensation
- Process plugin automation and parameters

```cpp
// In Session::start_audio_export()
if (enable_plugins) {
    // Ensure all plugin inserts are activated
    // Process plugin automation
    // Handle plugin latency
}
```

#### 3.2 Plugin Processing Integration

**File**: `libs/ardour/plugin_insert.cc`

**Changes**:

- Ensure plugin processing works in headless mode
- Handle plugin parameter automation
- Process plugin sidechains and MIDI

```cpp
// Ensure PluginInsert::run() works in headless context
// Handle plugin parameter changes during export
// Process plugin automation data
```

### Phase 4: Error Handling and Robustness

#### 4.1 Plugin Loading Failures

**Implementation**:

- Graceful handling of missing plugins
- Plugin blacklisting for problematic VSTs
- Fallback mechanisms for failed plugin loads

```cpp
// Add to headless session loading
try {
    // Attempt plugin instantiation
} catch (const std::exception& e) {
    cerr << "Plugin loading failed: " << e.what() << endl;
    if (strict_plugin_loading) {
        exit(EXIT_FAILURE);
    }
    // Continue with disabled plugins
}
```

#### 4.2 Timeout Mechanisms

**Implementation**:

- Plugin processing timeouts
- Deadlock detection
- Resource cleanup

```cpp
// Add timeout wrapper for plugin processing
class PluginTimeoutWrapper {
    // Implement timeout mechanism for plugin processing
    // Handle hanging plugins gracefully
};
```

### Phase 5: Configuration and Control

#### 5.1 Command-Line Interface

**Enhancements**:

- Plugin-specific enable/disable flags
- Plugin path configuration
- Processing quality settings

```cpp
// Add to command-line options
{ "vst-path",           required_argument, 0, 'V' },
{ "plugin-blacklist",   required_argument, 0, 'B' },
{ "strict-plugins",     no_argument,       0, 'S' },
```

#### 5.2 Configuration Files

**Implementation**:

- Headless-specific plugin configuration
- Plugin blacklist management
- Performance tuning options

```cpp
// Add headless-specific configuration
// Plugin timeout settings
// Memory usage limits
// Processing quality settings
```

## Technical Considerations

### Memory Management

- **Plugin Memory**: VST plugins can consume significant memory
- **Buffer Management**: Ensure proper buffer allocation for plugin processing
- **Resource Cleanup**: Proper cleanup of plugin instances

### Performance Optimization

- **Plugin Caching**: Cache plugin instances for repeated use
- **Processing Efficiency**: Optimize plugin processing for headless operation
- **Parallel Processing**: Consider parallel plugin processing where possible

### Platform Compatibility

- **Windows VST**: Ensure FST wrapper compatibility
- **Linux VST**: Handle LXVST environment properly
- **macOS VST**: Support for Mac VST plugins
- **VST3**: Full VST3 support implementation

### Security Considerations

- **Plugin Sandboxing**: Isolate plugin execution
- **Resource Limits**: Prevent plugins from consuming excessive resources
- **Error Isolation**: Prevent plugin crashes from affecting export

## Testing Strategy

### Unit Tests

1. **Plugin Loading Tests**: Test plugin discovery and loading
2. **Processing Tests**: Test plugin audio processing
3. **Error Handling Tests**: Test plugin failure scenarios

### Integration Tests

1. **Session Export Tests**: Test complete export with plugins
2. **Performance Tests**: Test export performance with various plugins
3. **Compatibility Tests**: Test with different VST plugin types

### Validation Tests

1. **Audio Quality Tests**: Ensure exported audio matches GUI export
2. **Parameter Tests**: Verify plugin parameters are correctly processed
3. **Automation Tests**: Test plugin automation during export

## Implementation Timeline

### Week 1-2: Phase 1

- Modify headless session loading
- Update build configuration
- Basic plugin loading infrastructure

### Week 3-4: Phase 2

- Plugin instantiation and state restoration
- Basic plugin processing integration

### Week 5-6: Phase 3

- Audio processing pipeline integration
- Plugin automation and parameter handling

### Week 7-8: Phase 4

- Error handling and robustness
- Timeout mechanisms and resource management

### Week 9-10: Phase 5

- Configuration and control interfaces
- Testing and validation

## Success Criteria

1. **Functional**: Headless export produces audio with VST plugins
2. **Performance**: Export performance is comparable to GUI export
3. **Reliability**: Robust error handling and resource management
4. **Compatibility**: Support for major VST plugin formats
5. **Quality**: Exported audio matches GUI export quality

## Risk Assessment

### High Risk

- **Plugin Compatibility**: Some VST plugins may not work in headless mode
- **Performance**: Plugin processing may significantly slow export
- **Memory Usage**: Large plugin chains may exceed memory limits

### Medium Risk

- **Platform Differences**: VST behavior may vary across platforms
- **Resource Management**: Complex plugin resource management
- **Error Handling**: Comprehensive error handling for all failure modes

### Low Risk

- **Build Integration**: Adding VST support to headless build
- **Configuration**: Plugin configuration and control interfaces

## Conclusion

Adding VST plugin support to Ardour's headless export is a significant but achievable enhancement. The existing plugin architecture provides a solid foundation, and the implementation can be done incrementally with proper testing and validation.

The key success factors are:

1. Proper integration with existing plugin architecture
2. Robust error handling and resource management
3. Comprehensive testing across different plugin types
4. Performance optimization for headless operation

This enhancement will significantly improve Ardour's headless export capabilities and enable automated audio processing workflows with VST plugins.
