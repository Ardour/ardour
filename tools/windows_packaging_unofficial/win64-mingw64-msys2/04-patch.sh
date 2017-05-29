#!/bin/sh
# Run this in an MSYS2 shell.
#
# IMPORTANT: These changes to the Ardour source code aren't tested on other
# platforms and will likely break Linux support. Please submit a pull request 
# if you test these changes on Linux and you end up having to fix something. 
# The goal is to patch the source so it still builds on all other platforms.

export ARG1="${ARG1:-$1}"

patch_ardour()
{
	SOURCE="${SOURCE:-.}"
	INCLUDE_SCRIPT="${INCLUDE_SCRIPT:-00-00-conf.sh}"
	$SOURCE "./$INCLUDE_SCRIPT"
	cd "$BUILD_DIR/$SRC_REPO_NAME" &&
	sed -i "s/compiler_flags = \[\]/compiler_flags = \[\"-Wa,-mbig-obj\"\]/g" wscript &&
	sed -i "s/assert (0 == (((unsigned long)mem_pool) \& PTR_MASK))\;/assert (0 == ((strtoul(mem_pool, NULL, 0)) \& PTR_MASK))\;/g" libs/pbd/tlsf.cc &&
	sed -i "s/^static uint64_t/uint64_t/g" libs/pbd/fpu.cc &&
	cd "$BUILD_SCRIPT_DIR"
}

patch_ardour
