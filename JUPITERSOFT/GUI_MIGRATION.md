# GUI Migration & Touch Interface Development

## Overview

Ardour is undergoing a GUI migration from GTK+2.0 to a custom touch-optimized framework called **YTK** (Yupiter Touch Kit). This migration is in progress and affects multiple components.

## Architecture

### Current State

- **GTK+2.0**: Legacy GUI framework (stable, widely supported)
- **YTK**: Custom GTK variant for touch interfaces (in development)
- **YDK**: Custom GDK variant (companion to YTK)
- **ZTK**: Additional touch interface components

### Migration Strategy

- **Incremental migration**: Components are migrated one at a time
- **Backward compatibility**: GTK+2.0 remains functional during transition
- **Feature flags**: `YTK` define controls which framework is used

## Key Components

### YTK Libraries

```
libs/tk/ytk/          # Main YTK implementation
libs/tk/ydk/          # GDK variant for YTK
libs/tk/ztk/          # Additional touch components
libs/clearlooks-newer/ # Theme engine (supports both GTK and YTK)
```

### Build Configuration

```python
# Example from libs/clearlooks-newer/wscript
if bld.is_defined('YTK'):
    obj.use     = [ 'libztk', 'libytk', 'libydk', 'libydk-pixbuf' ]
    obj.uselib  = ' CAIRO PANGO GTK'  # GTK still needed for symbols
else:
    obj.uselib = 'GTK'
```

## Development Rules

### ⚠️ CRITICAL: Don't Modify GUI Code

**Rule**: Do NOT modify YTK/GTK code unless you're specifically working on the GUI migration.

**Rationale**:

- GUI migration is in progress and complex
- Changes can break touch interface development
- Your code doesn't touch the GUI, so avoid conflicts

**What NOT to do**:

- ❌ Modify `libs/tk/ytk/` files
- ❌ Modify `libs/tk/ydk/` files
- ❌ Modify `libs/clearlooks-newer/` files
- ❌ Change GTK-related build configuration
- ❌ Modify GUI-related wscript files

**What IS allowed**:

- ✅ Add missing library linkages (like adding `'GTK'` to uselib)
- ✅ Fix build issues that prevent compilation
- ✅ Add platform-specific guards (like `#ifndef __APPLE__`)

## Touch Interface Features

### YTK Capabilities

- **Touch-optimized widgets**: Larger hit targets, gesture support
- **Custom drawing**: Cairo-based rendering for better control
- **Responsive design**: Adapts to different screen sizes
- **Gesture recognition**: Multi-touch support

### Development Status

- **In Progress**: Core framework development
- **Experimental**: Touch interface features
- **Unstable**: May break during development

## Platform Compatibility

### macOS Considerations

- **Clang compatibility**: YTK uses GNU extensions not supported by Clang
- **Alias attributes**: Fixed with `#ifndef __APPLE__` guards
- **Library linkage**: YTK libraries need GTK symbols for compatibility

### Linux Considerations

- **GCC compatibility**: Full support for GNU extensions
- **Native GTK**: Better integration with system GTK
- **Package management**: Easier dependency resolution

## Build System Integration

### Feature Detection

```python
# Check if YTK is enabled
if bld.is_defined('YTK'):
    # Use YTK libraries
    obj.use = [ 'libztk', 'libytk', 'libydk', 'libydk-pixbuf' ]
else:
    # Use standard GTK
    obj.uselib = 'GTK'
```

### Conditional Compilation

```c
#ifdef YTK
    // YTK-specific code
    ytk_widget_do_something();
#else
    // GTK-specific code
    gtk_widget_do_something();
#endif
```

## Known Issues

### 1. Clearlooks GTK Linkage

**Issue**: Clearlooks library missing GTK symbols when YTK is defined
**Status**: ✅ Fixed by adding `'GTK'` to uselib
**Impact**: Build failure on macOS

### 2. Alias Attribute Support

**Issue**: Clang doesn't support GNU `alias` attribute
**Status**: ✅ Fixed with `#ifndef __APPLE__` guards
**Impact**: Compilation failure on macOS

### 3. Library Dependencies

**Issue**: YTK libraries don't provide all GTK symbols
**Status**: 🔄 Ongoing - libraries need GTK for compatibility
**Impact**: Linkage errors during build

## Future Development

### Migration Roadmap

1. **Phase 1**: Core YTK framework (in progress)
2. **Phase 2**: Widget migration (planned)
3. **Phase 3**: Touch interface features (planned)
4. **Phase 4**: GTK+2.0 deprecation (future)

### Touch Interface Goals

- **Multi-touch support**: Gesture recognition and handling
- **Responsive design**: Adapt to different screen sizes
- **Accessibility**: Better support for accessibility features
- **Performance**: Optimized rendering for touch devices

## Development Guidelines

### For GUI Developers

- Work in `libs/tk/ytk/` and related directories
- Test both GTK and YTK modes
- Maintain backward compatibility
- Document touch interface features

### For Non-GUI Developers

- Avoid modifying GUI-related code
- Focus on your specific domain (audio, plugins, etc.)
- Report GUI issues to the GUI team
- Test your changes with both GTK and YTK

### Testing

```bash
# Test with GTK (default)
./waf configure
./waf build

# Test with YTK
./waf configure --ytk
./waf build
```

## Resources

### Documentation

- [GTK+2.0 Reference](https://developer.gnome.org/gtk2/)
- [Cairo Graphics](https://www.cairographics.org/)
- [Touch Interface Design](https://developer.apple.com/design/human-interface-guidelines/)

### Community

- [Ardour GUI Development](https://discourse.ardour.org/c/development/gui)
- [Touch Interface Discussion](https://discourse.ardour.org/c/development/touch)
