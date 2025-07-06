# Plugin System Architecture

## Overview

Ardour's plugin system supports multiple plugin formats including VST, Audio Units (AU), and LV2. The system is designed to be extensible, secure, and performant, with special considerations for headless operation.

## Architecture Components

### Core Plugin System

```
libs/ardour/
├── plugin_manager.cc          # Main plugin management
├── plugin.cc                  # Base plugin interface
├── vst_plugin.cc             # VST plugin implementation
├── au_plugin.cc              # Audio Units implementation
├── lv2_plugin.cc             # LV2 plugin implementation
└── plugin_scan.cc            # Plugin discovery and scanning
```

### Headless Support

```
headless/
├── plugin_loader.cc          # Headless plugin loading
├── plugin_error_handler.cc   # Error handling for plugins
├── plugin_timeout_wrapper.h  # Timeout protection
└── headless_config.cc        # Headless configuration
```

### Platform-Specific Support

```
libs/ardour/
├── fst_headless.cc           # FST (Free VST) wrapper
├── linux_vst_support_headless.cc  # Linux VST support
└── mac_vst_support_headless.cc    # macOS VST support
```

## Plugin Types

### VST (Virtual Studio Technology)

- **VST2**: Legacy format, widely supported
- **VST3**: Modern format with improved features
- **FST**: Free VST wrapper for cross-platform compatibility

### Audio Units (AU)

- **macOS Native**: Apple's plugin format
- **Core Audio Integration**: Native macOS audio system
- **Performance**: Optimized for macOS

### LV2 (Linux VST2)

- **Linux Native**: Open-source plugin format
- **Extensible**: Modular design with extensions
- **Cross-platform**: Available on multiple platforms

## Plugin Lifecycle

### 1. Discovery

```cpp
// Plugin discovery process
class PluginScanner {
public:
    std::vector<PluginInfo> scan_directory(const std::string& path);
    std::vector<PluginInfo> scan_system_paths();

private:
    bool is_valid_plugin(const std::string& path);
    PluginInfo extract_plugin_info(const std::string& path);
};
```

### 2. Validation

```cpp
// Plugin validation
bool PluginManager::validate_plugin(const std::string& path) {
    // Check file exists and is readable
    if (!Glib::file_test(path, Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR)) {
        return false;
    }

    // Check architecture compatibility
    if (!is_architecture_compatible(path)) {
        return false;
    }

    // Check plugin format
    if (!is_supported_format(path)) {
        return false;
    }

    return true;
}
```

### 3. Loading

```cpp
// Plugin loading with error handling
std::shared_ptr<Plugin> PluginManager::load_plugin(const std::string& path) {
    try {
        // Validate plugin
        if (!validate_plugin(path)) {
            throw std::runtime_error("Invalid plugin: " + path);
        }

        // Load plugin binary
        auto plugin = load_plugin_binary(path);
        if (!plugin) {
            throw std::runtime_error("Failed to load plugin binary: " + path);
        }

        // Initialize plugin
        if (!plugin->initialize()) {
            throw std::runtime_error("Failed to initialize plugin: " + path);
        }

        return plugin;
    } catch (const std::exception& e) {
        PluginErrorHandler::handle_plugin_error(path, e.what());
        return nullptr;
    }
}
```

### 4. Initialization

```cpp
// Plugin initialization
bool VSTPlugin::initialize() {
    // Set up host information
    VSTHostInfo host_info;
    host_info.sample_rate = session_sample_rate;
    host_info.block_size = session_block_size;
    host_info.process_mode = VST_PROCESS_MODE_REALTIME;

    // Initialize plugin
    if (!vst_plugin->initialize(host_info)) {
        return false;
    }

    // Set up audio processing
    setup_audio_processing();

    return true;
}
```

### 5. Processing

```cpp
// Audio processing
void VSTPlugin::process(float* input, float* output, int frames) {
    // Prepare input/output buffers
    prepare_buffers(input, output, frames);

    // Process audio through plugin
    vst_plugin->process(frames);

    // Copy output back
    copy_output(output, frames);
}
```

### 6. Cleanup

```cpp
// Plugin cleanup
void VSTPlugin::cleanup() {
    // Deactivate plugin
    if (is_active) {
        deactivate();
    }

    // Release resources
    release_resources();

    // Unload plugin binary
    unload_binary();
}
```

## Headless Operation

### Headless Plugin Loading

```cpp
// Headless plugin loader
class HeadlessPluginLoader {
public:
    std::shared_ptr<Plugin> load_plugin_headless(const std::string& path) {
        // Set up headless environment
        setup_headless_environment();

        // Load plugin with timeout protection
        return TimeoutWrapper::execute_with_timeout(
            [this, &path]() { return load_plugin_internal(path); },
            DEFAULT_TIMEOUT_MS
        );
    }

private:
    std::shared_ptr<Plugin> load_plugin_internal(const std::string& path);
    void setup_headless_environment();
};
```

### Timeout Protection

```cpp
// Timeout wrapper for plugin operations
template<typename Func>
class TimeoutWrapper {
public:
    static bool execute_with_timeout(Func func, int timeout_ms) {
        std::future<bool> future = std::async(std::launch::async, func);

        if (future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
            // Operation timed out
            return false;
        }

        return future.get();
    }
};
```

### Error Handling

```cpp
// Plugin error handler
class PluginErrorHandler {
public:
    static void handle_plugin_crash(const std::string& plugin_name) {
        error << "Plugin crashed: " << plugin_name << endmsg;
        // Log crash information
        log_crash_info(plugin_name);
        // Notify monitoring system
        notify_crash(plugin_name);
    }

    static void handle_timeout(const std::string& plugin_name) {
        warning << "Plugin operation timed out: " << plugin_name << endmsg;
        // Force cleanup
        force_cleanup(plugin_name);
    }

    static void handle_memory_error(const std::string& plugin_name) {
        error << "Plugin memory error: " << plugin_name << endmsg;
        // Emergency cleanup
        emergency_cleanup(plugin_name);
    }
};
```

## Platform-Specific Implementation

### macOS Implementation

```cpp
#ifdef __APPLE__
class MacVSTPlugin : public VSTPlugin {
public:
    MacVSTPlugin(const std::string& path) : VSTPlugin(path) {
        // macOS-specific initialization
        setup_core_audio_integration();
    }

private:
    void setup_core_audio_integration();
    bool is_architecture_compatible(const std::string& path) {
        // Check for ARM64/x86_64 compatibility
        return check_architecture(path);
    }
};
#endif
```

### Linux Implementation

```cpp
#ifdef __linux__
class LinuxVSTPlugin : public VSTPlugin {
public:
    LinuxVSTPlugin(const std::string& path) : VSTPlugin(path) {
        // Linux-specific initialization
        setup_alsa_integration();
    }

private:
    void setup_alsa_integration();
    bool is_architecture_compatible(const std::string& path) {
        // Check for x86_64/ARM64 compatibility
        return check_architecture(path);
    }
};
#endif
```

## Security Considerations

### Plugin Sandboxing

```cpp
// Plugin sandboxing for security
class PluginSandbox {
public:
    static std::shared_ptr<Plugin> load_sandboxed_plugin(const std::string& path) {
        // Validate plugin integrity
        if (!validate_plugin_integrity(path)) {
            throw std::runtime_error("Plugin integrity check failed");
        }

        // Set up sandbox environment
        setup_sandbox_environment();

        // Load plugin in sandbox
        auto plugin = load_plugin_in_sandbox(path);

        // Monitor plugin behavior
        start_behavior_monitoring(plugin);

        return plugin;
    }

private:
    static bool validate_plugin_integrity(const std::string& path);
    static void setup_sandbox_environment();
    static std::shared_ptr<Plugin> load_plugin_in_sandbox(const std::string& path);
    static void start_behavior_monitoring(std::shared_ptr<Plugin> plugin);
};
```

### Input Validation

```cpp
// Comprehensive input validation
bool PluginManager::validate_plugin_path(const std::string& path) {
    // Check for path traversal
    if (path.find("..") != std::string::npos) {
        return false;
    }

    // Check for absolute paths outside allowed directories
    if (Glib::path_is_absolute(path)) {
        if (!is_in_allowed_directory(path)) {
            return false;
        }
    }

    // Check file extension
    if (!has_valid_extension(path)) {
        return false;
    }

    return true;
}
```

## Performance Optimization

### Plugin Caching

```cpp
// Plugin metadata caching
class PluginCache {
public:
    PluginInfo get_cached_info(const std::string& path) {
        auto it = cache.find(path);
        if (it != cache.end()) {
            // Check if cache is still valid
            if (is_cache_valid(path, it->second)) {
                return it->second;
            }
        }

        // Load and cache plugin info
        auto info = load_plugin_info(path);
        cache[path] = info;
        return info;
    }

private:
    std::map<std::string, PluginInfo> cache;
    bool is_cache_valid(const std::string& path, const PluginInfo& info);
    PluginInfo load_plugin_info(const std::string& path);
};
```

### Lazy Loading

```cpp
// Lazy plugin loading
class LazyPluginManager {
public:
    std::shared_ptr<Plugin> get_plugin(const std::string& name) {
        auto it = loaded_plugins.find(name);
        if (it == loaded_plugins.end()) {
            // Load plugin only when requested
            auto plugin = load_plugin(name);
            if (plugin) {
                loaded_plugins[name] = plugin;
            }
            return plugin;
        }
        return it->second;
    }

private:
    std::map<std::string, std::shared_ptr<Plugin>> loaded_plugins;
};
```

## Testing Framework

### Plugin Testing

```cpp
// Plugin testing framework
class PluginTestFramework {
public:
    static bool test_plugin_loading(const std::string& path) {
        try {
            auto plugin = load_test_plugin(path);
            if (!plugin) {
                return false;
            }

            // Test basic functionality
            if (!test_plugin_basic_functionality(plugin)) {
                return false;
            }

            // Test audio processing
            if (!test_plugin_audio_processing(plugin)) {
                return false;
            }

            return true;
        } catch (const std::exception& e) {
            test_error << "Plugin test failed: " << e.what() << endmsg;
            return false;
        }
    }

private:
    static std::shared_ptr<Plugin> load_test_plugin(const std::string& path);
    static bool test_plugin_basic_functionality(std::shared_ptr<Plugin> plugin);
    static bool test_plugin_audio_processing(std::shared_ptr<Plugin> plugin);
};
```

### Automated Testing

```bash
#!/bin/bash
# test_vst_support.sh

# Test VST plugin support
test_plugin() {
    local plugin_path="$1"
    echo "Testing plugin: $plugin_path"

    if ./ardour --headless --test-plugin "$plugin_path"; then
        echo "✅ Plugin test passed: $plugin_path"
        return 0
    else
        echo "❌ Plugin test failed: $plugin_path"
        return 1
    fi
}

# Test all plugins in directory
test_plugin_directory() {
    local dir="$1"
    echo "Testing plugins in directory: $dir"

    for plugin in "$dir"/*.vst "$dir"/*.dylib "$dir"/*.so; do
        if [ -f "$plugin" ]; then
            test_plugin "$plugin"
        fi
    done
}
```

## Configuration Management

### Plugin Configuration

```cpp
// Plugin configuration management
class PluginConfig {
public:
    struct Config {
        int default_timeout_ms = 5000;
        int max_plugins = 100;
        bool enable_sandboxing = true;
        std::vector<std::string> allowed_directories;
        std::vector<std::string> blocked_plugins;
    };

    static Config load_config() {
        Config config;
        // Load from configuration file
        load_config_from_file(config);
        return config;
    }

private:
    static void load_config_from_file(Config& config);
};
```

### Environment Configuration

```bash
# Plugin environment variables
export VST_PATH="/path/to/vst/plugins"
export VST3_PATH="/path/to/vst3/plugins"
export AU_PATH="/Library/Audio/Plug-Ins/Components"
export LV2_PATH="/usr/lib/lv2:/usr/local/lib/lv2"

# Headless configuration
export ARDOUR_HEADLESS=1
export ARDOUR_VST_TIMEOUT=5000
export ARDOUR_MAX_PLUGINS=100
```

## Future Development

### Planned Features

1. **VST3 Full Support**: Complete VST3 implementation
2. **Plugin Presets**: Save and load plugin configurations
3. **Batch Processing**: Process multiple files with plugin chains
4. **Remote Processing**: Support for remote plugin processing
5. **Plugin Validation**: Enhanced plugin integrity checking

### Integration Goals

1. **Session Management**: Full integration with Ardour sessions
2. **Automation**: Support for plugin parameter automation
3. **MIDI Support**: MIDI control of plugins
4. **Real-time Processing**: Low-latency real-time processing
5. **Plugin Chains**: Support for complex plugin chains
