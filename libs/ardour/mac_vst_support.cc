/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2012 Paul Davis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <dlfcn.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <glib.h>
#include "pbd/gstdio_compat.h"
#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include "ardour/mac_vst_support.h"
#include "pbd/basename.h"
#include "pbd/error.h"

#include "pbd/i18n.h"

#include <Carbon/Carbon.h>

/*Simple error handler stuff for VSTFX*/

void mac_vst_error (const char *fmt, ...)
{
	va_list ap;
	char buffer[512];

	va_start (ap, fmt);
	vsnprintf (buffer, sizeof (buffer), fmt, ap);
	mac_vst_error_callback (buffer);
	va_end (ap);
}

/*default error handler callback*/

static void default_mac_vst_error_callback (const char *desc)
{
	PBD::error << desc << endmsg;
}

void (*mac_vst_error_callback)(const char *desc) = &default_mac_vst_error_callback;

/* --- */

/*Create and return a pointer to a new VSTFX handle*/

static VSTHandle *
mac_vst_handle_new ()
{
	VSTHandle* mac_vst = (VSTHandle *) calloc (1, sizeof (VSTHandle));
	return mac_vst;
}

/*Create and return a pointer to a new mac_vst instance*/

static VSTState *
mac_vst_new ()
{
	VSTState* mac_vst = (VSTState *) calloc (1, sizeof (VSTState));
	vststate_init (mac_vst);
	return mac_vst;
}

/*This loads up a plugin, given the path to its .vst bundle and
 * finds its main entry point etc */

VSTHandle *
mac_vst_load (const char *path)
{
	VSTHandle* fhandle;

	/*Create a new handle we can use to reference the plugin*/

	fhandle = mac_vst_handle_new ();

	fhandle->dll = NULL;

	CFURLRef url;
	if (!(url = CFURLCreateFromFileSystemRepresentation (0, (const UInt8*)path, (CFIndex) strlen (path), true))) {
		return 0;
	}

	CFBundleRef bundleRef = CFBundleCreate (kCFAllocatorDefault, url);
	CFRelease (url);

	if (bundleRef == 0) {
		return 0;
	}

	if (!CFBundleLoadExecutable (bundleRef)) {
		CFRelease (bundleRef);
		return 0;
	}

	fhandle->name = strdup (path);
	fhandle->dll = (void*) &bundleRef;

	fhandle->main_entry = (main_entry_t)
		CFBundleGetFunctionPointerForName (bundleRef, CFSTR("main_macho"));

	if (!fhandle->main_entry) {
		fhandle->main_entry = (main_entry_t)
			CFBundleGetFunctionPointerForName (bundleRef, CFSTR("VSTPluginMain"));
	}

	if (fhandle->main_entry == 0) {
		mac_vst_unload (fhandle);
		return 0;
	}

	fhandle->res_file_id = CFBundleOpenBundleResourceMap (bundleRef);

	/*return the handle of the plugin*/
	return fhandle;
}

/*This unloads a plugin*/

int
mac_vst_unload (VSTHandle* fhandle)
{
	if (fhandle->plugincnt)
	{
		/*Still have plugin instances - can't unload the library
		- actually dlclose keeps an instance count anyway*/

		return -1;
	}

	/*Valid plugin loaded?*/

	if (fhandle->dll)
	{
		CFBundleRef* bundleRefPtr = (CFBundleRef*) fhandle->dll;
		CFBundleCloseBundleResourceMap (*bundleRefPtr, (CFBundleRefNum)fhandle->res_file_id);
		CFRelease (*bundleRefPtr);
		fhandle->dll = 0;
	}

	if (fhandle->name)
	{
		free (fhandle->name);
	}

	/*Don't need the plugin handle any more*/

	free (fhandle);
	return 0;
}

/*This instantiates a plugin*/

VSTState *
mac_vst_instantiate (VSTHandle* fhandle, audioMasterCallback amc, void* userptr)
{
	VSTState* mac_vst = mac_vst_new ();

	if (fhandle == 0)
	{
		mac_vst_error ( "** ERROR ** VSTFX : The handle was 0\n" );
		free (mac_vst);
		return 0;
	}

	if ((mac_vst->plugin = fhandle->main_entry (amc)) == 0)
	{
		mac_vst_error ("** ERROR ** VSTFX : %s could not be instantiated :(\n", fhandle->name);
		free (mac_vst);
		return 0;
	}

	mac_vst->handle = fhandle;
	mac_vst->plugin->user = userptr;

	if (mac_vst->plugin->magic != kEffectMagic)
	{
		mac_vst_error ("** ERROR ** VSTFX : %s is not a VST plugin\n", fhandle->name);
		free (mac_vst);
		return 0;
	}

	mac_vst->plugin->dispatcher (mac_vst->plugin, effOpen, 0, 0, 0, 0);

	/*May or May not need to 'switch the plugin on' here - unlikely
	since FST doesn't and most plugins start up 'On' by default - I think this is the least of our worries*/

	//mac_vst->plugin->dispatcher (mac_vst->plugin, effMainsChanged, 0, 1, 0, 0);

	/* configure plugin to use Cocoa View */
	mac_vst->plugin->dispatcher (mac_vst->plugin, effCanDo, 0, 0, const_cast<char*> ("hasCockosViewAsConfig"), 0.0f);

	mac_vst->vst_version = mac_vst->plugin->dispatcher (mac_vst->plugin, effGetVstVersion, 0, 0, 0, 0);

	mac_vst->handle->plugincnt++;
	mac_vst->wantIdle = 0;

	return mac_vst;
}

/*Close a mac_vst instance*/

void mac_vst_close (VSTState* mac_vst)
{
	// assert that the GUI object is destoyed

	if (mac_vst->plugin)
	{
		mac_vst->plugin->dispatcher (mac_vst->plugin, effMainsChanged, 0, 0, 0, 0);

		/*Calling dispatcher with effClose will cause the plugin's destructor to
		be called, which will also remove the editor if it exists*/

		mac_vst->plugin->dispatcher (mac_vst->plugin, effClose, 0, 0, 0, 0);
	}

	if (mac_vst->handle->plugincnt)
			mac_vst->handle->plugincnt--;

	/*mac_vst_unload will unload the dll if the instance count allows -
	we need to do this because some plugins keep their own instance count
	and (JUCE) manages the plugin UI in its own thread.  When the plugins
	internal instance count reaches zero, JUCE stops the UI thread and won't
	restart it until the next time the library is loaded.  If we don't unload
	the lib JUCE will never restart*/


	if (mac_vst->handle->plugincnt)
	{
		return;
	}

	/*Valid plugin loaded - so we can unload it and 0 the pointer
	to it.  We can't free the handle here because we don't know what else
	might need it.  It should be / is freed when the plugin is deleted*/

	if (mac_vst->handle->dll)
	{
		dlclose (mac_vst->handle->dll); //dlclose keeps its own reference count
		mac_vst->handle->dll = 0;
	}
	free (mac_vst);
}

#if 0 // TODO wrap dispatch
intptr_t
mac_vst_dispatch (VSTState* mac_vst, int op, int idx, intptr_t val, void* ptr, float opt)
{
	const ResFileRefNum old_resources = CurResFile();

	if (mac_vst->handle->res_file_id) {
		UseResFile (mac_vst->handle->res_file_id);
	}

	mac_vst->plugin->dispatcher (mac_vst->plugin, op, idx, val, prt, opt);

	const ResFileRefNum current_res = CurResFile();
	if (current_res != old_resources) {
		mac_vst->handle->res_file_id = current_res;
		UseResFile (old_resources);
	}
}
#endif
