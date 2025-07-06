# Build System Documentation

## Waf Build System Overview

Ardour uses the **Waf** build system, a Python-based alternative to Make/CMake. Waf files (`wscript`) contain Python code that defines build targets, dependencies, and compilation rules.

### Key Waf Concepts

#### Wscript Structure

```python
# Example from libs/clearlooks-newer/wscript
def build(bld):
    obj = bld.new_task_gen('c', 'cprogram', 'cshlib')
    obj.source = '''
        animation.c
        cairo-support.c
        clearlooks_draw.c
        # ... source files
    '''

    if bld.is_defined('YTK'):
        obj.use     = [ 'libztk', 'libytk', 'libydk', 'libydk-pixbuf' ]
        obj.uselib  = ' CAIRO PANGO GTK'  # ← GTK added for macOS compatibility
    else:
        obj.uselib = 'GTK'
```

#### Build Configuration

- **`bld.is_defined('FEATURE')`** - Check if feature is enabled
- **`obj.use = ['lib1', 'lib2']`** - Link against internal libraries
- **`obj.uselib = 'LIB1 LIB2'`** - Link against external libraries (via pkg-config)
- **`obj.includes = ['path1', 'path2']`** - Add include paths

### macOS Build Conventions

#### Environment Setup (Critical)

```bash
# Essential environment variables for macOS/Homebrew
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:/opt/homebrew/opt/libarchive/lib/pkgconfig"
export CPATH="/opt/homebrew/include:/opt/homebrew/opt/boost/include"
export CPLUS_INCLUDE_PATH="$CPATH"
export LDFLAGS="-L/opt/homebrew/opt/libarchive/lib"
export CPPFLAGS="-I/opt/homebrew/opt/libarchive/include"
```

#### Build Commands

```bash
# Clean and reconfigure
./waf clean
./waf configure --strict --with-backends=jack,coreaudio,dummy --ptformat --optimize

# Build with all cores
./waf -j$(sysctl -n hw.logicalcpu)

# Run without installing (macOS convention)
cd gtk2_ardour && ./ardev
```

#### Key Dependencies

- **Boost** (≥1.68) - C++ libraries
- **libarchive** - Archive handling
- **GTK+2.0** (≥2.12.1) - GUI framework
- **JACK** - Audio backend
- **CoreAudio** - macOS audio backend

### Build System Quirks

#### Platform-Specific Guards

```c
// Example from libs/tk/ytk/gtkaliasdef.c
#ifndef DISABLE_VISIBILITY
#include <glib.h>
#ifdef G_HAVE_GNUC_VISIBILITY
#ifndef __APPLE__  // ← Prevents alias compilation on macOS
    // ... alias definitions
#endif
#endif
#endif
```

#### Library Linkage Issues

- **Clearlooks + YTK**: Needs explicit GTK linkage when YTK is defined
- **pkg-config paths**: Must be set correctly for Homebrew libraries
- **ARM64 considerations**: Some libraries may need specific flags

### Dependency Management

#### Homebrew Integration

```bash
# Install dependencies
brew install boost libarchive jack gtk+2

# Check pkg-config availability
pkg-config --cflags gtk+-2.0
pkg-config --libs gtk+-2.0
```

#### Build Configuration Detection

The build system automatically detects:

- Platform (macOS/Linux/Windows)
- Architecture (x86_64/ARM64)
- Available backends (JACK/CoreAudio/ALSA)
- Library versions and features

### Troubleshooting

#### Common Issues

1. **Missing Boost headers**: Set `CPATH` and `CPLUS_INCLUDE_PATH`
2. **libarchive not found**: Add to `PKG_CONFIG_PATH`
3. **GTK symbols undefined**: Ensure `GTK` is in `uselib` for clearlooks
4. **pkg-config errors**: Verify Homebrew installation and paths

#### Debug Commands

```bash
# Check configuration
./waf configure --help
cat build/config.log | grep -i "not found"

# Verbose build
./waf build -v

# Check specific library
pkg-config --exists gtk+-2.0 && echo "Found" || echo "Missing"
```

### Build Targets

#### Main Targets

- **`./waf`** - Build everything
- **`./waf build`** - Explicit build
- **`./waf install`** - Install (avoid on macOS)
- **`./waf clean`** - Clean build artifacts

#### Component Targets

- **`./waf libs`** - Build libraries only
- **`./waf gtk2_ardour`** - Build GUI application
- **`./waf headless`** - Build headless components

### Performance Optimization

#### Parallel Builds

```bash
# Use all CPU cores
./waf -j$(sysctl -n hw.logicalcpu)

# Or specify number
./waf -j8
```

#### Build Caching

- Waf caches build artifacts in `build/` directory
- Use `./waf clean` to force rebuild
- Incremental builds are automatic
