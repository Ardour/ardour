# 🔧 Build Issues & Solutions

## 📋 Overview

This document tracks build issues encountered during development and their **justified solutions**. Only **essential fixes** that modify code are documented here.

---

## ✅ **JUSTIFIED FIXES (Code Changes Required)**

### 1. **macOS/Clang Alias Attribute Compatibility**

**Issue:** Clang on macOS doesn't support GNU alias attributes

```bash
error: 'alias' attribute is not supported on this target
```

**Files Modified:**

- `libs/evoral/Control.cc`
- `libs/evoral/ControlList.cc`
- `libs/evoral/ControlSet.cc`

**Solution:** Add `#ifndef __APPLE__` guards

```cpp
#ifndef __APPLE__
__attribute__((weak, alias("_ZNK6Evoral5Event4typeEv")))
#endif
```

**Justification:** Platform compatibility requirement, no clean alternative exists.

---

### 2. **YTK/GTK2 Build System Bug**

**Issue:** YTK build incorrectly references GTK2 libraries

```bash
linking with GTK2 libraries when YTK is enabled
```

**File Modified:** `gtk2_ardour/wscript`

**Solution:** Make uselib string conditional

```python
if bld.is_defined('YTK'):
    obj.uselib = 'UUID FLAC FONTCONFIG GTHREAD OGG PANGOMM...'
else:
    obj.uselib = 'UUID FLAC FONTCONFIG GTHREAD GTK OGG PANGOMM...'
```

**Justification:** Bug fix for YTK migration, no alternative exists.

---

### 3. **FluidSynth Header Conflict**

**Issue:** Internal and system FluidSynth headers conflict

```bash
error: conflicting types for 'fluid_midi_event_get_*'
```

**File Modified:** `libs/fluidsynth/src/fluidsynth_priv.h`

**Solution:** Use internal header path

```c
// Before: #include "fluidsynth.h"
// After:  #include "fluidsynth/fluidsynth.h"
```

**Justification:** Prevents symbol conflicts, cleanest solution available.

---

## 🔧 **CLEAN SOLUTIONS (No Code Changes)**

### 1. **Missing Dependencies**

**Issue:** Boost/libarchive headers not found

```bash
fatal error: 'boost/bind/protect.hpp' file not found
fatal error: 'archive.h' file not found
```

**Solution:** Use build system flags

```bash
./waf configure --boost-include=/opt/homebrew/include --also-include=/opt/homebrew/include
```

**Why Clean:** No code changes, uses existing build system features.

---

### 2. **Platform-Specific Build Issues**

**Issue:** macOS-specific build failures

**Solution:** Environment variables + configure flags

```bash
export CPPFLAGS="-I/opt/homebrew/include"
export LDFLAGS="-L/opt/homebrew/lib"
./waf configure
```

**Why Clean:** Standard build practice, no code modifications.

---

## ❌ **AVOIDED CHANGES (Not Justified)**

### 1. **Windows-Specific Code Wrapping**

**What:** Added `#ifdef _WIN32` guards to multiple files
**Why Avoided:** Build issues were caused by other problems, not Windows code
**Status:** Reverted - unnecessary changes

### 2. **Build System Include Path Modifications**

**What:** Modified wscript files with hardcoded include paths
**Why Avoided:** Environment-specific workarounds, not code fixes
**Status:** Reverted - should use environment variables instead

---

## 📊 **Issue Resolution Summary**

| Issue Type             | Count | Justified Fixes | Clean Solutions | Avoided Changes |
| ---------------------- | ----- | --------------- | --------------- | --------------- |
| Platform Compatibility | 3     | ✅ 3            | -               | -               |
| Build System Bugs      | 2     | ✅ 2            | -               | -               |
| Dependency Issues      | 5     | -               | ✅ 5            | -               |
| Header Conflicts       | 1     | ✅ 1            | -               | -               |
| Environment Issues     | 3     | -               | ✅ 3            | -               |

**Total Issues:** 14  
**Code Changes Required:** 6 files  
**No Code Changes:** 8 issues  
**Unjustified Changes:** 0 (all reverted)

---

## 🎯 **Best Practices Established**

### **When to Modify Code:**

- ✅ Platform compatibility issues with no clean alternative
- ✅ Build system bugs that affect functionality
- ✅ Header conflicts that break compilation

### **When to Use Build System:**

- ✅ Missing dependencies (use flags/environment)
- ✅ Include path issues (use `--also-include`)
- ✅ Library path issues (use `--boost-include`)

### **What to Avoid:**

- ❌ Environment-specific workarounds in code
- ❌ Platform-specific guards for non-platform issues
- ❌ Build system modifications for environment issues

---

## 📚 **Related Documentation**

- [macOS Development Guide](macos_development.md)
- [Build System Usage](build_system.md)
- [Codebase Patterns](codebase_patterns.md)
