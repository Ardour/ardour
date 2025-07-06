# JUPITERSOFT Development Documentation

This directory contains comprehensive documentation for the JUPITERSOFT branch of Ardour, consolidating key insights, development patterns, and ongoing issues discovered during development.

## Documentation Structure

### Core Development

- **[BUILD_SYSTEM.md](BUILD_SYSTEM.md)** - Waf build system insights, macOS conventions, dependency management
- **[CODEBASE_PATTERNS.md](CODEBASE_PATTERNS.md)** - Code style, architecture patterns, common conventions
- **[GUI_MIGRATION.md](GUI_MIGRATION.md)** - YTK/GTK migration status, touch interface development

### Feature Development

- **[VST_HEADLESS_SUPPORT.md](VST_HEADLESS_SUPPORT.md)** - VST plugin support in headless mode (consolidated from TODO_VST_HEADLESS_SUPPORT.md)
- **[PLUGIN_SYSTEM.md](PLUGIN_SYSTEM.md)** - Plugin architecture, loading mechanisms, platform-specific handling

### Platform & Environment

- **[MACOS_DEVELOPMENT.md](MACOS_DEVELOPMENT.md)** - macOS-specific build issues, Homebrew setup, ARM64 considerations
- **[DEVELOPMENT_WORKFLOW.md](DEVELOPMENT_WORKFLOW.md)** - Git workflow, testing procedures, debugging techniques

### Known Issues & Solutions

- **[BUILD_ISSUES.md](BUILD_ISSUES.md)** - Common build failures, workarounds, dependency conflicts
- **[RUNTIME_ISSUES.md](RUNTIME_ISSUES.md)** - Runtime problems, crash patterns, performance issues

## Quick Reference

### Essential Commands

```bash
# Build with proper macOS environment
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:/opt/homebrew/opt/libarchive/lib/pkgconfig"
export CPATH="/opt/homebrew/include:/opt/homebrew/opt/boost/include"
export CPLUS_INCLUDE_PATH="$CPATH"
export LDFLAGS="-L/opt/homebrew/opt/libarchive/lib"
export CPPFLAGS="-I/opt/homebrew/opt/libarchive/include"
./waf configure --strict --with-backends=jack,coreaudio,dummy --ptformat --optimize
./waf -j$(sysctl -n hw.logicalcpu)

# Run Ardour (don't use ./waf install on macOS)
cd gtk2_ardour && ./ardev
```

### Key Insights

- **Don't modify YTK/GTK code** - GUI migration is in progress
- **Use `ardev` script** - Don't install on macOS, run from build directory
- **Environment variables matter** - PKG_CONFIG_PATH and include paths are critical
- **Clearlooks needs GTK linkage** - Added `'GTK'` to uselib when YTK is defined

## Contributing

When adding new documentation:

1. Follow the existing structure and naming conventions
2. Include specific examples and code snippets
3. Reference related files when appropriate
4. Update this README if adding new categories
