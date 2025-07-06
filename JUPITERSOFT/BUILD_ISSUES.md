# Build Issues & Solutions

## Common Build Failures

### 1. Missing Boost Headers

**Error**: `'boost/operators.hpp' file not found`

**Solution**:

```bash
# Set environment variables
export CPATH="/opt/homebrew/include:/opt/homebrew/opt/boost/include"
export CPLUS_INCLUDE_PATH="$CPATH"

# Reconfigure and build
./waf clean
./waf configure --boost-include=/opt/homebrew/opt/boost/include
./waf build
```

**Root Cause**: Homebrew Boost headers not in compiler include path

### 2. libarchive Not Found

**Error**: `'archive.h' file not found`

**Solution**:

```bash
# Set environment variables
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:/opt/homebrew/opt/libarchive/lib/pkgconfig"
export CPPFLAGS="-I/opt/homebrew/opt/libarchive/include"
export LDFLAGS="-L/opt/homebrew/opt/libarchive/lib"

# Reconfigure and build
./waf clean
./waf configure --also-include=/opt/homebrew/opt/libarchive/include
./waf build
```

**Root Cause**: libarchive not in pkg-config path or include paths

### 3. GTK Symbols Undefined (Clearlooks)

**Error**: `Undefined symbols for architecture arm64: "_gdk_cairo_create"`

**Solution**: ✅ **FIXED** - Added `'GTK'` to uselib in `libs/clearlooks-newer/wscript`

```python
if bld.is_defined('YTK'):
    obj.uselib = ' CAIRO PANGO GTK'  # GTK added for macOS compatibility
```

**Root Cause**: Clearlooks library missing GTK linkage when YTK is defined
**Status**: Applied during development, documented for future reference

### 4. Alias Attribute Not Supported

**Error**: `error: 'alias' attribute is not supported on this target`

**Solution**: ✅ **FIXED** - Added `#ifndef __APPLE__` guards in alias files

```c
#ifndef __APPLE__  // Prevents alias compilation on macOS
    // ... alias definitions
#endif
```

**Root Cause**: Clang doesn't support GNU `alias` attribute
**Status**: Applied during development, documented for future reference
**Impact**: Enables YDK/YTK libraries to build on macOS

### 5. pkg-config Errors

**Error**: `Package 'gtk+-2.0' not found`

**Solution**:

```bash
# Install GTK
brew install gtk+2

# Check pkg-config
pkg-config --exists gtk+-2.0 && echo "Found" || echo "Missing"

# Set PKG_CONFIG_PATH
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
```

**Root Cause**: GTK not installed or pkg-config path incorrect

## Build Fixes Applied During Development

### Clearlooks GTK Linkage Fix

**File**: `libs/clearlooks-newer/wscript`
**Change**: Added `'GTK'` to uselib when YTK is defined
**Reason**: Clearlooks library needs GTK symbols even when using YTK libraries
**Impact**: Fixes build failure on macOS with YTK enabled

### YDK/YTK Alias Attribute Fix

**Files**:

- `libs/tk/ydk/gdkaliasdef.c`
- `libs/tk/ytk/gtkaliasdef.c`
  **Change**: Added `#ifndef __APPLE__` guards around alias definitions
  **Reason**: Clang on macOS doesn't support GNU `alias` attribute
  **Impact**: Enables YDK/YTK libraries to compile on macOS
  **Note**: These are generated files, changes may be lost on regeneration

## Platform-Specific Issues

### macOS Issues

#### ARM64 (Apple Silicon) Issues

**Problem**: x86_64 libraries on ARM64
**Solution**: Use Rosetta 2 or native ARM64 libraries

```bash
# Check architecture
uname -m  # Should show arm64

# Use native ARM64 libraries
brew install --build-from-source boost libarchive
```

#### Homebrew Path Issues

**Problem**: Libraries in `/opt/homebrew/` not found
**Solution**: Set correct environment variables

```bash
export PATH="/opt/homebrew/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
export CPATH="/opt/homebrew/include:$CPATH"
```

### Linux Issues

#### Missing Development Packages

**Problem**: Headers not found
**Solution**: Install development packages

```bash
# Ubuntu/Debian
sudo apt-get install libboost-all-dev libarchive-dev libgtk2.0-dev

# CentOS/RHEL
sudo yum install boost-devel libarchive-devel gtk2-devel
```

#### Library Version Conflicts

**Problem**: Multiple versions of same library
**Solution**: Use pkg-config to find correct version

```bash
pkg-config --modversion gtk+-2.0
pkg-config --cflags gtk+-2.0
```

## Dependency Conflicts

### Version Mismatches

**Problem**: Library version too old or incompatible
**Solution**: Check minimum versions in wscript

```bash
# Check installed versions
pkg-config --modversion gtk+-2.0
brew list --versions boost

# Update if needed
brew upgrade boost libarchive gtk+2
```

### Multiple Installations

**Problem**: System and Homebrew versions conflict
**Solution**: Prioritize Homebrew paths

```bash
# Ensure Homebrew paths come first
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
```

## Build System Issues

### Waf Configuration Problems

**Problem**: Configuration fails or incorrect
**Solution**: Clean and reconfigure

```bash
# Clean everything
./waf clean
rm -rf build/

# Reconfigure with verbose output
./waf configure -v

# Check configuration log
cat build/config.log | grep -i "not found"
```

### Parallel Build Issues

**Problem**: Race conditions or memory issues
**Solution**: Reduce parallel jobs

```bash
# Use fewer cores
./waf -j4

# Or build sequentially
./waf -j1
```

## Environment Issues

### Shell Configuration

**Problem**: Environment variables not set
**Solution**: Add to shell profile

```bash
# Add to ~/.zshrc or ~/.bash_profile
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
export CPATH="/opt/homebrew/include:$CPATH"
export CPLUS_INCLUDE_PATH="$CPATH"
```

### Xcode Command Line Tools

**Problem**: Missing development tools
**Solution**: Install Xcode command line tools

```bash
xcode-select --install
```

## Debugging Techniques

### Verbose Build Output

```bash
# Get detailed build information
./waf build -v

# Check specific component
./waf build -v gtk2_ardour
```

### Configuration Analysis

```bash
# Check what was detected
cat build/config.log | grep -E "(yes|no|found|not found)"

# Check compiler flags
cat build/config.log | grep "CXXFLAGS"
```

### Library Detection

```bash
# Test pkg-config
pkg-config --exists gtk+-2.0 && echo "GTK found" || echo "GTK missing"

# Check library paths
pkg-config --libs gtk+-2.0
pkg-config --cflags gtk+-2.0
```

## Prevention Strategies

### Environment Setup Script

Create a setup script to ensure consistent environment:

```bash
#!/bin/bash
# setup_build_env.sh

export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:/opt/homebrew/opt/libarchive/lib/pkgconfig"
export CPATH="/opt/homebrew/include:/opt/homebrew/opt/boost/include"
export CPLUS_INCLUDE_PATH="$CPATH"
export LDFLAGS="-L/opt/homebrew/opt/libarchive/lib"
export CPPFLAGS="-I/opt/homebrew/opt/libarchive/include"

echo "Build environment configured"
```

### Dependency Check Script

Create a script to verify all dependencies:

```bash
#!/bin/bash
# check_deps.sh

deps=("gtk+-2.0" "boost" "libarchive")

for dep in "${deps[@]}"; do
    if pkg-config --exists "$dep"; then
        echo "✅ $dep found"
    else
        echo "❌ $dep missing"
    fi
done
```

## Community Resources

### Official Documentation

- [Ardour Development Guide](https://ardour.org/development.html)
- [Build Troubleshooting](https://ardour.org/development.html#troubleshooting)

### Community Forums

- [Ardour Discourse - Build Issues](https://discourse.ardour.org/c/development/build)
- [GitHub Issues](https://github.com/Ardour/ardour/issues)

### Platform-Specific Help

- **macOS**: [Ardour macOS Build](https://discourse.ardour.org/c/development/macos)
- **Linux**: [Ardour Linux Build](https://discourse.ardour.org/c/development/linux)
- **Windows**: [Ardour Windows Build](https://discourse.ardour.org/c/development/windows)
