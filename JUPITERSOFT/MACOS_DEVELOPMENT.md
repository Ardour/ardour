# macOS Development Guide

## ⚠️ **Official Ardour Stance on Package Managers**

**Important**: The official Ardour development team explicitly states that **package managers like Homebrew, MacPorts, or Fink are not recommended** for building Ardour. They prefer building dependencies from source.

However, for **development and testing purposes**, Homebrew can work with proper environment configuration. This guide provides a practical approach for developers who prefer package managers.

## Platform-Specific Considerations

### Architecture Support

- **Apple Silicon (ARM64)**: Primary target for modern macOS
- **Intel (x86_64)**: Legacy support via Rosetta 2
- **Universal Binaries**: Not currently supported in Ardour

### Key Differences from Linux

1. **No system package manager**: Use Homebrew instead of apt/yum
2. **Different library paths**: `/opt/homebrew/` vs `/usr/local/`
3. **CoreAudio backend**: Native macOS audio system
4. **Clang compiler**: Different from GCC on Linux
5. **No installation**: Run from build directory with `ardev` script

## Prerequisites

### Required System Tools

```bash
# XCode Command Line Tools (required)
xcode-select --install

# Verify installation
gcc --version  # Should work if XCode is installed

# Git (required)
brew install git

# Python 2.6+ (required for Waf build system)
# macOS includes Python 2.7 by default
python --version
```

### JACK Audio Server (Optional)

```bash
# JACK is not required but recommended for audio development
# JackOSX: version 0.89 or newer
# JACK1: version 0.121 or newer
brew install jack
```

## Homebrew Setup

### Essential Dependencies

Based on official Ardour build dependencies:

```bash
# Core build dependencies
brew install boost@1.68 libarchive jack gtk+2

# Development tools
brew install pkg-config autoconf automake cmake

# Audio libraries
brew install libsndfile libsamplerate fftw

# GUI libraries
brew install cairo cairomm glib glibmm gtkmm

# Additional libraries
brew install curl libxml2 libxslt expat
brew install libogg libvorbis flac
brew install libpng libjpeg fontconfig freetype
brew install harfbuzz fribidi

# Optional but recommended
brew install ninja ripgrep fd bat exa
```

### Version-Specific Requirements

**Critical**: Some dependencies require specific versions for compatibility:

```bash
# Boost 1.68.0 is the official version used by Ardour
brew install boost@1.68
brew link boost@1.68 --force

# Other version-sensitive dependencies
brew install libsndfile@1.1.0  # Modified version used by Ardour
brew install fftw@3.3.8        # Official version
```

### Path Configuration

```bash
# Add to ~/.zshrc or ~/.bash_profile
export PATH="/opt/homebrew/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
export CPATH="/opt/homebrew/include:$CPATH"
export CPLUS_INCLUDE_PATH="/opt/homebrew/include:$CPLUS_INCLUDE_PATH"
```

## Build Environment

### Required Environment Variables

```bash
# Critical for successful builds
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:/opt/homebrew/opt/libarchive/lib/pkgconfig"
export CPATH="/opt/homebrew/include:/opt/homebrew/opt/boost/include"
export CPLUS_INCLUDE_PATH="$CPATH"
export LDFLAGS="-L/opt/homebrew/opt/libarchive/lib"
export CPPFLAGS="-I/opt/homebrew/opt/libarchive/include"

# Boost-specific paths
export BOOST_ROOT="/opt/homebrew/opt/boost@1.68"
export BOOST_INCLUDEDIR="/opt/homebrew/opt/boost@1.68/include"
export BOOST_LIBRARYDIR="/opt/homebrew/opt/boost@1.68/lib"
```

### Build Configuration

```bash
# Standard macOS configuration
./waf configure \
  --strict \
  --with-backends=jack,coreaudio,dummy \
  --ptformat \
  --optimize \
  --boost-include=/opt/homebrew/opt/boost@1.68/include
```

## Known Issues & Solutions

### 1. Alias Attribute Not Supported

**Issue**: Clang doesn't support GNU `alias` attribute used in YDK/YTK libraries
**Solution**: Already fixed with `#ifndef __APPLE__` guards in `gtkaliasdef.c`

### 2. Clearlooks GTK Linkage

**Issue**: Clearlooks library missing GTK symbols when YTK is defined
**Solution**: Added `'GTK'` to `uselib` in `libs/clearlooks-newer/wscript`

```python
if bld.is_defined('YTK'):
    obj.uselib = ' CAIRO PANGO GTK'  # GTK added for macOS
```

### 3. pkg-config Path Issues

**Issue**: Build system can't find Homebrew libraries
**Solution**: Set `PKG_CONFIG_PATH` to include Homebrew paths

### 4. Boost Headers Not Found

**Issue**: Compiler can't find Boost headers
**Solution**: Set `CPATH` and `CPLUS_INCLUDE_PATH` to include Boost

### 5. libarchive Not Found

**Issue**: Archive handling library not detected
**Solution**: Add libarchive to `PKG_CONFIG_PATH` and set include paths

### 6. Version Conflicts

**Issue**: Homebrew packages may be newer than Ardour expects
**Solution**: Pin specific versions or use `brew switch` to manage versions

```bash
# Example: Pin Boost to 1.68
brew unlink boost
brew link boost@1.68 --force
```

## Runtime Considerations

### Audio Backend

- **CoreAudio**: Native macOS audio system (recommended)
- **JACK**: Cross-platform audio server (optional)
- **Dummy**: For testing without audio hardware

### GUI Framework

- **GTK+2.0**: Legacy GUI framework
- **YTK**: Custom GTK variant for touch interfaces (in development)
- **Don't modify YTK/GTK code**: GUI migration is in progress

### Running Ardour

```bash
# Don't use ./waf install on macOS
cd gtk2_ardour
./ardev  # Sets up environment and runs Ardour
```

### Creating Application Bundle

For distribution or easier launching, you can create a macOS application bundle:

```bash
# Create a .app bundle (optional)
cd tools/osx_packaging
./osx_build --public
```

This creates a `Ardour.app` bundle that can be double-clicked to launch Ardour. The bundle contains all necessary libraries and dependencies.

## Development Tools

### Recommended Setup

```bash
# Install development tools
brew install --cask visual-studio-code
brew install --cask xcode

# Install useful CLI tools
brew install ripgrep fd bat exa
```

### Debugging

```bash
# Check library availability
pkg-config --exists gtk+-2.0 && echo "Found" || echo "Missing"

# Verbose build output
./waf build -v

# Check configuration
cat build/config.log | grep -i "not found"

# Verify dependency versions
pkg-config --modversion boost
pkg-config --modversion libarchive
```

## Performance Optimization

### Build Performance

```bash
# Use all CPU cores
./waf -j$(sysctl -n hw.logicalcpu)

# Clean build for troubleshooting
./waf clean
```

### Runtime Performance

- **ARM64 native**: Better performance on Apple Silicon
- **Rosetta 2**: Automatic translation for Intel binaries
- **Memory management**: macOS handles memory differently than Linux

## Troubleshooting Checklist

### Build Fails

1. ✅ Check environment variables are set
2. ✅ Verify Homebrew dependencies are installed
3. ✅ Check for version conflicts (especially Boost)
4. ✅ Run `./waf clean` and reconfigure
5. ✅ Check `build/config.log` for specific errors
6. ✅ Verify pkg-config can find libraries

### Runtime Issues

1. ✅ Use `ardev` script to run Ardour
2. ✅ Check audio backend configuration
3. ✅ Verify JACK server is running (if using JACK)
4. ✅ Check system permissions for audio access

### GUI Issues

1. ✅ Don't modify YTK/GTK code (GUI migration in progress)
2. ✅ Check GTK theme compatibility
3. ✅ Verify display scaling settings

## Alternative: Official Source Build

If you encounter issues with Homebrew, consider the official approach:

1. **Download all dependencies from source** (see [current_dependencies.html](https://ardour.org/current_dependencies.html))
2. **Build each dependency manually** from source
3. **Use the exact versions** specified in the nightly build dependencies

This is more complex but ensures compatibility with the official build process.

### Modified Libraries

The official Ardour builds use modified versions of several libraries:

- **cairo**: Patched to disable h/w gradient rendering on buggy Linux video drivers
- **libsndfile**: Backported fix for Zoom R8/R24 file reading (large meta-data header chunks)
- **libwebsockets**: Repacked with Windows build fixes
- **LV2 stack (serd/lilv)**: Fixes for cross-platform use with waf

These modifications are available from the [nightly build dependencies page](https://nightly.ardour.org/list.php#build_deps). For development purposes, the standard versions from Homebrew should work fine.

## Community Resources

### Official Documentation

- [Ardour Development Guide](https://ardour.org/development.html)
- [macOS Build Instructions](https://ardour.org/building_osx_native.html)
- [Current Dependencies](https://ardour.org/current_dependencies.html)
- [Nightly Build Dependencies](https://nightly.ardour.org/list.php#build_deps)

### Community Forums

- [Ardour Community](https://discourse.ardour.org/)
- [macOS Development](https://discourse.ardour.org/c/development/macos)

### Related Projects

- [ardour-build](https://github.com/chapatt/ardour-build) - Community build scripts
- [Homebrew](https://brew.sh/) - Package manager for macOS

## 🖥️ **GUI Migration & Touch Interface**

### **Current State**

- **GTK+2.0**: Legacy GUI framework (stable, widely supported)
- **YTK**: Custom GTK variant for touch interfaces (in development)
- **YDK**: Custom GDK variant (companion to YTK)
- **ZTK**: Additional touch interface components

### **Migration Strategy**

- **Incremental migration**: Components are migrated one at a time
- **Backward compatibility**: GTK+2.0 remains functional during transition
- **Feature flags**: `YTK` define controls which framework is used

### **Key Components**

```
libs/tk/ytk/          # Main YTK implementation
libs/tk/ydk/          # GDK variant for YTK
libs/tk/ztk/          # Additional touch components
libs/clearlooks-newer/ # Theme engine (supports both GTK and YTK)
```

### **macOS-Specific GUI Considerations**

- **Quartz backend**: YTK uses Quartz on macOS instead of X11
- **Touch interface**: YTK designed for touch/pen input (relevant for iPad apps)
- **High DPI**: Better support for Retina displays
- **Native look**: More native macOS appearance

### **Development Guidelines**

- **Don't modify YTK/GTK code**: GUI migration is in progress
- **Use feature flags**: Check `bld.is_defined('YTK')` before making changes
- **Test both frameworks**: Ensure compatibility with GTK+2.0 and YTK
- **Follow existing patterns**: Maintain consistency with current codebase

### **Build Configuration**

```bash
# Build with GTK+2.0 (default)
./waf configure --strict

# Build with YTK (experimental)
./waf configure --strict --ytk
```

### **Runtime Selection**

```bash
# GTK+2.0 (default)
cd gtk2_ardour && ./ardev

# YTK (if built with --ytk)
cd gtk2_ardour && ./ardev --ytk
```
