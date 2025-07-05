/*
 * Copyright (C) 2024 Ardour Development Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ardour/mac_vst_support.h"
#include "ardour/libardour_visibility.h"

#ifdef MACVST_SUPPORT

// Headless flag for MacVST initialization
#define MAC_VST_FLAG_HEADLESS 0x1000

LIBARDOUR_API int mac_vst_init_headless(int flags)
{
    // Initialize MacVST for headless operation
    // Set up dummy Core Graphics context or use alternative context
    // Configure headless-specific VST host callbacks

    return mac_vst_init(flags | MAC_VST_FLAG_HEADLESS);
}

LIBARDOUR_API void mac_vst_exit_headless()
{
    // Clean up headless MacVST environment
    mac_vst_exit();
}

#else

LIBARDOUR_API int mac_vst_init_headless(int flags)
{
    // Stub implementation for non-Mac platforms
    return 0;
}

LIBARDOUR_API void mac_vst_exit_headless()
{
    // Stub implementation for non-Mac platforms
}

#endif