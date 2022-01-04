/*
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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

#include <gtkmm/dialog.h>

#include "pbd/enumwriter.h"

#include "widgets/ardour_icon.h"

#include "audio_clock.h"
#include "editing.h"
#include "enums.h"
#include "editor_items.h"
#include "startup_fsm.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace Editing;
using namespace ArdourWidgets;
using namespace Gtk;

void
setup_gtk_ardour_enums ()
{
	EnumWriter& enum_writer (EnumWriter::instance());
	vector<int> i;
	vector<string> s;

	AudioClock::Mode clock_mode;
	Width width;
	ImportMode import_mode;
	EditPoint edit_point;
	LayerDisplay layer_display;
	RegionListSortType region_list_sort_type;
	GridType grid_type;
	SnapMode snap_mode;
	ZoomFocus zoom_focus;
	ItemType item_type;
	MouseMode mouse_mode;
	StartupFSM::MainState startup_state;
	StartupFSM::DialogID startup_dialog;
	Gtk::ResponseType dialog_response;

#define REGISTER(e) enum_writer.register_distinct (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_BITS(e) enum_writer.register_bits (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_ENUM(e) i.push_back (e); s.push_back (#e)
#define REGISTER_CLASS_ENUM(t,e) i.push_back (t::e); s.push_back (#e)

	REGISTER_CLASS_ENUM (AudioClock, Timecode);
	REGISTER_CLASS_ENUM (AudioClock, BBT);
	REGISTER_CLASS_ENUM (AudioClock, MinSec);
	REGISTER_CLASS_ENUM (AudioClock, Seconds);
	REGISTER_CLASS_ENUM (AudioClock, Samples);
	REGISTER (clock_mode);

	REGISTER_ENUM (Wide);
	REGISTER_ENUM (Narrow);
	REGISTER (width);

	REGISTER_ENUM (ImportAsTrack);
	REGISTER_ENUM (ImportToTrack);
	REGISTER_ENUM (ImportAsRegion);
	REGISTER_ENUM (ImportAsTrigger);
	REGISTER (import_mode);

	REGISTER_ENUM (EditAtPlayhead);
	REGISTER_ENUM (EditAtMouse);
	REGISTER_ENUM (EditAtSelectedMarker);
	REGISTER (edit_point);

	REGISTER_ENUM (Overlaid);
	REGISTER_ENUM (Stacked);
	REGISTER (layer_display);

	REGISTER_ENUM (ByEndInFile);
	REGISTER_ENUM (ByLength);
	REGISTER_ENUM (ByName);
	REGISTER_ENUM (ByPosition);
	REGISTER_ENUM (BySourceFileCreationDate);
	REGISTER_ENUM (BySourceFileFS);
	REGISTER_ENUM (BySourceFileLength);
	REGISTER_ENUM (BySourceFileName);
	REGISTER_ENUM (ByStartInFile);
	REGISTER_ENUM (ByTimestamp);
	REGISTER (region_list_sort_type);

	REGISTER_ENUM (GridTypeNone);
	REGISTER_ENUM (GridTypeBar);
	REGISTER_ENUM (GridTypeBeat);
	REGISTER_ENUM (GridTypeBeatDiv2);
	REGISTER_ENUM (GridTypeBeatDiv4);
	REGISTER_ENUM (GridTypeBeatDiv8);
	REGISTER_ENUM (GridTypeBeatDiv16);
	REGISTER_ENUM (GridTypeBeatDiv32);
	REGISTER_ENUM (GridTypeBeatDiv3);
	REGISTER_ENUM (GridTypeBeatDiv6);
	REGISTER_ENUM (GridTypeBeatDiv12);
	REGISTER_ENUM (GridTypeBeatDiv24);
	REGISTER_ENUM (GridTypeBeatDiv5);
	REGISTER_ENUM (GridTypeBeatDiv10);
	REGISTER_ENUM (GridTypeBeatDiv20);
	REGISTER_ENUM (GridTypeBeatDiv7);
	REGISTER_ENUM (GridTypeBeatDiv14);
	REGISTER_ENUM (GridTypeBeatDiv28);
	REGISTER_ENUM (GridTypeTimecode);
	REGISTER_ENUM (GridTypeMinSec);
	REGISTER_ENUM (GridTypeCDFrame);
	REGISTER (grid_type);

	REGISTER_ENUM (SnapOff);
	REGISTER_ENUM (SnapNormal);
	REGISTER_ENUM (SnapMagnetic);
	REGISTER (snap_mode);

	REGISTER_ENUM (ZoomFocusLeft);
	REGISTER_ENUM (ZoomFocusRight);
	REGISTER_ENUM (ZoomFocusCenter);
	REGISTER_ENUM (ZoomFocusPlayhead);
	REGISTER_ENUM (ZoomFocusMouse);
	REGISTER_ENUM (ZoomFocusEdit);
	REGISTER (zoom_focus);

	REGISTER_ENUM (RegionItem);
	REGISTER_ENUM (WaveItem);
	REGISTER_ENUM (StreamItem);
	REGISTER_ENUM (PlayheadCursorItem);
	REGISTER_ENUM (MarkerItem);
	REGISTER_ENUM (MarkerBarItem);
	REGISTER_ENUM (RangeMarkerBarItem);
	REGISTER_ENUM (CdMarkerBarItem);
	REGISTER_ENUM (CueMarkerBarItem);
	REGISTER_ENUM (VideoBarItem);
	REGISTER_ENUM (TransportMarkerBarItem);
	REGISTER_ENUM (SelectionItem);
	REGISTER_ENUM (ControlPointItem);
	REGISTER_ENUM (GainLineItem);
	REGISTER_ENUM (AutomationLineItem);
	REGISTER_ENUM (MeterMarkerItem);
	REGISTER_ENUM (TempoCurveItem);
	REGISTER_ENUM (TempoMarkerItem);
	REGISTER_ENUM (MeterBarItem);
	REGISTER_ENUM (TempoBarItem);
	REGISTER_ENUM (RegionViewNameHighlight);
	REGISTER_ENUM (RegionViewName);
	REGISTER_ENUM (StartSelectionTrimItem);
	REGISTER_ENUM (EndSelectionTrimItem);
	REGISTER_ENUM (AutomationTrackItem);
	REGISTER_ENUM (FadeInItem);
	REGISTER_ENUM (FadeInHandleItem);
	REGISTER_ENUM (FadeOutItem);
	REGISTER_ENUM (FadeOutHandleItem);
	REGISTER_ENUM (NoteItem);
	REGISTER_ENUM (FeatureLineItem);
	REGISTER_ENUM (LeftFrameHandle);
	REGISTER_ENUM (RightFrameHandle);
	REGISTER_ENUM (StartCrossFadeItem);
	REGISTER_ENUM (EndCrossFadeItem);
	REGISTER_ENUM (CrossfadeViewItem);
	REGISTER_ENUM (TimecodeRulerItem);
	REGISTER_ENUM (MinsecRulerItem);
	REGISTER_ENUM (BBTRulerItem);
	REGISTER_ENUM (SamplesRulerItem);
	REGISTER (item_type);

	REGISTER_ENUM(MouseObject);
	REGISTER_ENUM(MouseRange);
	REGISTER_ENUM(MouseDraw);
	REGISTER_ENUM(MouseTimeFX);
	REGISTER_ENUM(MouseAudition);
	REGISTER_ENUM(MouseCut);
	REGISTER_ENUM(MouseContent);
	REGISTER (mouse_mode);

	REGISTER_CLASS_ENUM (StartupFSM, WaitingForPreRelease);
	REGISTER_CLASS_ENUM (StartupFSM, WaitingForNewUser);
	REGISTER_CLASS_ENUM (StartupFSM, WaitingForSessionPath);
	REGISTER_CLASS_ENUM (StartupFSM, WaitingForEngineParams);
	REGISTER_CLASS_ENUM (StartupFSM, WaitingForPlugins);
	REGISTER (startup_state);

	REGISTER_CLASS_ENUM (StartupFSM, PreReleaseDialog);
	REGISTER_CLASS_ENUM (StartupFSM, NewUserDialog);
	REGISTER_CLASS_ENUM (StartupFSM, NewSessionDialog);
	REGISTER_CLASS_ENUM (StartupFSM, AudioMIDISetup);
	REGISTER_CLASS_ENUM (StartupFSM, PluginDialog);
	REGISTER (startup_dialog);

	REGISTER_ENUM (RESPONSE_NONE);
	REGISTER_ENUM (RESPONSE_REJECT);
	REGISTER_ENUM (RESPONSE_ACCEPT);
	REGISTER_ENUM (RESPONSE_DELETE_EVENT);
	REGISTER_ENUM (RESPONSE_OK);
	REGISTER_ENUM (RESPONSE_CANCEL);
	REGISTER_ENUM (RESPONSE_CLOSE);
	REGISTER_ENUM (RESPONSE_YES);
	REGISTER_ENUM (RESPONSE_NO);
	REGISTER_ENUM (RESPONSE_APPLY);
	REGISTER_ENUM (RESPONSE_HELP);
	REGISTER (dialog_response);
}
