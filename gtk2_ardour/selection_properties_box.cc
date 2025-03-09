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
#include "editor.h"
#include "region_fx_properties_box.h"
#include "region_view.h"
#include "route_properties_box.h"
#include "selection_properties_box.h"
#include "time_info_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using std::min;
using std::max;

SelectionPropertiesBox::SelectionPropertiesBox ()
	: _region_editor (nullptr)
	, _region_fx_box (nullptr)
{
	init ();

	_time_info_box  = new TimeInfoBox ("EditorTimeInfo", true);
	_route_prop_box = new RoutePropertiesBox ();

	pack_start(*_time_info_box, false, false, 0);
	pack_start(*_route_prop_box, true, true, 0);
	pack_start(_region_editor_box, true, true, 0);

	_time_info_box->set_no_show_all ();
	_route_prop_box->set_no_show_all ();
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

	selection_changed();
}

void
SelectionPropertiesBox::track_mouse_mode ()
{
	/* maybe do something here? */
}

void
SelectionPropertiesBox::delete_region_editor ()
{
	if (!_region_editor) {
		return;
	}
	assert (_region_fx_box);
	_region_editor_box.remove (*_region_editor);
	_region_editor_box.remove (*_region_fx_box);
	delete _region_editor;
	delete _region_fx_box;
	_region_editor = nullptr;
	_region_fx_box = nullptr;
	_region_editor_box.hide ();
}

void
SelectionPropertiesBox::selection_changed ()
{
	if (!_session || _session->inital_connect_or_deletion_in_progress ()) {
		_time_info_box->hide ();
		_route_prop_box->hide ();
		delete_region_editor ();
		return;
	}

	Selection& selection (Editor::instance().get_selection());

	if (!selection.time.empty ()) {
		_time_info_box->show ();
	} else {
		_time_info_box->hide ();
	}

	bool show_route_properties = false;
	if (!selection.tracks.empty ()) {
		TimeAxisView *tav = selection.tracks.front ();
		RouteTimeAxisView *rtav = dynamic_cast<RouteTimeAxisView *>(tav);
		if (rtav) {
			_route_prop_box->set_route (rtav->route());
			show_route_properties = true;
		}
	}
	if (show_route_properties) {
		_route_prop_box->show();
	} else {
		_route_prop_box->hide();
	}

	if (selection.regions.size () == 1)  {
		RegionView* rv = (selection.regions.front ());
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
			_region_editor_box.pack_start (*_region_fx_box);
			rv->RegionViewGoingAway.connect_same_thread (_region_connection, std::bind (&SelectionPropertiesBox::delete_region_editor, this));
		}
		_region_editor_box.show ();
	} else {
		/* only hide region props when selecting a track or trigger ..*/
		if (_route_prop_box->get_visible () || !selection.markers.empty () || !selection.playlists.empty () || !selection.triggers.empty ()) {
			delete_region_editor ();
		}
	}
}
