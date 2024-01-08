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

#include "audio_region_operations_box.h"
#include "audio_region_properties_box.h"
#include "audio_trigger_properties_box.h"
#include "cuebox_ui.h"
#include "fitted_canvas_widget.h"
#include "midi_region_operations_box.h"
#include "midi_region_properties_box.h"
#include "midi_trigger_properties_box.h"
#include "route_processor_selection.h"
#include "slot_properties_box.h"
#include "trigger_clip_picker.h"
#include "trigger_region_list.h"
#include "trigger_route_list.h"
#include "trigger_source_list.h"
#include "trigger_master.h"

class TriggerStrip;
class MidiCueEditor;

class TriggerPage : public ArdourWidgets::Tabbable, public ARDOUR::SessionHandlePtr, public PBD::ScopedConnectionList, public AxisViewProvider
{
public:
	TriggerPage ();
	~TriggerPage ();

	void set_session (ARDOUR::Session*);

	XMLNode& get_state () const;
	int      set_state (const XMLNode&, int /* version */);

	Gtk::Window* use_own_window (bool and_fill_it);

	RouteProcessorSelection& selection() { return _selection; }

private:
	void load_bindings ();
	void register_actions ();
	void update_title ();
	void session_going_away ();
	void parameter_changed (std::string const&);

	void initial_track_display ();
	void add_routes (ARDOUR::RouteList&);
	void remove_route (TriggerStrip*);

	void clear_selected_slot ();

	void redisplay_track_list ();
	void pi_property_changed (PBD::PropertyChange const&);
	void stripable_property_changed (PBD::PropertyChange const&, std::weak_ptr<ARDOUR::Stripable>);

	void rec_state_changed ();
	void rec_state_clicked ();

	void add_sidebar_page (std::string const&, Gtk::Widget&);

	bool strip_button_release_event (GdkEventButton*, TriggerStrip*);
	bool no_strip_button_event (GdkEventButton*);
	bool no_strip_drag_motion (Glib::RefPtr<Gdk::DragContext> const&, int, int, guint);
	void no_strip_drag_data_received (Glib::RefPtr<Gdk::DragContext> const&, int, int, Gtk::SelectionData const&, guint, guint);

	bool idle_drop_paths (std::vector<std::string>);
	void drop_paths_part_two (std::vector<std::string>);

	AxisView* axis_view_by_stripable (std::shared_ptr<ARDOUR::Stripable>) const;
	AxisView* axis_view_by_control (std::shared_ptr<ARDOUR::AutomationControl>) const;

	void                      selection_changed ();
	PBD::ScopedConnectionList editor_connections;

	gint start_updating ();
	gint stop_updating ();
	void fast_update_strips ();

	Gtkmm2ext::Bindings* bindings;
	Gtk::VBox            _content;

	ArdourWidgets::HPane _pane_upper;
	Gtk::HBox            _strip_group_box;
	Gtk::ScrolledWindow  _strip_scroller;
	Gtk::HBox            _strip_packer;
	Gtk::EventBox        _no_strips;
	Gtk::Alignment       _cue_area_frame;
	Gtk::VBox            _cue_area_box;
	Gtk::HBox            _parameter_box;
	Gtk::VBox            _sidebar_vbox;
	Gtk::Notebook        _sidebar_notebook;
	TriggerClipPicker    _trigger_clip_picker;
	TriggerSourceList    _trigger_source_list;
	TriggerRegionList    _trigger_region_list;
	TriggerRouteList     _trigger_route_list;

	CueBoxWidget       _cue_box;
	FittedCanvasWidget _master_widget;
	CueMaster          _master;

	SlotPropertiesBox _slot_prop_box;

	AudioTriggerPropertiesBox _audio_trig_box;
	MidiTriggerPropertiesBox _midi_trig_box;

#if REGION_PROPERTIES_BOX_TODO
	AudioRegionOperationsBox  _audio_ops_box;
	AudioClipEditorBox        _audio_trim_box;
#endif

	MidiCueEditor*           _midi_editor;

	RouteProcessorSelection  _selection;
	std::list<TriggerStrip*> _strips;
	sigc::connection         _fast_screen_update_connection;
};

#endif /* __gtk_ardour_trigger_page_h__ */
