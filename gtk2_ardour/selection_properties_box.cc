/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2024 Robin Gareus <robin@gareus.org>
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

#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour/session.h"

#include "audio_region_editor.h"
#include "audio_region_view.h"
#include "control_point.h"
#include "editor.h"
#include "region_fx_line.h"
#include "region_fx_properties_box.h"
#include "region_view.h"
#include "route_properties_box.h"
#include "selection_properties_box.h"
#include "slot_properties_box.h"
#include "time_info_box.h"
#include "trigger_strip.h"
#include "triggerbox_ui.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using std::min;
using std::max;

SelectionPropertiesBox::SelectionPropertiesBox (DispositionMask mask)
	: _region_editor_box_rhs (nullptr)
	, _region_editor (nullptr)
	, _region_fx_box (nullptr)
	, _disposition (mask)
{
	init ();

	_time_info_box  = new TimeInfoBox ("EditorTimeInfo", true);
	_route_prop_box = new RoutePropertiesBox ();
	_slot_prop_box = new SlotPropertiesBox ();

	pack_start(*_time_info_box, false, false, 0);
	pack_start(*_route_prop_box, true, true, 0);
	pack_start(*_slot_prop_box, true, true, 0);
	pack_start(_region_editor_box, true, true, 0);

	_time_info_box->set_no_show_all ();
	_route_prop_box->set_no_show_all ();
	_slot_prop_box->set_no_show_all ();
	_region_editor_box.set_no_show_all ();
	_region_editor_box.set_spacing (4);

	_time_info_box->hide ();
	_route_prop_box->hide ();
}

SelectionPropertiesBox::~SelectionPropertiesBox ()
{
	delete _time_info_box;
	delete _route_prop_box;
	delete _region_editor;
	delete _region_fx_box;
	delete _slot_prop_box;
}

void
SelectionPropertiesBox::init ()
{
	Selection& selection (Editor::instance().get_selection());

	/* watch for any change in our selection, so we can show an appropriate property editor */
	selection.TracksChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.RegionsChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.TimeChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.LinesChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.PlaylistsChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.PointsChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.MarkersChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.MidiNotesChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));
	selection.TriggersChanged.connect (sigc::mem_fun (*this, &SelectionPropertiesBox::selection_changed));

	/* maybe we care about mouse mode?? */
	Editor::instance().MouseModeChanged.connect (_editor_connection, invalidator(*this), std::bind (&SelectionPropertiesBox::track_mouse_mode, this), gui_context());
}

void
SelectionPropertiesBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!s) {
		return;
	}

	_time_info_box->set_session(s);
	_route_prop_box->set_session(s);
	_slot_prop_box->set_session(s);

	selection_changed();
}

void
SelectionPropertiesBox::add_region_rhs (Gtk::Widget& w)
{
	/* it will be packed on an as-needed basis */
	_region_editor_box_rhs = &w;
}

void
SelectionPropertiesBox::remove_region_rhs ()
{
	if (_region_editor_box_rhs) {
		if (_region_editor_box_rhs->get_parent()) {
			_region_editor_box.remove (*_region_editor_box_rhs);
		}
		_region_editor_box_rhs = nullptr;
	}
}

void
SelectionPropertiesBox::track_mouse_mode ()
{
	/* maybe do something here? */
}

void
SelectionPropertiesBox::on_map ()
{
	HBox::on_map ();
	SelectionPropertiesBox::selection_changed ();
}

void
SelectionPropertiesBox::on_unmap ()
{
	/* This also triggers when switching pages, or hiding the GUI
	 * perhaps consider show/hide get_visible() instead.
	 */
	HBox::on_unmap ();
	SelectionPropertiesBox::selection_changed ();
	_route_prop_box->set_route (std::shared_ptr<Route>());
}

void
SelectionPropertiesBox::delete_region_editor ()
{
	if (!_region_editor) {
		return;
	}
	assert (_region_fx_box);
	_region_editor_box.remove (*_region_editor);
	if (_region_fx_box && _region_fx_box->get_parent()) {
		_region_editor_box.remove (*_region_fx_box);
	}
	if (_region_editor_box_rhs && _region_editor_box_rhs->get_parent()) {
		_region_editor_box.remove (*_region_editor_box_rhs);
	}
	delete _region_editor;
	_region_editor = nullptr;
	delete _region_fx_box;
	_region_fx_box = nullptr;
	_region_editor = nullptr;
	_region_fx_box = nullptr;
	_region_editor_box.hide ();
}

void
SelectionPropertiesBox::selection_changed ()
{
	if (!_session || _session->inital_connect_or_deletion_in_progress () || !get_mapped ()) {
		_time_info_box->hide ();
		_route_prop_box->hide ();
		_slot_prop_box->hide ();
		delete_region_editor ();
		return;
	}

	Selection& selection (Editor::instance().get_selection());

	bool show_slot_properties = false;
	if (!selection.triggers.empty () && 0 != (_disposition & ShowTriggers)) {
		TriggerSelection ts      = selection.triggers;
		TriggerEntry*    entry   = *ts.begin ();
		TriggerReference ref     = entry->trigger_reference ();

		_slot_prop_box->set_slot(ref);
		show_slot_properties = true;
	}

	bool show_route_properties = false;
	if (!selection.tracks.empty () && 0 != (_disposition & ShowRoutes)) {
		TimeAxisView *tav = selection.tracks.back (); //the LAST selected stripable is the clicked one. see selection.cc line ~92
		RouteTimeAxisView *rtav = dynamic_cast<RouteTimeAxisView *>(tav);

		/* If the selected time axis isn't a route, check the parent */
		if (!rtav) {
			tav = tav->get_parent ();
			rtav = dynamic_cast<RouteTimeAxisView *>(tav);
		}

		if (rtav) {
			_route_prop_box->set_route (rtav->route());
			show_route_properties = true;
		}
	}

	if (!selection.points.empty()) {
		AutomationTimeAxisView* atv = selection.points.back()->line().automation_time_axis_view();
		if (atv) {
			/* Points are selected in an automation time axis, show route properties for corresponding route */
			TimeAxisView* tav = atv->get_parent ();
			RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView *>(tav);

			if (rtav) {
				_route_prop_box->set_route (rtav->route());
				show_route_properties = true;
			}
		}
	}

	RegionView* rv = nullptr;
	if (selection.regions.size () == 1) {
		rv = selection.regions.front ();
	} else if (!selection.points.empty ()) {
		RegionFxLine *rfx = dynamic_cast<RegionFxLine*>(&selection.points.back()->line ());
		if (rfx) {
			rv = &rfx->region_view();
		}
	}

	if (rv && 0 != (_disposition & ShowRegions)) {
		if (!_region_editor || _region_editor->region () != rv->region ()) {
			delete_region_editor ();
			AudioRegionView* arv = dynamic_cast<AudioRegionView*> (rv);
			if (arv) {
				_region_editor = new AudioRegionEditor (_session, arv);
			} else {
				_region_editor = new RegionEditor (_session, rv->region());
			}
			// TODO subscribe to region name changes
			_region_editor->set_label (string_compose (_("Region '%1'"), rv->region()->name ()));
			_region_editor->set_padding (4);
			_region_editor->set_edge_color (0x000000ff); // black
			_region_editor->show_all ();
			_region_editor_box.pack_start (*_region_editor, false, false);

			_region_fx_box = new RegionFxPropertiesBox (rv->region ());
			/* If there's a RHS element and the RegionFX box is
			   empty, don't show the region fx box
			*/
			if (!_region_editor_box_rhs || !_region_fx_box->empty()) {
				_region_editor_box.pack_start (*_region_fx_box);
			}
			if (_region_editor_box_rhs) {
				_region_editor_box.pack_start (*_region_editor_box_rhs, true, true);
			}

			rv->RegionViewGoingAway.connect_same_thread (_region_connection, std::bind (&SelectionPropertiesBox::delete_region_editor, this));

#ifndef MIXBUS
			float min_h = _region_editor->size_request().height;
			float ui_scale = std::max<float> (1.f, UIConfiguration::instance().get_ui_scale());
			_region_editor_box.set_size_request (-1, std::max (365 * ui_scale, min_h));
#endif
		}
	} else {
		/* only hide region props when selecting a track or trigger,
		 * retain existing RegionEditor, when selecting another additional region, or
		 * when switching tools (grab -> draw) to edit region-gain, or note entry.
		 */
		if (!selection.tracks.empty () || !selection.points.empty() || !selection.markers.empty () || !selection.playlists.empty () || !selection.triggers.empty ()) {
			delete_region_editor ();
		}
	}

	if (show_slot_properties) {
		_slot_prop_box->show ();
		_route_prop_box->hide ();
		delete_region_editor ();
	} else if (_region_editor) {
		_slot_prop_box->hide ();
		_route_prop_box->hide ();
		_region_editor_box.show ();
	} else if (show_route_properties) {
		_slot_prop_box->hide ();
		_route_prop_box->show ();
		delete_region_editor ();
	} else {
		_slot_prop_box->hide ();
		_route_prop_box->hide ();
		delete_region_editor ();
	}

	if (!selection.time.empty () && 0 != (_disposition & ShowTimeInfo)) {
		_time_info_box->show ();
	} else {
		_time_info_box->hide ();
	}
}
