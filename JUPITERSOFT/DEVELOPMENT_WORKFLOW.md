# 🔄 Development Workflow & Best Practices

## 📋 Overview

This document outlines the **clean development workflow** established during this project, focusing on **minimal code changes** and **justified modifications only**.

---

## 🎯 **Core Principles**

### 1. **Minimal Code Changes**

- ✅ Only modify code when **absolutely necessary**
- ✅ Prefer build system solutions over code changes
- ✅ Use environment variables and configure flags first

### 2. **Justified Changes Only**

- ✅ Platform compatibility issues with no clean alternative
- ✅ Build system bugs that affect functionality
- ✅ Header conflicts that break compilation
- ❌ Environment-specific workarounds
- ❌ Platform-specific guards for non-platform issues

### 3. **Clean Alternatives First**

- ✅ Build system flags (`--boost-include`, `--also-include`)
- ✅ Environment variables (`CPPFLAGS`, `LDFLAGS`)
- ✅ pkg-config and standard build practices
- ❌ Hardcoded paths in wscript files
- ❌ Platform-specific code wrapping

---

## 🔧 **Problem-Solving Workflow**

### **Step 1: Identify the Issue**

```bash
# Get detailed error information
./waf build -v 2>&1 | tee build.log

# Analyze the error
grep -i "error\|fatal\|not found" build.log
```

### **Step 2: Determine Root Cause**

- **Dependency Issue?** → Use build system flags
- **Platform Compatibility?** → Check if code change is justified
- **Build System Bug?** → Verify it's a real bug, not environment issue
- **Header Conflict?** → Look for clean include path solutions

### **Step 3: Choose Solution Strategy**

#### **Option A: Build System Solution (Preferred)**

```bash
# Missing headers
./waf configure --boost-include=/opt/homebrew/include

# Additional include paths
./waf configure --also-include=/opt/homebrew/include

# Environment variables
export CPPFLAGS="-I/opt/homebrew/include"
./waf configure
```

#### **Option B: Code Change (Only if Justified)**

```cpp
// Platform compatibility with no alternative
#ifndef __APPLE__
__attribute__((weak, alias("symbol")))
#endif

// Build system bug fix
if bld.is_defined('YTK'):
    obj.uselib = 'correct_libs'
else:
    obj.uselib = 'other_libs'
```

#### **Option C: Avoid/Work Around**

- Environment-specific issues
- Temporary build problems
- Issues caused by other problems

---

## 📊 **Decision Matrix**

| Issue Type             | Build System         | Code Change          | Avoid |
| ---------------------- | -------------------- | -------------------- | ----- |
| Missing Headers        | ✅ `--also-include`  | ❌                   | ❌    |
| Library Paths          | ✅ `--boost-include` | ❌                   | ❌    |
| Platform Compatibility | ❌                   | ✅ If no alternative | ❌    |
| Build System Bug       | ❌                   | ✅ If real bug       | ❌    |
| Header Conflicts       | ✅ Include path fix  | ✅ If cleanest       | ❌    |
| Environment Issues     | ✅ Env vars          | ❌                   | ✅    |
| Temporary Problems     | ❌                   | ❌                   | ✅    |

---

## 🔍 **Justification Checklist**

Before making any code change, verify:

### **✅ Is it Justified?**

- [ ] **No clean alternative exists** (build system, environment, etc.)
- [ ] **Platform compatibility issue** with no workaround
- [ ] **Build system bug** that affects functionality
- [ ] **Header conflict** that breaks compilation

### **✅ Is it Minimal?**

- [ ] **Smallest possible change** to fix the issue
- [ ] **No collateral damage** to other platforms/components
- [ ] **Follows existing patterns** in the codebase

### **✅ Is it Documented?**

- [ ] **Clear justification** for why code change was needed
- [ ] **Alternatives considered** and why they were rejected
- [ ] **Impact assessment** on other platforms/components

---

## 🚫 **Anti-Patterns to Avoid**

### **1. Environment-Specific Code Changes**

```cpp
// ❌ DON'T: Hardcode paths for specific environment
#ifdef __APPLE__
#include "/opt/homebrew/include/boost/version.hpp"
#endif

// ✅ DO: Use build system or environment variables
#include <boost/version.hpp>
```

### **2. Platform Guards for Non-Platform Issues**

```cpp
// ❌ DON'T: Wrap code that's not platform-specific
#ifdef _WIN32
// Windows-specific code here
#endif

// ✅ DO: Only wrap truly platform-specific code
#ifdef _WIN32
// Windows-specific API calls
#endif
```

### **3. Build System Workarounds**

```python
# ❌ DON'T: Hardcode paths in wscript
obj.includes = ['/opt/homebrew/include']

# ✅ DO: Use configure flags or environment
obj.includes = ['.']
# Then: ./waf configure --also-include=/opt/homebrew/include
```

---

## 📝 **Documentation Standards**

### **For Code Changes:**

```cpp
// Justification: Platform compatibility - Clang doesn't support GNU alias attributes
// Alternative considered: Using __attribute__((weak)) alone - breaks symbol resolution
// Impact: Enables compilation on macOS, no impact on other platforms
#ifndef __APPLE__
__attribute__((weak, alias("symbol")))
#endif
```

### **For Build System Changes:**

```python
# Justification: YTK build system bug - incorrectly references GTK2 when YTK enabled
# Alternative considered: None - this was a bug in the YTK migration
# Impact: Fixes YTK build, no impact on GTK2 builds
if bld.is_defined('YTK'):
    obj.uselib = 'correct_libs'
```

### **For Documentation:**

- **Clear problem statement** with error messages
- **Root cause analysis** with evidence
- **Solution justification** with alternatives considered
- **Impact assessment** on other platforms/components

---

## 🔄 **Review Process**

### **Before Committing:**

1. **Verify justification** using the checklist above
2. **Test on multiple platforms** if possible
3. **Document the change** with clear reasoning
4. **Consider alternatives** and why they were rejected

### **During Code Review:**

1. **Challenge every code change** - is it really necessary?
2. **Look for build system alternatives** first
3. **Verify platform impact** assessment
4. **Check documentation quality** and completeness

---

## 📚 **Related Documentation**

- [Build Issues & Solutions](build_issues.md)
- [macOS Development Guide](macos_development.md)
- [Build System Usage](build_system.md)
- [Codebase Patterns](codebase_patterns.md)
