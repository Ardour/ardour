# Development Workflow

## Git Workflow

### Branch Management

- **Master**: Upstream Ardour master branch (don't modify)
- **jupitersoft**: Your development branch for JUPITERSOFT features
- **Feature branches**: Create for specific features or fixes

### Critical Rules

1. **Never change branches without permission** - See `.cursor/rules/NO_BRANCH_CHANGE.md`
2. **Don't modify upstream master** - This is an upstream project
3. **Commit frequently** - Small, focused commits are better
4. **Test before committing** - Ensure builds and basic functionality work

### Branch Operations

```bash
# Check current branch
git branch

# Create feature branch
git checkout -b feature/vst-headless-improvements

# Switch back to main development branch
git checkout jupitersoft

# Merge feature branch
git merge feature/vst-headless-improvements

# Delete feature branch
git branch -d feature/vst-headless-improvements
```

### Commit Guidelines

```bash
# Good commit message format
git commit -m "Add timeout protection for VST plugin loading

- Implement TimeoutWrapper class for plugin operations
- Add 5-second default timeout for plugin initialization
- Handle timeout errors gracefully with proper cleanup
- Fixes issue #123: plugins hanging during load"

# Bad commit message
git commit -m "fix stuff"
```

## Development Process

### 1. Environment Setup

```bash
# Set up build environment
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:/opt/homebrew/opt/libarchive/lib/pkgconfig"
export CPATH="/opt/homebrew/include:/opt/homebrew/opt/boost/include"
export CPLUS_INCLUDE_PATH="$CPATH"
export LDFLAGS="-L/opt/homebrew/opt/libarchive/lib"
export CPPFLAGS="-I/opt/homebrew/opt/libarchive/include"
```

### 2. Code Changes

- **Focus on your domain**: Don't modify GUI code unless necessary
- **Follow existing patterns**: Match the codebase style
- **Add documentation**: Document new features and changes
- **Test incrementally**: Test after each significant change

### 3. Build Testing

```bash
# Clean build
./waf clean

# Configure
./waf configure --strict --with-backends=jack,coreaudio,dummy --ptformat --optimize

# Build
./waf -j$(sysctl -n hw.logicalcpu)

# Test basic functionality
cd gtk2_ardour && ./ardev
```

### 4. Testing Procedures

#### Unit Testing

```bash
# Run specific tests
./waf test --test-name=test_vst_loading

# Run all tests
./waf test

# Run tests with verbose output
./waf test -v
```

#### Integration Testing

```bash
# Test VST plugin loading
./headless/test_vst_support.sh

# Test specific plugin
./headless/test_vst_support.sh /path/to/plugin.vst

# Test plugin directory
./headless/test_vst_support.sh /path/to/plugin/directory
```

#### Manual Testing

1. **Build verification**: Ensure clean build succeeds
2. **Runtime testing**: Test basic Ardour functionality
3. **Feature testing**: Test specific features you've added
4. **Error handling**: Test error conditions and recovery

## Code Quality

### Code Style

- **Follow existing conventions**: Match the codebase style
- **Use consistent naming**: Follow established naming patterns
- **Add comments**: Document complex logic
- **Keep functions small**: Single responsibility principle

### Error Handling

```cpp
// Good error handling pattern
try {
    auto plugin = load_plugin(plugin_path);
    if (!plugin) {
        throw std::runtime_error("Failed to load plugin: " + plugin_path);
    }
    // Process plugin...
} catch (const std::exception& e) {
    PluginErrorHandler::handle_plugin_error(plugin_path, e.what());
    return false;
}
```

### Memory Management

```cpp
// Use smart pointers
std::shared_ptr<Plugin> plugin = std::make_shared<Plugin>();

// RAII for resources
class PluginResource {
public:
    PluginResource() { /* acquire resource */ }
    ~PluginResource() { /* release resource */ }
};
```

## Debugging

### Build Debugging

```bash
# Verbose build output
./waf build -v

# Check specific component
./waf build -v headless

# Check configuration
cat build/config.log | grep -i "not found"
```

### Runtime Debugging

```bash
# Run with debug output
./ardev --debug

# Run with specific log level
./ardev --log-level=debug

# Run with plugin debugging
./ardev --debug-plugins
```

### Code Debugging

```cpp
// Add debug output
#ifdef DEBUG
    std::cout << "Loading plugin: " << plugin_path << std::endl;
#endif

// Use logging system
PBD::info << "Plugin loaded successfully: " << plugin_name << endmsg;
```

## Documentation

### Code Documentation

- **Header comments**: Document class and function purposes
- **Inline comments**: Explain complex logic
- **API documentation**: Document public interfaces
- **Examples**: Provide usage examples

### Update Documentation

When making changes:

1. **Update relevant docs**: Modify affected documentation
2. **Add new docs**: Create documentation for new features
3. **Update examples**: Ensure examples are current
4. **Review existing docs**: Check for outdated information

## Code Review

### Self Review Checklist

Before committing:

- [ ] Code compiles without warnings
- [ ] All tests pass
- [ ] Documentation is updated
- [ ] Error handling is implemented
- [ ] Memory management is correct
- [ ] Code follows style guidelines
- [ ] No debug code left in

### Review Process

1. **Self review**: Check your own code first
2. **Build verification**: Ensure clean build
3. **Test execution**: Run relevant tests
4. **Documentation check**: Verify docs are current
5. **Commit**: Only commit when satisfied

## Performance Considerations

### Build Performance

```bash
# Use parallel builds
./waf -j$(sysctl -n hw.logicalcpu)

# Incremental builds
./waf  # Only rebuilds changed files

# Clean when needed
./waf clean  # Force full rebuild
```

### Runtime Performance

- **Profile code**: Use profiling tools to identify bottlenecks
- **Optimize algorithms**: Choose efficient algorithms
- **Minimize allocations**: Reuse objects when possible
- **Cache results**: Cache expensive computations

## Security

### Code Security

- **Validate inputs**: Always validate user inputs
- **Check file paths**: Validate file paths before use
- **Handle errors**: Don't expose internal errors to users
- **Use secure defaults**: Choose secure default configurations

### Plugin Security

- **Validate plugins**: Check plugin integrity before loading
- **Sandbox when possible**: Isolate plugins from system
- **Monitor behavior**: Watch for suspicious plugin activity
- **Timeout operations**: Prevent hanging plugins

## Continuous Integration

### Automated Testing

- **Build verification**: Ensure code builds on all platforms
- **Unit testing**: Run automated unit tests
- **Integration testing**: Test component interactions
- **Performance testing**: Monitor performance regressions

### Quality Gates

- **Build success**: All builds must pass
- **Test coverage**: Maintain minimum test coverage
- **Code quality**: Pass static analysis checks
- **Documentation**: Ensure documentation is current

## Release Process

### Pre-release Checklist

- [ ] All tests pass
- [ ] Documentation is complete
- [ ] Performance is acceptable
- [ ] Security review completed
- [ ] Release notes prepared
- [ ] Version numbers updated

### Release Steps

1. **Final testing**: Comprehensive testing
2. **Documentation review**: Verify all docs are current
3. **Version bump**: Update version numbers
4. **Tag release**: Create git tag
5. **Build artifacts**: Create release builds
6. **Announcement**: Notify community

## Community Interaction

### Communication

- **Be respectful**: Treat community members with respect
- **Provide context**: Explain your changes and reasoning
- **Accept feedback**: Be open to suggestions and criticism
- **Help others**: Assist other developers when possible

### Contribution Guidelines

- **Follow conventions**: Adhere to project conventions
- **Test thoroughly**: Ensure your changes work correctly
- **Document changes**: Explain what and why you changed
- **Be patient**: Understand that review takes time
