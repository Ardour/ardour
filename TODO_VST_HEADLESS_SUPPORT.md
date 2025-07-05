# TODO: VST Plugin Support for Ardour Headless Export

## Overview

Currently, Ardour's headless export functionality (`hardour`) does not support VST plugin audio rendering. The headless export tool loads sessions and processes audio but bypasses or disables all plugins, resulting in 44-byte WAV files with no actual audio content.

This document outlines the implementation plan to add full VST plugin support to the headless export system, incorporating comprehensive technical analysis and addressing identified challenges.

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
4. **Plugin Manager Initialization Gap**: Headless mode only refreshes from cache, not full plugin discovery

### Plugin System Architecture

Ardour's plugin system is well-architected:

- `PluginManager` - Manages plugin discovery and caching
- `PluginInsert` - Handles plugin instantiation and processing
- `VSTPlugin`, `VST3Plugin` - VST-specific implementations
- Session state loading restores plugin configurations

## Technical Challenges Identified

### Critical Issues

#### 1. Plugin Manager Initialization Gap

**Problem**: While `PluginManager::instance()` is called during `ARDOUR::init()`, the actual plugin discovery and loading is deferred in headless mode.

**Evidence from `libs/ardour/globals.cc:807-815`**:

```cpp
if (start_cnt == 0) {
    if (!running_from_gui) {
        /* find plugins, but only using the existing cache (i.e. do
         * not discover new ones. GUIs are responsible for
         * invoking this themselves after the engine is
         * started, with whatever options they want.
         */
        ARDOUR::PluginManager::instance ().refresh (true);
    }
}
```

**Impact**: Headless mode only refreshes from cache (`refresh(true)`), but doesn't perform full plugin discovery. This means:

- New VST plugins won't be found
- Plugin cache might be stale or empty
- VST host environments aren't properly initialized

#### 2. VST Host Environment Dependencies

**Problem**: VST plugins require specific host environment setup that may not work in headless mode.

**Evidence from VST plugin files**:

- `libs/ardour/windows_vst_plugin.cc:32-35` - FST initialization
- `libs/ardour/lxvst_plugin.cc:32-35` - LXVST initialization
- `libs/ardour/mac_vst_plugin.cc:32-35` - MacVST initialization

**Impact**: These initializations assume GUI context and may fail in headless mode due to:

- Missing X11 display (Linux VST)
- Missing window handles (Windows VST)
- Missing Core Graphics context (Mac VST)

#### 3. Session State Loading Complexity

**Problem**: Plugin state restoration during session loading is more complex than initially assessed.

**Evidence from `libs/ardour/session_state.cc:2090-2120`**:

```cpp
if ((child = find_named_node (node, "IOPlugins"))) {
    RCUWriter<IOPlugList> writer (_io_plugins);
    std::shared_ptr<IOPlugList> iopl = writer.get_copy ();
    for (XMLNodeList::const_iterator n = child->children ().begin (); n != child->children ().end (); ++n) {
        std::shared_ptr<IOPlug> iop = std::make_shared<IOPlug>(*this);
        if (0 == iop->set_state (**n, version)) {
            iopl->push_back (iop);
            iop->LatencyChanged.connect_same_thread (*this, std::bind (&Session::update_latency_compensation, this, true, false));
        } else {
            /* TODO Unknown I/O Plugin, retain state */
        }
    }
}
```

**Impact**: Plugin instantiation during session loading involves complex state management, error handling, and thread safety.

### Specific Code-Level Challenges

#### 1. Plugin Processing Integration

**Problem**: The export processing pipeline in `libs/ardour/export_graph_builder.cc` doesn't directly integrate with plugin processing chains.

**Evidence**: The export system uses `ExportChannelPtr` and `PortExportChannel` which read from audio ports, but don't directly invoke plugin processing chains.

**Required Changes**:

- Modify `RouteExportChannel::read()` to ensure plugin processing
- Integrate with `PluginInsert::run()` during export
- Handle plugin latency compensation in export context

#### 2. Memory and Resource Management

**Problem**: VST plugins can consume significant memory and resources that need careful management in headless mode.

**Evidence from `libs/ardour/plugin_manager.cc:200-240`**:

```cpp
#if defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT || defined VST3_SUPPORT
    // source-tree (ardev, etc)
    PBD::Searchpath vstsp(Glib::build_filename(ARDOUR::ardour_dll_directory(), "fst"));
```

**Impact**: VST scanner processes and plugin instances need proper cleanup and resource limits.

#### 3. Threading and Real-time Constraints

**Problem**: Plugin processing in headless mode must handle threading correctly without GUI event loops.

**Evidence from `libs/ardour/session_export.cc:219-240`**:

```cpp
if (realtime) {
    Glib::Threads::Mutex::Lock lm (AudioEngine::instance()->process_lock ());
    _export_rolling = true;
    _realtime_export = true;
    export_status->stop = false;
    process_function = &Session::process_export_fw;
```

**Impact**: Plugin processing must work in both realtime and non-realtime export modes with proper thread safety.

## Implementation Plan (Revised)

### Phase 1: Plugin Manager Initialization (Enhanced)

#### 1.1 Modify Headless Session Loading

**File**: `headless/load_session.cc`

**Changes**:

- Remove default plugin bypassing/disabling
- Add command-line option to control plugin loading
- Ensure proper plugin manager initialization with full discovery

```cpp
// Add to command-line options
{ "enable-plugins",     no_argument,       0, 'E' },
{ "plugin-timeout",     required_argument, 0, 'T' },
{ "strict-plugins",     no_argument,       0, 'S' },
{ "vst-path",           required_argument, 0, 'V' },
{ "plugin-blacklist",   required_argument, 0, 'B' },

// Add to help text
<< "  -E, --enable-plugins        Enable VST plugin processing during export\n"
<< "  -T, --plugin-timeout <ms>   Plugin processing timeout in milliseconds\n"
<< "  -S, --strict-plugins        Exit on plugin loading failure\n"
<< "  -V, --vst-path <path>       Additional VST plugin search path\n"
<< "  -B, --plugin-blacklist <f>  Plugin blacklist file\n"
```

#### 1.2 Enhanced Plugin Manager Initialization

**File**: `headless/load_session.cc`

**Changes**:

```cpp
// After ARDOUR::init() call
if (enable_plugins) {
    // Force full plugin discovery, not just cache refresh
    PluginManager::instance().refresh(false);

    // Initialize VST host environments with headless context
#ifdef WINDOWS_VST_SUPPORT
    if (Config->get_use_windows_vst()) {
        // Create headless VST host context
        if (fst_init_headless(0)) {
            cerr << "Failed to initialize Windows VST support in headless mode\n";
            exit(EXIT_FAILURE);
        }
    }
#endif

#ifdef LXVST_SUPPORT
    if (Config->get_use_lxvst()) {
        // Create headless LXVST host context
        if (vstfx_init_headless(0)) {
            cerr << "Failed to initialize Linux VST support in headless mode\n";
            exit(EXIT_FAILURE);
        }
    }
#endif

#ifdef MACVST_SUPPORT
    if (Config->get_use_macvst()) {
        // Create headless MacVST host context
        if (mac_vst_init_headless(0)) {
            cerr << "Failed to initialize Mac VST support in headless mode\n";
            exit(EXIT_FAILURE);
        }
    }
#endif
}
```

#### 1.3 Update Build Configuration

**File**: `headless/wscript`

**Changes**:

```python
# Add VST support to headless build
if bld.is_defined('WINDOWS_VST_SUPPORT'):
    obj.uselib += ' FST'
if bld.is_defined('LXVST_SUPPORT'):
    obj.uselib += ' LXVST'
if bld.is_defined('VST3_SUPPORT'):
    obj.uselib += ' VST3'
if bld.is_defined('MACVST_SUPPORT'):
    obj.uselib += ' MACVST'

# Add plugin manager dependencies
obj.use += [ 'libardour' ]
```

### Phase 2: VST Host Environment Support (New)

#### 2.1 Create Headless VST Host Contexts

**File**: `libs/ardour/fst_headless.cc` (new)

**Implementation**:

```cpp
#include "fst.h"

int fst_init_headless(int flags) {
    // Initialize FST for headless operation
    // Create dummy window handles or use NULL contexts
    // Set up headless-specific callbacks

    // Initialize VST host environment without GUI dependencies
    return fst_init(flags | FST_FLAG_HEADLESS);
}

void fst_exit_headless() {
    // Clean up headless VST host environment
    fst_exit();
}
```

**File**: `libs/ardour/linux_vst_support_headless.cc` (new)

**Implementation**:

```cpp
#include "ardour/linux_vst_support.h"

int vstfx_init_headless(int flags) {
    // Initialize LXVST for headless operation
    // Set up dummy X11 display or use alternative context
    // Configure headless-specific VST host callbacks

    return vstfx_init(flags | VSTFX_FLAG_HEADLESS);
}

void vstfx_exit_headless() {
    // Clean up headless LXVST environment
    vstfx_exit();
}
```

#### 2.2 Plugin Loading with Error Handling

**File**: `headless/plugin_loader.cc` (new)

**Implementation**:

```cpp
class PluginLoader {
private:
    std::chrono::milliseconds timeout_ms;
    std::atomic<bool> cancelled{false};

public:
    PluginLoader(int timeout_ms = 30000) : timeout_ms(timeout_ms) {}

    template<typename F>
    auto execute_with_timeout(F&& func) -> decltype(func()) {
        auto future = std::async(std::launch::async, std::forward<F>(func));

        if (future.wait_for(timeout_ms) == std::future_status::timeout) {
            cancelled = true;
            throw std::runtime_error("Plugin processing timeout");
        }

        return future.get();
    }

    bool load_plugin_with_retry(PluginInfoPtr plugin_info, Session& session) {
        for (int retry = 0; retry < 3; ++retry) {
            try {
                PluginLoader loader(5000); // 5 second timeout per attempt
                return loader.execute_with_timeout([&]() {
                    return plugin_info->load(session) != nullptr;
                });
            } catch (const std::exception& e) {
                cerr << "Plugin loading attempt " << (retry + 1) << " failed: " << e.what() << endl;
                if (retry == 2) {
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return false;
    }
};
```

### Phase 3: Session Loading with Plugin Support (Enhanced)

#### 3.1 Enhanced Session State Loading

**File**: `libs/ardour/session.cc`

**Changes**:

```cpp
// In Session::load_state() or similar
if (enable_plugins) {
    // Add plugin loading timeout mechanism
    PluginLoadingTimeout timeout(plugin_timeout_ms);

    // Ensure plugin inserts are instantiated with error handling
    for (auto& processor : processors) {
        if (auto plugin_insert = std::dynamic_pointer_cast<PluginInsert>(processor)) {
            try {
                PluginLoader loader;
                bool success = loader.load_plugin_with_retry(plugin_insert->plugin_info(), *this);

                if (!success) {
                    if (strict_plugin_loading) {
                        throw std::runtime_error("Plugin loading failed: " + plugin_insert->plugin_info()->name);
                    }
                    // Log error and continue with disabled plugin
                    cerr << "Plugin loading failed: " << plugin_insert->plugin_info()->name << endl;
                    plugin_insert->disable();
                }
            } catch (const std::exception& e) {
                if (strict_plugin_loading) {
                    throw;
                }
                cerr << "Plugin loading failed: " << e.what() << endl;
                plugin_insert->disable();
            }
        }
    }
}
```

#### 3.2 Plugin State Restoration

**File**: `libs/ardour/plugin_insert.cc`

**Changes**:

```cpp
// In PluginInsert::set_state()
if (enable_plugins) {
    // Set plugin loading timeout
    if (plugin_timeout_ms > 0) {
        // Implement timeout mechanism for plugin loading
        PluginLoader loader(plugin_timeout_ms);
        return loader.execute_with_timeout([&]() {
            return set_state_internal(node, version);
        });
    }
}
```

### Phase 4: Export Processing Pipeline Integration (Critical)

#### 4.1 Modify Export Processing

**File**: `libs/ardour/export_channel.cc`

**Changes**:

```cpp
// In RouteExportChannel::read()
void RouteExportChannel::read (Buffer const*& buf, samplecnt_t samples) const
{
    assert (_processor);

    // Ensure plugin processing chain is active
    if (enable_plugins) {
        // Process through plugin chain before capture
        _processor->process_plugins(samples);
    }

    Buffer const& buffer = _processor->get_capture_buffers ().get_available (_type, _channel);
    buf = &buffer;
}
```

#### 4.2 Plugin Processing Integration

**File**: `libs/ardour/plugin_insert.cc`

**Changes**:

```cpp
// Ensure PluginInsert::run() works in headless context
// Handle plugin parameter changes during export
// Process plugin automation data

void PluginInsert::run (BufferSet& bufs, samplepos_t start, samplepos_t end, double speed, pframes_t nframes, bool)
{
    if (!enable_plugins || !_active) {
        return;
    }

    // Add timeout protection for plugin processing
    if (plugin_timeout_ms > 0) {
        PluginLoader loader(plugin_timeout_ms);
        try {
            loader.execute_with_timeout([&]() {
                run_internal(bufs, start, end, speed, nframes);
            });
        } catch (const std::exception& e) {
            cerr << "Plugin processing timeout: " << e.what() << endl;
            // Handle timeout gracefully
        }
    } else {
        run_internal(bufs, start, end, speed, nframes);
    }
}
```

### Phase 5: Error Handling and Robustness (Enhanced)

#### 5.1 Plugin Loading Failures

**File**: `headless/plugin_error_handler.cc` (new)

**Implementation**:

```cpp
class PluginErrorHandler {
private:
    std::set<std::string> blacklisted_plugins;
    std::map<std::string, int> plugin_failure_count;

public:
    bool should_retry_plugin(const std::string& plugin_name) {
        if (blacklisted_plugins.find(plugin_name) != blacklisted_plugins.end()) {
            return false;
        }

        auto it = plugin_failure_count.find(plugin_name);
        return it == plugin_failure_count.end() || it->second < 3;
    }

    void record_plugin_failure(const std::string& plugin_name) {
        plugin_failure_count[plugin_name]++;
        if (plugin_failure_count[plugin_name] >= 3) {
            blacklisted_plugins.insert(plugin_name);
            cerr << "Plugin blacklisted due to repeated failures: " << plugin_name << endl;
        }
    }

    void load_blacklist_from_file(const std::string& filename) {
        std::ifstream file(filename);
        std::string line;
        while (std::getline(file, line)) {
            blacklisted_plugins.insert(line);
        }
    }
};
```

#### 5.2 Timeout Mechanisms

**File**: `headless/plugin_timeout_wrapper.h` (new)

**Implementation**:

```cpp
class PluginTimeoutWrapper {
private:
    std::chrono::milliseconds timeout_ms;
    std::atomic<bool> cancelled{false};

public:
    PluginTimeoutWrapper(int timeout_ms = 30000) : timeout_ms(timeout_ms) {}

    template<typename F>
    auto execute_with_timeout(F&& func) -> decltype(func()) {
        auto future = std::async(std::launch::async, std::forward<F>(func));

        if (future.wait_for(timeout_ms) == std::future_status::timeout) {
            cancelled = true;
            throw std::runtime_error("Plugin processing timeout");
        }

        return future.get();
    }

    void cancel() {
        cancelled = true;
    }

    bool is_cancelled() const {
        return cancelled;
    }
};
```

### Phase 6: Configuration and Control (Enhanced)

#### 6.1 Command-Line Interface

**File**: `headless/load_session.cc`

**Enhancements**:

```cpp
// Add to command-line options
{ "vst-path",           required_argument, 0, 'V' },
{ "plugin-blacklist",   required_argument, 0, 'B' },
{ "strict-plugins",     no_argument,       0, 'S' },
{ "plugin-memory-limit", required_argument, 0, 'M' },
{ "plugin-threads",     required_argument, 0, 't' },
```

#### 6.2 Configuration Files

**File**: `headless/headless_config.cc` (new)

**Implementation**:

```cpp
class HeadlessConfig {
private:
    std::string config_file;

public:
    struct PluginConfig {
        bool enable_plugins = false;
        int plugin_timeout_ms = 30000;
        bool strict_plugin_loading = false;
        std::string vst_path;
        std::string plugin_blacklist_file;
        size_t plugin_memory_limit_mb = 1024;
        int plugin_threads = 1;
    };

    PluginConfig load_config() {
        PluginConfig config;
        // Load from config file or environment variables
        return config;
    }

    void save_config(const PluginConfig& config) {
        // Save configuration to file
    }
};
```

## Alternative Implementation Approaches

### Approach 1: Hybrid GUI-Assisted Plugin Loading

**Description**: Use the GUI to load and configure plugins, then export headless.

**Advantages**:

- Simpler implementation
- Leverages existing GUI plugin management
- Reduces headless complexity

**Disadvantages**:

- Requires GUI session
- Less automated
- Not fully headless

### Approach 2: Plugin State Export

**Description**: Export plugin states to a format that can be loaded headless.

**Advantages**:

- Decouples plugin loading from export
- More reliable
- Better error handling

**Disadvantages**:

- Additional complexity
- State synchronization issues
- Limited to supported plugin types

### Approach 3: Limited Plugin Support

**Description**: Support only LV2 and LADSPA plugins initially (no VST).

**Advantages**:

- Much simpler implementation
- More reliable
- Faster development

**Disadvantages**:

- Limited plugin support
- Doesn't solve VST problem
- May not meet requirements

## Risk Assessment (Updated)

### High Risk

- **VST Host Environment**: VST plugins may fail to initialize in headless mode due to missing GUI context
- **Plugin Compatibility**: Some VST plugins may not work without GUI event loops
- **Memory Management**: Large plugin chains could exceed memory limits in headless mode
- **Threading Issues**: Plugin processing in headless mode may have race conditions

### Medium Risk

- **Performance**: Plugin processing may significantly slow export performance
- **Error Handling**: Comprehensive error handling for all plugin failure modes
- **Resource Cleanup**: Proper cleanup of plugin instances and VST host environments
- **Platform Differences**: VST behavior may vary across platforms

### Low Risk

- **Build Integration**: Adding VST support to headless build
- **Configuration**: Plugin configuration and control interfaces
- **Documentation**: Updating documentation and user guides

## Implementation Timeline (Revised)

### Week 1-2: Phase 1 - Plugin Manager Initialization

- Modify headless session loading
- Update build configuration
- Basic plugin loading infrastructure
- **Risk**: Medium - VST host environment setup

### Week 3-4: Phase 2 - VST Host Environment Support

- Create headless VST host contexts
- Implement plugin loading with error handling
- **Risk**: High - VST initialization in headless mode

### Week 5-6: Phase 3 - Session Loading with Plugin Support

- Enhanced session state loading
- Plugin state restoration
- **Risk**: Medium - Complex state management

### Week 7-8: Phase 4 - Export Processing Pipeline Integration

- Modify export processing
- Plugin processing integration
- **Risk**: High - Core audio processing changes

### Week 9-10: Phase 5 - Error Handling and Robustness

- Plugin loading failures
- Timeout mechanisms
- **Risk**: Medium - Comprehensive error handling

### Week 11-12: Phase 6 - Configuration and Control

- Command-line interface
- Configuration files
- Testing and validation
- **Risk**: Low - Configuration management

## Success Criteria (Updated)

1. **Functional**: Headless export produces audio with VST plugins
2. **Performance**: Export performance is comparable to GUI export
3. **Reliability**: Robust error handling and resource management
4. **Compatibility**: Support for major VST plugin formats
5. **Quality**: Exported audio matches GUI export quality
6. **Stability**: No crashes or hangs during plugin processing
7. **Resource Management**: Proper memory and resource cleanup

## Testing Strategy (Enhanced)

### Unit Tests

1. **Plugin Loading Tests**: Test plugin discovery and loading
2. **Processing Tests**: Test plugin audio processing
3. **Error Handling Tests**: Test plugin failure scenarios
4. **Timeout Tests**: Test plugin processing timeouts
5. **Memory Tests**: Test memory usage and cleanup

### Integration Tests

1. **Session Export Tests**: Test complete export with plugins
2. **Performance Tests**: Test export performance with various plugins
3. **Compatibility Tests**: Test with different VST plugin types
4. **Stress Tests**: Test with large plugin chains
5. **Error Recovery Tests**: Test recovery from plugin failures

### Validation Tests

1. **Audio Quality Tests**: Ensure exported audio matches GUI export
2. **Parameter Tests**: Verify plugin parameters are correctly processed
3. **Automation Tests**: Test plugin automation during export
4. **Cross-Platform Tests**: Test on different operating systems
5. **Memory Leak Tests**: Ensure no memory leaks during export

## Conclusion

Adding VST plugin support to Ardour's headless export is a **significant technical challenge** that requires careful consideration of multiple complex factors. The implementation is **achievable** but requires **substantial additional work** beyond the initial assessment.

The key success factors are:

1. **Proper VST host environment setup** for headless operation
2. **Robust error handling and resource management**
3. **Comprehensive testing across different plugin types**
4. **Performance optimization for headless operation**
5. **Careful consideration of threading and real-time constraints**

**Recommendation**: Start with LV2/LADSPA plugin support first, then tackle VST support as a separate, more complex phase. Consider the hybrid approach for initial implementation to reduce risk and complexity.

This enhancement will significantly improve Ardour's headless export capabilities and enable automated audio processing workflows with VST plugins, but should be approached with appropriate caution and thorough testing.
