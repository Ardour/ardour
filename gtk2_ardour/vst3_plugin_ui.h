/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_vst3_plugin_ui_h__
#define __ardour_vst3_plugin_ui_h__

#ifdef VST3_SUPPORT

#include "plugin_ui.h"

namespace ARDOUR {
	class PluginInsert;
	class VST3Plugin;
}

class VST3PluginUI : public PlugUIBase, public Gtk::VBox
{
public:
	VST3PluginUI (boost::shared_ptr<ARDOUR::PluginInsert>, boost::shared_ptr<ARDOUR::VST3Plugin>);
	virtual ~VST3PluginUI ();

	gint get_preferred_height ();
	gint get_preferred_width ();
	bool resizable ();
	void forward_key_event (GdkEventKey*);
	bool non_gtk_gui() const;

	int  package (Gtk::Window&);
	bool start_updating (GdkEventAny*);
	bool stop_updating (GdkEventAny*);

protected:
	virtual void resize_callback (int, int) = 0;

  bool forward_scroll_event (GdkEventScroll*);

	boost::shared_ptr<ARDOUR::PluginInsert> _pi;
	boost::shared_ptr<ARDOUR::VST3Plugin>   _vst3;

	Gtk::HBox _ardour_buttons_box;

	int _req_width;
	int _req_height;

	bool _resize_in_progress;
	bool _view_realized;

private:
	void parameter_update ();

	PBD::ScopedConnection _resize_connection;
	sigc::connection      _update_connection;
};

#endif // VST3_SUPPORT
#endif
