# 🔄 **Development Workflow & Decision Matrix**

## 📋 **Overview**

This document establishes the development workflow and decision-making process used in this project, aligned with Ardour's official development practices. All justified code changes have been applied and are working correctly.

**Reference:** [Ardour Development Guide](https://ardour.org/development.html)

---

## 🏗️ **Ardour Architecture Context**

### **Codebase Overview**

- **Total Size**: ~160,000 lines of code
- **UI Layer**: ~48,000 lines (gtkmm C++ wrapper around GTK+)
- **Backend Engine**: ~34,000 lines
- **Architecture**: Heavy use of async signal/callback system for anonymous coupling
- **Pattern**: Model-View-Controller (MVC) programming model

### **Development Philosophy**

- **Real-time Collaboration**: Core development happens on IRC for real-time discussions
- **No Formal Roadmap**: Development is iterative and discussion-driven
- **Minimal Documentation**: Doxygen-generated docs, technical notes for specific areas
- **Community-Driven**: Bug/feature tracking, mailing lists for involvement

---

## 🎯 **Core Principles**

### **1. Minimal Code Changes**

- ✅ Only modify code when **absolutely necessary**
- ✅ Prefer build system solutions over code changes
- ✅ Use environment variables and configure flags first
- ✅ Respect the existing async signal/callback architecture

### **2. Justified Changes Only**

- ✅ Platform compatibility issues with no clean alternative
- ✅ Build system bugs that affect functionality
- ✅ Header conflicts that break compilation
- ✅ Plugin system integration (respecting non-sandboxed approach)
- ❌ Environment-specific workarounds
- ❌ Platform-specific guards for non-platform issues

### **3. Clean Alternatives First**

- ✅ Build system flags (`--boost-include`, `--also-include`)
- ✅ Environment variables (`CPPFLAGS`, `LDFLAGS`)
- ✅ pkg-config and standard build practices
- ✅ Leverage existing async callback mechanisms
- ❌ Hardcoded paths in wscript files
- ❌ Platform-specific code wrapping

---

## 🔍 **Decision Matrix**

### **When to Modify Code**

| Issue Type                 | Justification                   | Example                            | Status     |
| -------------------------- | ------------------------------- | ---------------------------------- | ---------- |
| **Platform Compatibility** | No clean alternative exists     | macOS/Clang alias attributes       | ✅ Applied |
| **Build System Bug**       | Affects core functionality      | YTK/GTK2 library linking           | ✅ Applied |
| **Header Conflict**        | Breaks compilation              | FluidSynth internal/system headers | ✅ Applied |
| **Plugin Integration**     | Headless VST support needed     | VST plugin loading in headless     | ✅ Applied |
| **Environment Issue**      | Can be solved with build system | Missing Boost headers              | ❌ Avoided |
| **Platform-Specific**      | Not actually platform-specific  | Windows code wrapping              | ❌ Avoided |

### **When to Use Build System**

| Issue Type               | Solution               | Example                 | Status      |
| ------------------------ | ---------------------- | ----------------------- | ----------- |
| **Missing Dependencies** | `--boost-include` flag | Boost headers not found | ✅ Resolved |
| **Include Path Issues**  | `--also-include` flag  | libarchive headers      | ✅ Resolved |
| **Library Path Issues**  | Environment variables  | Homebrew library paths  | ✅ Resolved |
| **pkg-config Issues**    | `PKG_CONFIG_PATH`      | libarchive detection    | ✅ Resolved |

---

## 📊 **Applied Changes Summary**

### **Justified Code Changes (Applied)**

1. **YTK/GTK2 Build Fix** - Conditional library linking in `gtk2_ardour/wscript`
2. **FluidSynth Headers** - Internal header path in `fluidsynth_priv.h`
3. **VST Headless Support** - Plugin loading and processing in headless mode

### **Environment Solutions (Applied)**

1. **Boost Headers** - `--boost-include=/opt/homebrew/include`
2. **libarchive Headers** - `PKG_CONFIG_PATH` environment variable
3. **General Includes** - `CPPFLAGS` and `LDFLAGS` environment variables

### **Avoided Changes**

1. **Windows Code Wrapping** - Fixed real root causes instead
2. **Build System Path Mods** - Used existing flags and environment
3. **Platform-Specific Guards** - Not actually platform-specific issues
4. **Plugin Sandboxing** - Respecting Ardour's non-sandboxed plugin approach

---

## 🔄 **Development Process**

### **1. Issue Identification**

```bash
# Build fails with specific error
./waf build
# Error: 'some_header.h' file not found
```

### **2. Root Cause Analysis**

```bash
# Check if it's a dependency issue
brew list | grep package_name
# Check if it's a path issue
pkg-config --cflags package_name
# Check if it's an architecture issue
# (async callbacks, MVC patterns, etc.)
```

### **3. Solution Selection**

```bash
# Try environment variables first
export CPPFLAGS="-I/opt/homebrew/include"
export LDFLAGS="-L/opt/homebrew/lib"

# Try build system flags
./waf configure --boost-include=/opt/homebrew/include

# Only modify code if no clean alternative exists
# Respect existing async callback architecture
```

### **4. Validation**

```bash
# Test the solution
./waf clean
./waf configure
./waf build
```

---

## 🚫 **Anti-Patterns to Avoid**

### **1. Platform-Specific Code Wrapping**

```cpp
// ❌ DON'T: Add platform guards for non-platform issues
#ifdef _WIN32
// Windows-specific code
#endif

// ✅ DO: Fix the actual root cause
// Usually missing dependencies or environment issues
```

### **2. Hardcoded Paths in Build System**

```python
# ❌ DON'T: Hardcode paths in wscript files
obj.includes = ['/opt/homebrew/include']

# ✅ DO: Use environment variables and flags
./waf configure --also-include=/opt/homebrew/include
```

### **3. Environment-Specific Workarounds**

```cpp
// ❌ DON'T: Add workarounds for environment issues
#ifdef __APPLE__
// macOS-specific workaround
#endif

// ✅ DO: Fix environment setup
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
```

### **4. Plugin Sandboxing (Ardour-Specific)**

```cpp
// ❌ DON'T: Implement plugin sandboxing
// Ardour explicitly doesn't sandbox plugins for performance reasons

// ✅ DO: Follow Ardour's plugin architecture
// Direct plugin loading with proper error handling
```

---

## 📚 **Documentation Standards**

### **For Code Changes**

- Document the **root cause** of the issue
- Explain **why** the change is justified
- List **alternatives considered** and why they were rejected
- Include **evidence** that the change is necessary
- Reference **Ardour's architecture patterns** where applicable

### **For Build System Solutions**

- Document the **environment setup** required
- Explain **why** no code changes were needed
- Provide **reproducible steps** for the solution

### **For Avoided Changes**

- Document **what** was considered
- Explain **why** it was rejected
- Show the **clean alternative** used instead
- Reference **Ardour's development philosophy** where relevant

---

## 🎯 **Quality Assurance Checklist**

### **Before Making Code Changes**

- [ ] Is this issue actually a code problem?
- [ ] Have I tried all build system solutions?
- [ ] Have I tried all environment solutions?
- [ ] Is there a clean alternative that doesn't modify code?
- [ ] Is this change justified and necessary?
- [ ] Does this respect Ardour's async callback architecture?
- [ ] Does this follow Ardour's MVC patterns?

### **After Making Code Changes**

- [ ] Is the change minimal and focused?
- [ ] Is the change well-documented?
- [ ] Does the change follow existing patterns?
- [ ] Is the change platform-agnostic where possible?
- [ ] Have I tested the change thoroughly?
- [ ] Does this integrate properly with Ardour's plugin system?

---

## 🔗 **Ardour Development Resources**

### **Official Documentation**

- [Development Guide](https://ardour.org/development.html)
- [Building on macOS](https://ardour.org/building_osx_native.html)

### **Technical Notes**

- Transport Threading design
- Canvas editing window notes
- Event handling in GUI
- Cross-thread notifications/callbacks
- MIDI data handling

### **Community Resources**

- IRC: Real-time development discussions
- Mailing List: ardour-dev
- Bug Tracker: For issues and features
- GitHub Mirror: For pull requests and contributions
