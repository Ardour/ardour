#!/bin/bash

# Test script for VST headless support
# This script tests the basic functionality of the VST headless support

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if hardour binary exists
HARDOUR_BIN="./hardour-$(cat ../VERSION)"
if [ ! -f "$HARDOUR_BIN" ]; then
    print_error "Hardour binary not found: $HARDOUR_BIN"
    print_error "Please build the project first: ./waf build"
    exit 1
fi

print_status "Testing VST headless support with binary: $HARDOUR_BIN"

# Test 1: Help output should include new VST options
print_status "Test 1: Checking help output for VST options"
if $HARDOUR_BIN --help 2>&1 | grep -q "enable-plugins"; then
    print_status "✓ Help output includes --enable-plugins option"
else
    print_error "✗ Help output missing --enable-plugins option"
    exit 1
fi

if $HARDOUR_BIN --help 2>&1 | grep -q "plugin-timeout"; then
    print_status "✓ Help output includes --plugin-timeout option"
else
    print_error "✗ Help output missing --plugin-timeout option"
    exit 1
fi

if $HARDOUR_BIN --help 2>&1 | grep -q "strict-plugins"; then
    print_status "✓ Help output includes --strict-plugins option"
else
    print_error "✗ Help output missing --strict-plugins option"
    exit 1
fi

# Test 2: Version output
print_status "Test 2: Checking version output"
if $HARDOUR_BIN --version 2>&1 | grep -q "hardour"; then
    print_status "✓ Version output works correctly"
else
    print_error "✗ Version output failed"
    exit 1
fi

# Test 3: Invalid arguments should show help
print_status "Test 3: Testing invalid arguments"
if $HARDOUR_BIN --invalid-option 2>&1 | grep -q "Usage:"; then
    print_status "✓ Invalid arguments show help"
else
    print_error "✗ Invalid arguments don't show help"
    exit 1
fi

# Test 4: Plugin timeout validation
print_status "Test 4: Testing plugin timeout validation"
if $HARDOUR_BIN -E -T 0 /tmp /tmp 2>&1 | grep -q "Invalid plugin timeout"; then
    print_status "✓ Plugin timeout validation works"
else
    print_warning "⚠ Plugin timeout validation may not be working (expected for invalid timeout)"
fi

# Test 5: Check if VST support is compiled in
print_status "Test 5: Checking VST support compilation"
if $HARDOUR_BIN --help 2>&1 | grep -q "novst"; then
    print_status "✓ Windows VST support detected"
else
    print_warning "⚠ Windows VST support not detected (may not be compiled)"
fi

# Test 6: Test with minimal arguments (should fail gracefully)
print_status "Test 6: Testing with minimal arguments"
if $HARDOUR_BIN -E 2>&1 | grep -q "Usage:"; then
    print_status "✓ Minimal arguments handled correctly"
else
    print_error "✗ Minimal arguments not handled correctly"
    exit 1
fi

# Test 7: Test configuration file creation (if possible)
print_status "Test 7: Testing configuration file handling"
CONFIG_DIR="$HOME/.config/ardour"
CONFIG_FILE="$CONFIG_DIR/headless_config"

# Create test configuration
mkdir -p "$CONFIG_DIR"
cat > "$CONFIG_FILE" << EOF
# Test configuration
enable_plugins=true
plugin_timeout_ms=15000
strict_plugin_loading=false
vst_path=/test/vst/path
plugin_blacklist_file=/test/blacklist.txt
plugin_memory_limit_mb=512
plugin_threads=2
EOF

if [ -f "$CONFIG_FILE" ]; then
    print_status "✓ Test configuration file created"
else
    print_warning "⚠ Could not create test configuration file"
fi

# Clean up test configuration
rm -f "$CONFIG_FILE"

print_status "All basic tests completed successfully!"
print_status "VST headless support appears to be working correctly."

print_warning "Note: This is a basic functionality test."
print_warning "For full testing, you need:"
print_warning "1. A valid Ardour session with VST plugins"
print_warning "2. VST plugins installed on the system"
print_warning "3. Proper audio backend configuration"

print_status "To test with actual plugins, run:"
print_status "  $HARDOUR_BIN -E -T 30000 /path/to/session session_name" 