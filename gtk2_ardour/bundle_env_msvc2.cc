/*
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

#include <stdlib.h>
#include <string>
#include <filesystem>
#include <iostream>
#include <vector>
#include "bundle_env.h"
#include "pbd/i18n.h"

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>
#include <fontconfig/fontconfig.h>
#include <pango/pangocairo.h>
#include <pango/pangoft2.h>

#include <windows.h>

#include "ardour/ardour.h"
#include "ardour/search_paths.h"
#include "ardour/filesystem_paths.h"

#include "pbd/file_utils.h"
#include "pbd/epa.h"

using namespace PBD;
using namespace ARDOUR;
namespace fs = std::filesystem;

static std::vector<std::string> g_loaded_fonts;

static void __cdecl unload_custom_fonts() 
{
    for (const auto& font_path : g_loaded_fonts) {
        RemoveFontResourceExA(font_path.c_str(), FR_PRIVATE, 0);
    }
    g_loaded_fonts.clear();
}

static LONG WINAPI unload_font_at_exception(PEXCEPTION_POINTERS /*pExceptionInfo*/) 
{
    unload_custom_fonts();
    return EXCEPTION_CONTINUE_SEARCH;
}

void
fixup_bundle_environment (int, char* [], std::string & localedir)
{
	if (!g_getenv ("ARDOUR_BUNDLED")) {
		return;
	}

	/* Support inheriting environment protection if already set */
	if (g_getenv ("PREBUNDLE_ENV")) {
		EnvironmentalProtectionAgency::set_global_epa (new EnvironmentalProtectionAgency (true, "PREBUNDLE_ENV"));
	} else {
		EnvironmentalProtectionAgency::set_global_epa (new EnvironmentalProtectionAgency (true));
	}
	Glib::unsetenv("GTK2_RC_FILES");
	
	const fs::path dll_dir = fs::path(ardour_dll_directory()).make_preferred();
	const fs::path root_dir = dll_dir.parent_path();

	/* Modern DPI Awareness fallback chain: V2 -> V1 -> System */
	HMODULE user32 = GetModuleHandleW(L"user32.dll");
	if (user32) {
		using SetDpiFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
		auto set_dpi = (SetDpiFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
		if (set_dpi) {
			if (!set_dpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
				set_dpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
			}
		}
	}

	if (ARDOUR::translations_are_enabled()) {
		fs::path lp = fs::path(windows_search_path().to_string()) / "locale";
		localedir = lp.string();
		Glib::setenv("GTK_LOCALEDIR", localedir, true);
	}

	auto extend_env = [](const std::string& var, const std::string& val) {
		const char* existing = g_getenv(var.c_str());
		/* Expand Windows %Var% strings if present in the new path */
		char expanded[MAX_PATH];
		ExpandEnvironmentStringsA(val.c_str(), expanded, MAX_PATH);
		std::string new_val = std::string(expanded) + (existing ? (G_SEARCHPATH_SEPARATOR + std::string(existing)) : "");
		Glib::setenv(var, new_val, true);
	};

	Glib::setenv("ARDOUR_DATA_PATH", (root_dir / "share").make_preferred().string(), true);
	Glib::setenv("ARDOUR_CONFIG_PATH", (root_dir / "etc").make_preferred().string(), true);
	Glib::setenv("GTK_PATH", (dll_dir / "gtk-2.0").make_preferred().string(), true);

	/* Set up ARDOUR_DLL_PATH for plugin discovery (surfaces, panners, backends) */
	std::string dll_path = dll_dir.string();
	dll_path += G_SEARCHPATH_SEPARATOR + (dll_dir / PROGRAM_NAME PROGRAM_VERSION / "surfaces").make_preferred().string();
	dll_path += G_SEARCHPATH_SEPARATOR + (dll_dir / PROGRAM_NAME PROGRAM_VERSION / "panners").make_preferred().string();
	dll_path += G_SEARCHPATH_SEPARATOR + (dll_dir / PROGRAM_NAME PROGRAM_VERSION / "backends").make_preferred().string();
	Glib::setenv("ARDOUR_DLL_PATH", dll_path, true);

	/* Ensure child processes find bundled executables and DLLs */
	std::string current_path = g_getenv("PATH") ? g_getenv("PATH") : "";
	Glib::setenv("PATH", (dll_dir.string() + G_SEARCHPATH_SEPARATOR + current_path), true);

	extend_env("VAMP_PATH", (dll_dir / "vamp").make_preferred().string());
	extend_env("VAMP_PATH", "%ProgramFiles%\\Vamp Plugins"); 
	extend_env("VAMP_PATH", "%CommonProgramFiles%\\Vamp Plugins");

	Glib::setenv("SUIL_MODULE_DIR", (dll_dir / "suil").make_preferred().string(), true);
	Glib::setenv("ARDOUR_SELF", (dll_dir / "ardour.exe").make_preferred().string(), true);

	/* Prevent GTK from looking outside the bundle for charset aliases */
	Glib::setenv ("CHARSETALIASDIR", root_dir.string(), true);

	/* If Fontconfig is used, point it to the bundled config if it exists */
	fs::path fc_cfg = root_dir / "etc" / "fonts" / "fonts.conf";
	if (fs::exists(fc_cfg)) {
		Glib::setenv ("FONTCONFIG_FILE", fc_cfg.string(), true);
	}
}

void
load_custom_fonts ()
{
	/* Support FontConfig if Pango is using FreeType backend */
	const bool use_fc = (pango_font_map_get_type() == PANGO_TYPE_FT2_FONT_MAP) || 
	                    (g_getenv("PANGOCAIRO_BACKEND") && std::string(g_getenv("PANGOCAIRO_BACKEND")) == "fc");

	FcConfig* config = use_fc ? FcInitLoadConfigAndFonts() : nullptr;

	auto sp = ardour_data_search_path();
	bool fonts_loaded = false;

	auto load_font = [&](const std::string& name) {
		std::string p;
		if (find_file(sp, name, p)) {
			bool success = false;
			if (config) {
				success = FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(p.c_str()));
			}
			/* Always attempt GDI registration as a robust fallback/primary */
			if (AddFontResourceExA(p.c_str(), FR_PRIVATE, 0)) {
				g_loaded_fonts.push_back(p);
				success = true;
			} else if (!success) {
				std::cerr << _("Cannot register font with Windows GDI: ") << name << std::endl;
			}
			if (success) fonts_loaded = true;
		} else {
			std::cerr << _("Cannot find font file: ") << name << std::endl;
		}
	};

	load_font("ArdourMono.ttf");
	load_font("ArdourSans.ttf");

	if (fonts_loaded) {
		/* Notify Pango that the font database has changed */
		pango_cairo_font_map_set_default(nullptr);
		atexit(unload_custom_fonts);
		SetUnhandledExceptionFilter(unload_font_at_exception);
	}
	
	if (config && FcConfigSetCurrent(config) == FcFalse) {
		std::cerr << _("Failed to set fontconfig configuration.") << std::endl;
	}
}

