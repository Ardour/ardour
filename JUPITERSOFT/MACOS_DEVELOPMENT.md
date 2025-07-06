# macOS Development Guide

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

## Homebrew Setup

### Essential Dependencies

```bash
# Core dependencies
brew install boost libarchive jack gtk+2

# Development tools
brew install pkg-config autoconf automake

# Optional but recommended
brew install cmake ninja
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
```

### Build Configuration

```bash
# Standard macOS configuration
./waf configure \
  --strict \
  --with-backends=jack,coreaudio,dummy \
  --ptformat \
  --optimize
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
3. ✅ Run `./waf clean` and reconfigure
4. ✅ Check `build/config.log` for specific errors
5. ✅ Verify pkg-config can find libraries

### Runtime Issues

1. ✅ Use `ardev` script to run Ardour
2. ✅ Check audio backend configuration
3. ✅ Verify JACK server is running (if using JACK)
4. ✅ Check system permissions for audio access

### GUI Issues

1. ✅ Don't modify YTK/GTK code (GUI migration in progress)
2. ✅ Check GTK theme compatibility
3. ✅ Verify display scaling settings

## Community Resources

### Official Documentation

- [Ardour Development Guide](https://ardour.org/development.html)
- [macOS Build Instructions](https://ardour.org/development.html#macos)

### Community Forums

- [Ardour Discourse](https://discourse.ardour.org/) - Community support
- [GitHub Issues](https://github.com/Ardour/ardour/issues) - Bug reports

### Related Projects

- [ardour-build](https://github.com/chapatt/ardour-build) - Community build scripts
- [Homebrew](https://brew.sh/) - Package manager for macOS
