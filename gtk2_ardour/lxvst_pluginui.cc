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

/******************************************************************/
/*linuxDSP - pluginui variant for LXVST (native Linux VST) Plugins*/
/******************************************************************/

#include <ardour/vstfx.h>
#include <gtk/gtk.h>
#include <gtk/gtksocket.h>
#include <ardour/processor.h>
#include <ardour/lxvst_plugin.h>

#include "ardour_ui.h"
#include "plugin_ui.h"
#include "lxvst_plugin_ui.h"

#include <gdk/gdkx.h>

#define LXVST_H_FIDDLE 40

using namespace Gtk;
using namespace ARDOUR;
using namespace PBD;

LXVSTPluginUI::LXVSTPluginUI (boost::shared_ptr<PluginInsert> pi, boost::shared_ptr<LXVSTPlugin> lxvp)
	: PlugUIBase (pi),
	  lxvst (lxvp)
{
	create_preset_store ();

	vstfx_run_editor (lxvst->vstfx());

	if (lxvst->vstfx()->current_program != -1) {
		lxvst_preset_combo.set_active (lxvst->vstfx()->current_program);
	} else {
		lxvst_preset_combo.set_active (0);
	}
	
	preset_box.set_spacing (6);
	preset_box.set_border_width (6);
	preset_box.pack_end (bypass_button, false, false, 10);
	preset_box.pack_end (save_button, false, false);
	preset_box.pack_end (lxvst_preset_combo, false, false);

	lxvst_preset_combo.signal_changed().connect (mem_fun (*this, &LXVSTPluginUI::preset_chosen));

	if (!insert->active()) {
		bypass_button.set_active_state (Gtkmm2ext::Active);
	} else {
		bypass_button.unset_active_state ();
	}
	
	pack_start (preset_box, false, false);
	pack_start (socket, true, true);
}

LXVSTPluginUI::~LXVSTPluginUI ()
{

	_screen_update_connection.disconnect();	
	
	// plugin destructor destroys the custom GUI, via the vstfx engine,
	// and then our PluginUIWindow does the rest
}


bool
LXVSTPluginUI::start_updating (GdkEventAny* ignored)
{
	_screen_update_connection.disconnect();
	_screen_update_connection = ARDOUR_UI::instance()->RapidScreenUpdate.connect 
			(mem_fun(*this, &LXVSTPluginUI::resize_callback));
	return false;
}

bool
LXVSTPluginUI::stop_updating (GdkEventAny* ignored)
{
	_screen_update_connection.disconnect();
	return false;
}


void
LXVSTPluginUI::resize_callback()
{
	/*We could maybe use this to resize the plugin GTK parent window
	if required*/
	
	if(lxvst->vstfx()->want_resize)
	{
		int new_height = lxvst->vstfx()->height;
		int new_width = lxvst->vstfx()->width;
		
		void* gtk_parent_window = lxvst->vstfx()->extra_data;
		
		if(gtk_parent_window)
			((Gtk::Window*)gtk_parent_window)->resize(new_width, new_height + LXVST_H_FIDDLE);
		
		lxvst->vstfx()->want_resize = 0;
	}
}

void
LXVSTPluginUI::preset_selected ()
{
	socket.grab_focus ();
	PlugUIBase::preset_selected ();
}

void
LXVSTPluginUI::preset_chosen ()
{
	// we can't dispatch directly here, too many plugins only expects one GUI thread.
	
	lxvst->vstfx()->want_program = lxvst_preset_combo.get_active_row_number ();
	socket.grab_focus ();
}

int
LXVSTPluginUI::get_preferred_height ()
{	
	/*FIXME*/
	
	/*We have to return the required height of the plugin UI window +  a fiddle factor
	because we can't know how big the preset menu bar is until the window is realised
	and we can't realise it until we have told it how big we would like it to be
	which we can't do until it is realised etc*/

	return (lxvst->vstfx()->height) + LXVST_H_FIDDLE; //May not be 40 for all screen res etc
}

int
LXVSTPluginUI::get_preferred_width ()
{
	return lxvst->vstfx()->width;
}

int
LXVSTPluginUI::package (Gtk::Window& win)
{
	/* forward configure events to plugin window */

	win.signal_configure_event().connect (bind (mem_fun (*this, &LXVSTPluginUI::configure_handler), &socket), false);
	
	/*Map the UI start and stop updating events to 'Map' events on the Window*/
	
	win.signal_map_event().connect (mem_fun (*this, &LXVSTPluginUI::start_updating));
	win.signal_unmap_event().connect (mem_fun (*this, &LXVSTPluginUI::stop_updating));

	/* this assumes that the window's owner understands the XEmbed protocol. */
	
	socket.add_id (vstfx_get_XID (lxvst->vstfx()));

	vstfx_move_window_into_view (lxvst->vstfx());
	
	lxvst->vstfx()->extra_data = (void*)(&win);
	lxvst->vstfx()->want_resize = 0;

	return 0;
}

bool
LXVSTPluginUI::configure_handler (GdkEventConfigure* ev, Gtk::Socket *socket)
{
	XEvent event;
	gint x, y;
	GdkWindow* w;

	if (socket == 0 || ((w = socket->gobj()->plug_window) == 0)) {
		return false;
	}

	event.xconfigure.type = ConfigureNotify;
	event.xconfigure.event = GDK_WINDOW_XWINDOW (w);
	event.xconfigure.window = GDK_WINDOW_XWINDOW (w);

	/* The ICCCM says that synthetic events should have root relative
	 * coordinates. We still aren't really ICCCM compliant, since
	 * we don't send events when the real toplevel is moved.
	 */
	gdk_error_trap_push ();
	gdk_window_get_origin (w, &x, &y);
	gdk_error_trap_pop ();

	event.xconfigure.x = x;
	event.xconfigure.y = y;
	event.xconfigure.width = GTK_WIDGET(socket->gobj())->allocation.width;
	event.xconfigure.height = GTK_WIDGET(socket->gobj())->allocation.height;

	event.xconfigure.border_width = 0;
	event.xconfigure.above = None;
	event.xconfigure.override_redirect = False;

	gdk_error_trap_push ();
	XSendEvent (GDK_WINDOW_XDISPLAY (w), GDK_WINDOW_XWINDOW (w), False, StructureNotifyMask, &event);
	gdk_error_trap_pop ();

	return false;
}

void
LXVSTPluginUI::forward_key_event (GdkEventKey* ev)
{
	std::cerr << "LXVSTPluginUI : keypress forwarding to linuxVSTs unsupported" << std::endl;
}


void
LXVSTPluginUI::create_preset_store ()
{
	VSTFX* vstfx = lxvst->vstfx();
	
	int vst_version = vstfx->plugin->dispatcher (vstfx->plugin, effGetVstVersion, 0, 0, NULL, 0.0f);

	preset_model = ListStore::create (preset_columns);

	for (int i = 0; i < vstfx->plugin->numPrograms; ++i) {
		char buf[100];
		TreeModel::Row row = *(preset_model->append());
	
		snprintf (buf, 90, "preset %d", i);
	
		if (vst_version >= 2) {
			vstfx->plugin->dispatcher (vstfx->plugin, 29, i, 0, buf, 0.0);
		}
		
		row[preset_columns.name] = buf;
		row[preset_columns.number] = i;
	}
	
	if (vstfx->plugin->numPrograms > 0) {
		vstfx->plugin->dispatcher( vstfx->plugin, effSetProgram, 0, 0, NULL, 0.0 );
	}
	
	lxvst_preset_combo.set_model (preset_model);

	CellRenderer* renderer = manage (new CellRendererText());
	lxvst_preset_combo.pack_start (*renderer, true);
	lxvst_preset_combo.add_attribute (*renderer, "text", 0);
}

typedef int (*error_handler_t)( Display *, XErrorEvent *);
static Display *the_gtk_display;
static error_handler_t vstfx_error_handler;
static error_handler_t gtk_error_handler;

static int 
gtk_xerror_handler( Display *disp, XErrorEvent *ev )
{
	std::cerr << "** ERROR ** LXVSTPluginUI : Trapped an X Window System Error" << std::endl;
	
	return 0;
}

void
gui_init (int *argc, char **argv[])
{
	vstfx_error_handler = XSetErrorHandler (NULL);
	gtk_init (argc, argv);
	the_gtk_display = gdk_x11_display_get_xdisplay (gdk_display_get_default());
	gtk_error_handler = XSetErrorHandler( gtk_xerror_handler );
}

