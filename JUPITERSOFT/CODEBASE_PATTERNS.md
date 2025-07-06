# Codebase Patterns & Architecture

## Overall Architecture

### Modular Design

Ardour follows a modular architecture with clear separation of concerns:

- **Core Engine**: Audio processing and session management
- **GUI Layer**: User interface (GTK/YTK)
- **Plugin System**: VST, AU, LV2 plugin support
- **Backend Layer**: Audio backends (JACK, CoreAudio, ALSA)
- **Utility Libraries**: Common functionality (PBD, Canvas, etc.)

### Library Structure

```
libs/
├── ardour/          # Core audio engine
├── pbd/             # Platform-independent base library
├── canvas/          # Drawing and UI components
├── evoral/          # MIDI and control data
├── midi++2/         # MIDI handling
├── gtkmm2ext/       # GTK extensions
├── tk/              # YTK/YDK touch interface
└── plugins/         # Built-in plugins
```

## Coding Conventions

### C++ Style Guidelines

#### Naming Conventions

```cpp
// Classes: PascalCase
class PluginManager;
class AudioEngine;

// Functions: camelCase
void loadPlugin(const std::string& path);
bool isValidPlugin(const std::string& name);

// Variables: camelCase
std::string pluginPath;
int sampleRate;

// Constants: UPPER_SNAKE_CASE
const int MAX_PLUGINS = 100;
const std::string DEFAULT_TIMEOUT = "5000";

// Member variables: m_camelCase (if used)
class Plugin {
private:
    std::string m_name;
    int m_id;
};
```

#### File Organization

```cpp
// Header file: plugin_manager.h
#ifndef __ardour_plugin_manager_h__
#define __ardour_plugin_manager_h__

#include <string>
#include <memory>
#include "ardour/plugin.h"

namespace ARDOUR {

class PluginManager {
public:
    PluginManager();
    ~PluginManager();

    std::shared_ptr<Plugin> load_plugin(const std::string& path);
    bool unload_plugin(std::shared_ptr<Plugin> plugin);

private:
    bool validate_plugin(const std::string& path);
    void handle_error(const std::string& error);
};

} // namespace ARDOUR

#endif // __ardour_plugin_manager_h__
```

#### Implementation file: plugin_manager.cc

```cpp
#include "ardour/plugin_manager.h"
#include "pbd/debug.h"
#include "pbd/error.h"

using namespace PBD;

namespace ARDOUR {

PluginManager::PluginManager() {
    // Constructor implementation
}

PluginManager::~PluginManager() {
    // Destructor implementation
}

std::shared_ptr<Plugin>
PluginManager::load_plugin(const std::string& path) {
    if (!validate_plugin(path)) {
        error << "Invalid plugin: " << path << endmsg;
        return nullptr;
    }

    // Implementation...
    return plugin;
}

} // namespace ARDOUR
```

### Error Handling Patterns

#### Exception Handling

```cpp
// Use try-catch for operations that can fail
try {
    auto plugin = load_plugin(plugin_path);
    if (!plugin) {
        throw std::runtime_error("Failed to load plugin: " + plugin_path);
    }
    // Process plugin...
} catch (const std::exception& e) {
    error << "Plugin error: " << e.what() << endmsg;
    return false;
}
```

#### Error Reporting

```cpp
// Use PBD logging system
#include "pbd/debug.h"
#include "pbd/error.h"
#include "pbd/info.h"

// Different log levels
debug << "Debug message" << endmsg;
info << "Info message" << endmsg;
warning << "Warning message" << endmsg;
error << "Error message" << endmsg;
fatal << "Fatal error" << endmsg;
```

### Memory Management

#### Smart Pointers

```cpp
// Prefer smart pointers over raw pointers
std::shared_ptr<Plugin> plugin = std::make_shared<Plugin>();
std::unique_ptr<PluginResource> resource = std::make_unique<PluginResource>();

// Use weak_ptr to break circular references
std::weak_ptr<Plugin> weak_plugin = plugin;
```

#### RAII Pattern

```cpp
// Resource Acquisition Is Initialization
class PluginResource {
public:
    PluginResource() {
        // Acquire resource in constructor
        acquire_resource();
    }

    ~PluginResource() {
        // Release resource in destructor
        release_resource();
    }

    // Disable copy
    PluginResource(const PluginResource&) = delete;
    PluginResource& operator=(const PluginResource&) = delete;

    // Allow move
    PluginResource(PluginResource&&) = default;
    PluginResource& operator=(PluginResource&&) = default;
};
```

## Platform-Specific Patterns

### Platform Detection

```cpp
// Use preprocessor directives for platform-specific code
#ifdef __APPLE__
    // macOS-specific code
    #include <CoreAudio/CoreAudio.h>
#elif defined(__linux__)
    // Linux-specific code
    #include <alsa/asoundlib.h>
#elif defined(_WIN32)
    // Windows-specific code
    #include <windows.h>
#endif
```

### Conditional Compilation

```cpp
// Feature detection
#ifdef HAVE_VST3
    // VST3 support available
    #include "vst3/vst3.h"
#endif

#ifdef HAVE_LV2
    // LV2 support available
    #include "lv2/lv2.h"
#endif
```

## Plugin System Patterns

### Plugin Interface

```cpp
// Abstract base class for plugins
class Plugin {
public:
    virtual ~Plugin() = default;

    virtual bool activate() = 0;
    virtual void deactivate() = 0;
    virtual void process(float* input, float* output, int frames) = 0;

    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
};

// Concrete implementation
class VSTPlugin : public Plugin {
public:
    VSTPlugin(const std::string& path);
    ~VSTPlugin() override;

    bool activate() override;
    void deactivate() override;
    void process(float* input, float* output, int frames) override;

    std::string name() const override;
    std::string version() const override;

private:
    // Implementation details
};
```

### Plugin Factory Pattern

```cpp
// Plugin factory for creating different types of plugins
class PluginFactory {
public:
    static std::shared_ptr<Plugin> create_plugin(const std::string& path);

private:
    static std::shared_ptr<Plugin> create_vst_plugin(const std::string& path);
    static std::shared_ptr<Plugin> create_au_plugin(const std::string& path);
    static std::shared_ptr<Plugin> create_lv2_plugin(const std::string& path);
};
```

## Build System Patterns

### Wscript Structure

```python
# Standard wscript pattern
def build(bld):
    # Create library target
    obj = bld.new_task_gen('c', 'cprogram', 'cshlib')

    # Source files
    obj.source = '''
        plugin_manager.cc
        plugin_loader.cc
        plugin_error_handler.cc
    '''

    # Dependencies
    obj.use = ['libardour', 'libpbd']
    obj.uselib = 'BOOST'

    # Include paths
    obj.includes = ['../..', '../../libs']

    # Compiler flags
    obj.cflags = ['-Wall', '-Wextra']
    obj.cxxflags = ['-std=c++17']
```

### Conditional Builds

```python
# Feature-based conditional builds
if bld.is_defined('VST_SUPPORT'):
    obj.source += 'vst_plugin.cc'
    obj.uselib += ' VST'

if bld.is_defined('AU_SUPPORT'):
    obj.source += 'au_plugin.cc'
    obj.uselib += ' CORE_AUDIO'
```

## Testing Patterns

### Unit Test Structure

```cpp
// Test class pattern
class PluginManagerTest : public CppUnit::TestFixture {
public:
    void setUp() override {
        // Setup test environment
    }

    void tearDown() override {
        // Cleanup test environment
    }

    void testLoadValidPlugin() {
        // Test implementation
    }

    void testLoadInvalidPlugin() {
        // Test implementation
    }

private:
    std::shared_ptr<PluginManager> manager;
};

// Test registration
CPPUNIT_TEST_SUITE_REGISTRATION(PluginManagerTest);
```

### Mock Objects

```cpp
// Mock plugin for testing
class MockPlugin : public Plugin {
public:
    MOCK_METHOD(bool, activate, (), (override));
    MOCK_METHOD(void, deactivate, (), (override));
    MOCK_METHOD(void, process, (float*, float*, int), (override));
    MOCK_METHOD(std::string, name, (), (const, override));
    MOCK_METHOD(std::string, version, (), (const, override));
};
```

## Performance Patterns

### Lazy Loading

```cpp
// Load resources only when needed
class PluginManager {
public:
    std::shared_ptr<Plugin> get_plugin(const std::string& name) {
        auto it = loaded_plugins.find(name);
        if (it == loaded_plugins.end()) {
            // Load plugin only when requested
            auto plugin = load_plugin(name);
            loaded_plugins[name] = plugin;
            return plugin;
        }
        return it->second;
    }

private:
    std::map<std::string, std::shared_ptr<Plugin>> loaded_plugins;
};
```

### Object Pooling

```cpp
// Reuse objects to avoid allocation overhead
template<typename T>
class ObjectPool {
public:
    std::shared_ptr<T> acquire() {
        if (pool.empty()) {
            return std::make_shared<T>();
        }

        auto obj = pool.back();
        pool.pop_back();
        return obj;
    }

    void release(std::shared_ptr<T> obj) {
        pool.push_back(obj);
    }

private:
    std::vector<std::shared_ptr<T>> pool;
};
```

## Security Patterns

### Input Validation

```cpp
// Always validate inputs
bool PluginManager::load_plugin(const std::string& path) {
    // Validate path
    if (path.empty()) {
        error << "Plugin path cannot be empty" << endmsg;
        return false;
    }

    // Check for path traversal
    if (path.find("..") != std::string::npos) {
        error << "Invalid plugin path: " << path << endmsg;
        return false;
    }

    // Validate file exists and is readable
    if (!Glib::file_test(path, Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR)) {
        error << "Plugin file does not exist: " << path << endmsg;
        return false;
    }

    // Continue with loading...
}
```

### Resource Limits

```cpp
// Limit resource usage
class PluginManager {
public:
    bool load_plugin(const std::string& path) {
        if (loaded_plugins.size() >= MAX_PLUGINS) {
            error << "Maximum number of plugins reached" << endmsg;
            return false;
        }

        // Load plugin...
    }

private:
    static const int MAX_PLUGINS = 100;
    std::vector<std::shared_ptr<Plugin>> loaded_plugins;
};
```

## Documentation Patterns

### Code Documentation

```cpp
/**
 * @brief Plugin manager for loading and managing VST plugins
 *
 * The PluginManager class provides functionality for discovering,
 * loading, and managing VST plugins in headless mode. It handles
 * plugin validation, error recovery, and resource management.
 *
 * @example
 * @code
 * PluginManager pm;
 * auto plugin = pm.load_plugin("/path/to/plugin.vst");
 * if (plugin) {
 *     plugin->activate();
 *     // Process audio...
 *     plugin->deactivate();
 * }
 * @endcode
 */
class PluginManager {
public:
    /**
     * @brief Load a plugin from the specified path
     *
     * @param path Path to the plugin file
     * @return Shared pointer to the loaded plugin, or nullptr if loading failed
     *
     * @throws std::runtime_error if the plugin file is invalid or corrupted
     */
    std::shared_ptr<Plugin> load_plugin(const std::string& path);
};
```

### API Documentation

- **Header comments**: Document class and function purposes
- **Parameter documentation**: Explain all parameters
- **Return value documentation**: Describe return values
- **Exception documentation**: Document exceptions that can be thrown
- **Usage examples**: Provide practical examples
