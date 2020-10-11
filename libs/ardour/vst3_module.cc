/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#if (__cplusplus < 201103L)
#  define nullptr 0
#endif

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#undef nil
#elif defined PLATFORM_WINDOWS
#include <windows.h>
#include <glibmm.h>
#else
#include <dlfcn.h>
#endif

#include <glibmm/miscutils.h>

#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"

#include "pluginterfaces/base/ipluginbase.h"
#include "ardour/vst3_module.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

#ifdef __APPLE__

class VST3MacModule : public VST3PluginModule
{
public:
	VST3MacModule (std::string const& module_path)
	{
		std::string path = Glib::path_get_dirname (module_path); // Contents
		path = Glib::path_get_dirname (path); // theVST.vst3
		CFURLRef url = CFURLCreateFromFileSystemRepresentation (0, (const UInt8*)path.c_str (), (CFIndex)path.length (), true);
		if (url) {
			_bundle = CFBundleCreate (kCFAllocatorDefault, url);
			CFRelease (url);
		}

		if (!_bundle) {
			throw failed_constructor ();
		}

		if (!CFBundleLoadExecutable (_bundle)) {
			CFRelease (_bundle);
			_bundle = 0;
			throw failed_constructor ();
		}

		if (!init ()) {
			CFRelease (_bundle);
			_bundle = 0;
			throw failed_constructor ();
		}
	}

	~VST3MacModule ()
	{
		release_factory ();
		if (_bundle) {
			exit ();
			CFRelease (_bundle);
		}
	}

	void* fn_ptr (const char* name) const
	{
		CFStringRef str = CFStringCreateWithCString (NULL, name, kCFStringEncodingUTF8);
		void*       fn  = CFBundleGetFunctionPointerForName (_bundle, str);
		if (str) {
			CFRelease (str);
		}
		return fn;
	}

private:
	bool init ()
	{
		typedef bool (*init_fn_t) (CFBundleRef);
		init_fn_t fn = (init_fn_t)fn_ptr ("bundleEntry");
		return (fn && fn (_bundle));
	}

	bool exit ()
	{
		typedef bool (*exit_fn_t) ();
		exit_fn_t fn = (exit_fn_t)fn_ptr ("bundleExit");
		return (fn && fn ());
	}

	CFBundleRef _bundle;
};

#elif defined PLATFORM_WINDOWS

class VST3WindowsModule : public VST3PluginModule
{
public:
	VST3WindowsModule (const std::string& path)
	{
		if ((_handle = LoadLibraryA (Glib::locale_from_utf8 (path).c_str ())) == 0) {
			throw failed_constructor ();
		}

		if (!init ()) {
			FreeLibrary (_handle);
			_handle = 0;
			throw failed_constructor ();
		}
	}

	~VST3WindowsModule ()
	{
		release_factory ();
		if (_handle) {
			exit ();
			FreeLibrary (_handle);
		}
	}

	void* fn_ptr (const char* name) const
	{
		return (void*)GetProcAddress (_handle, name);
	}

private:
	bool init ()
	{
		typedef bool(__stdcall * init_fn_t) ();
		init_fn_t fn = (init_fn_t)fn_ptr ("InitDll");
		return (!fn || fn ()); // init is optional
	}

	bool exit ()
	{
		typedef bool(__stdcall * exit_fn_t) ();
		exit_fn_t fn = (exit_fn_t)fn_ptr ("ExitDll");
		return (!fn || fn ()); // exit is optional
	}

	HMODULE _handle;
};

#else

class VST3LinuxModule : public VST3PluginModule
{
public:
	VST3LinuxModule (std::string const& path)
	{
		if ((_dll = dlopen (path.c_str (), RTLD_LOCAL | RTLD_LAZY)) == 0) {
			PBD::error << string_compose (_("Could not load VST3 plugin '%1': %2"), path, dlerror ()) << endmsg;
			throw failed_constructor ();
		}

		void* m_entry = dlsym (_dll, "ModuleEntry");
		void* m_exit  = dlsym (_dll, "ModuleExit");

		if (!m_entry || !m_exit) {
			PBD::error << string_compose (_("Invalid VST3 plugin: '%1'"), path) << endmsg;
			dlclose (_dll);
			_dll = 0;
			throw failed_constructor ();
		}

		if (!init ()) {
			dlclose (_dll);
			_dll = 0;
			throw failed_constructor ();
		}
	}

	~VST3LinuxModule ()
	{
		release_factory ();
		if (_dll) {
			exit ();
			dlclose (_dll);
		}
	}

	void* fn_ptr (const char* name) const
	{
		return dlsym (_dll, name);
	}

private:
	bool init ()
	{
		typedef bool (*init_fn_t) (void*);
		init_fn_t fn = (init_fn_t)fn_ptr ("ModuleEntry");
		return (fn && fn (_dll));
	}

	bool exit ()
	{
		typedef bool (*exit_fn_t) ();
		exit_fn_t fn = (exit_fn_t)fn_ptr ("ModuleExit");
		return (fn && fn ());
	}

	void* _dll;
};

#endif

Steinberg::IPluginFactory*
VST3PluginModule::factory ()
{
	if (!_factory) {
		GetFactoryProc fp = (GetFactoryProc)fn_ptr ("GetPluginFactory");
		if (fp) {
			_factory = fp ();
		}
	}
	return _factory;
}

void
VST3PluginModule::release_factory ()
{
	if (_factory) {
		_factory->release ();
	}
}

boost::shared_ptr<VST3PluginModule>
VST3PluginModule::load (std::string const& path)
{
#ifdef __APPLE__
	return boost::shared_ptr<VST3PluginModule> (new VST3MacModule (path));
#elif defined PLATFORM_WINDOWS
	return boost::shared_ptr<VST3PluginModule> (new VST3WindowsModule (path));
#else
	return boost::shared_ptr<VST3PluginModule> (new VST3LinuxModule (path));
#endif
}
