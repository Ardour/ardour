/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_trigger_page_h__
#define __gtk_ardour_trigger_page_h__

#include <gtkmm/box.h>

#include "ardour/session_handle.h"

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/cairo_widget.h"

#include "widgets/pane.h"
#include "widgets/tabbable.h"

#include "fitted_canvas_widget.h"

class TriggerStrip;
class CueMaster;
class SlotPropertiesBox;
class AudioRegionPropertiesBox;
class MidiRegionPropertiesBox;
class AudioRegionOperationsBox;
class MidiRegionOperationsBox;
class AudioRegionTrimmerBox;
class MidiRegionTrimmerBox;

class TriggerPage : public ArdourWidgets::Tabbable, public ARDOUR::SessionHandlePtr, public PBD::ScopedConnectionList
{
public:
	TriggerPage ();
	~TriggerPage ();

	void set_session (ARDOUR::Session*);

	XMLNode& get_state ();
	int      set_state (const XMLNode&, int /* version */);

	Gtk::Window* use_own_window (bool and_fill_it);

private:
	void load_bindings ();
	void register_actions ();
	void update_title ();
	void session_going_away ();
	void parameter_changed (std::string const&);

	void initial_track_display ();
	void add_routes (ARDOUR::RouteList&);
	void remove_route (TriggerStrip*);

	void redisplay_track_list ();
	void pi_property_changed (PBD::PropertyChange const&);
	void stripable_property_changed (PBD::PropertyChange const&, boost::weak_ptr<ARDOUR::Stripable>);

	void selection_changed ();
	PBD::ScopedConnectionList editor_connections;

	gint start_updating ();
	gint stop_updating ();
	void fast_update_strips ();

	Gtkmm2ext::Bindings* bindings;
	Gtk::VBox            _content;

	ArdourWidgets::VPane _pane;
	ArdourWidgets::HPane _pane_upper;
	Gtk::HBox            _strip_group_box;
	Gtk::ScrolledWindow  _strip_scroller;
	Gtk::HBox            _strip_packer;
	Gtk::EventBox        _no_strips;
	Gtk::VBox            _slot_area_box;
	Gtk::VBox            _browser_box;
	Gtk::HBox            _parameter_box;

	FittedCanvasWidget 		_master_widget;
	CueMaster				*_master;

	SlotPropertiesBox* _slot_prop_box;

	AudioRegionPropertiesBox* _audio_prop_box;
	AudioRegionOperationsBox* _audio_ops_box;
	AudioRegionTrimmerBox*    _audio_trim_box;

	MidiRegionPropertiesBox* _midi_prop_box;
	MidiRegionOperationsBox* _midi_ops_box;
	MidiRegionTrimmerBox*    _midi_trim_box;

	std::list<TriggerStrip*> _strips;
	sigc::connection         _fast_screen_update_connection;
};

#endif /* __gtk_ardour_trigger_page_h__ */
