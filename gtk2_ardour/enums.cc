/*
    Copyright (C) 2000-2007 Paul Davis

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

#include "pbd/enumwriter.h"

#include "audio_clock.h"
#include "editing.h"
#include "enums.h"
#include "editor_items.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;
using namespace Editing;

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
	SnapType snap_type;
	SnapMode snap_mode;
	ZoomFocus zoom_focus;
	ItemType item_type;

#define REGISTER(e) enum_writer.register_distinct (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_BITS(e) enum_writer.register_bits (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_ENUM(e) i.push_back (e); s.push_back (#e)
#define REGISTER_CLASS_ENUM(t,e) i.push_back (t::e); s.push_back (#e)

	REGISTER_CLASS_ENUM (AudioClock, Timecode);
	REGISTER_CLASS_ENUM (AudioClock, BBT);
	REGISTER_CLASS_ENUM (AudioClock, MinSec);
	REGISTER_CLASS_ENUM (AudioClock, Frames);
	REGISTER (clock_mode);

	REGISTER_ENUM (Wide);
	REGISTER_ENUM (Narrow);
	REGISTER (width);

	REGISTER_ENUM (ImportAsTrack);
	REGISTER_ENUM (ImportToTrack);
	REGISTER_ENUM (ImportAsRegion);
	REGISTER_ENUM (ImportAsTapeTrack);
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

	REGISTER_ENUM (SnapToCDFrame);
	REGISTER_ENUM (SnapToTimecodeFrame);
	REGISTER_ENUM (SnapToTimecodeSeconds);
	REGISTER_ENUM (SnapToTimecodeMinutes);
	REGISTER_ENUM (SnapToSeconds);
	REGISTER_ENUM (SnapToMinutes);
	REGISTER_ENUM (SnapToBeatDiv128);
	REGISTER_ENUM (SnapToBeatDiv64);
	REGISTER_ENUM (SnapToBeatDiv32);
	REGISTER_ENUM (SnapToBeatDiv28);
	REGISTER_ENUM (SnapToBeatDiv24);
	REGISTER_ENUM (SnapToBeatDiv20);
	REGISTER_ENUM (SnapToBeatDiv16);
	REGISTER_ENUM (SnapToBeatDiv14);
	REGISTER_ENUM (SnapToBeatDiv12);
	REGISTER_ENUM (SnapToBeatDiv10);
	REGISTER_ENUM (SnapToBeatDiv8);
	REGISTER_ENUM (SnapToBeatDiv7);
	REGISTER_ENUM (SnapToBeatDiv6);
	REGISTER_ENUM (SnapToBeatDiv5);
	REGISTER_ENUM (SnapToBeatDiv4);
	REGISTER_ENUM (SnapToBeatDiv3);
	REGISTER_ENUM (SnapToBeatDiv2);
	REGISTER_ENUM (SnapToBeat);
	REGISTER_ENUM (SnapToBar);
	REGISTER_ENUM (SnapToMark);
	REGISTER_ENUM (SnapToRegionStart);
	REGISTER_ENUM (SnapToRegionEnd);
	REGISTER_ENUM (SnapToRegionSync);
	REGISTER_ENUM (SnapToRegionBoundary);
	REGISTER (snap_type);

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
	REGISTER_ENUM (StreamItem);
	REGISTER_ENUM (PlayheadCursorItem);
	REGISTER_ENUM (MarkerItem);
	REGISTER_ENUM (MarkerBarItem);
	REGISTER_ENUM (RangeMarkerBarItem);
	REGISTER_ENUM (CdMarkerBarItem);
	REGISTER_ENUM (VideoBarItem);
	REGISTER_ENUM (TransportMarkerBarItem);
	REGISTER_ENUM (SelectionItem);
	REGISTER_ENUM (ControlPointItem);
	REGISTER_ENUM (GainLineItem);
	REGISTER_ENUM (AutomationLineItem);
	REGISTER_ENUM (MeterMarkerItem);
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
	REGISTER (item_type);
}
