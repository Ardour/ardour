/*
    Copyright (C) 2004 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <fst.h>
#include <gtk/gtk.h>
#include <gtk/gtksocket.h>
#include "ardour/plugin_insert.h"
#include "ardour/windows_vst_plugin.h"

#include "windows_vst_plugin_ui.h"

#include <gdk/gdkx.h>

using namespace Gtk;
using namespace ARDOUR;
using namespace PBD;

WindowsVSTPluginUI::WindowsVSTPluginUI (boost::shared_ptr<PluginInsert> pi, boost::shared_ptr<VSTPlugin> vp)
	: VSTPluginUI (pi, vp)
{
	fst_run_editor (_vst->state());

	pack_start (plugin_analysis_expander, true, true);
}

WindowsVSTPluginUI::~WindowsVSTPluginUI ()
{
	// plugin destructor destroys the custom GUI, via Windows fun-and-games,
	// and then our PluginUIWindow does the rest
}

int
WindowsVSTPluginUI::package (Gtk::Window& win)
{
	VSTPluginUI::package (win);

	/* This assumes that the window's owner understands the XEmbed protocol */

	_socket.add_id (fst_get_XID (_vst->state ()));

	fst_move_window_into_view (_vst->state ());

	return 0;
}

void
WindowsVSTPluginUI::forward_key_event (GdkEventKey* ev)
{
	if (ev->type != GDK_KEY_PRESS) {
		return;
	}

	VSTState* fst = _vst->state ();
	pthread_mutex_lock (&fst->lock);

	if (fst->n_pending_keys == (sizeof (fst->pending_keys) * sizeof (VSTKey))) {
		/* buffer full */
		return;
	}
	
	int special_windows_key = 0;
	int character_windows_key = 0;
	
	switch (ev->keyval) {
	case GDK_Left:
		special_windows_key = 0x25;
		break;
	case GDK_Right:
		special_windows_key = 0x27;
		break;
	case GDK_Up:
		special_windows_key = 0x26;
		break;
	case GDK_Down:
		special_windows_key = 0x28;
		break;
	case GDK_Return:
	case GDK_KP_Enter:
		special_windows_key = 0xd;
		break;
	default:
		character_windows_key = ev->keyval;
		break;
	}
	
	fst->pending_keys[fst->n_pending_keys].special = special_windows_key;
	fst->pending_keys[fst->n_pending_keys].character = character_windows_key;
	fst->n_pending_keys++;
	
	pthread_mutex_unlock (&fst->lock);
}

int
WindowsVSTPluginUI::get_XID ()
{
	return _vst->state()->xid;
}

typedef int (*error_handler_t)( Display *, XErrorEvent *);
static Display *the_gtk_display;
static error_handler_t wine_error_handler;
static error_handler_t gtk_error_handler;

static int
fst_xerror_handler (Display* disp, XErrorEvent* ev)
{
	if (disp == the_gtk_display) {
		printf ("relaying error to gtk\n");
		return gtk_error_handler (disp, ev);
	} else {
		printf( "relaying error to wine\n" );
		return wine_error_handler (disp, ev);
	}
}

void
windows_vst_gui_init (int *argc, char **argv[])
{
	wine_error_handler = XSetErrorHandler (NULL);
	gtk_init (argc, argv);
	the_gtk_display = gdk_x11_display_get_xdisplay (gdk_display_get_default());
	gtk_error_handler = XSetErrorHandler (fst_xerror_handler);
}

