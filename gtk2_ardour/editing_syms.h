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

/* Changing this order will break the menu */
SNAPTYPE(SnapToCDFrame)
SNAPTYPE(SnapToTimecodeFrame)
SNAPTYPE(SnapToTimecodeSeconds)
SNAPTYPE(SnapToTimecodeMinutes)
SNAPTYPE(SnapToSeconds)
SNAPTYPE(SnapToMinutes)
SNAPTYPE(SnapToBeatDiv32)
SNAPTYPE(SnapToBeatDiv28)
SNAPTYPE(SnapToBeatDiv24)
SNAPTYPE(SnapToBeatDiv20)
SNAPTYPE(SnapToBeatDiv16)
SNAPTYPE(SnapToBeatDiv14)
SNAPTYPE(SnapToBeatDiv12)
SNAPTYPE(SnapToBeatDiv10)
SNAPTYPE(SnapToBeatDiv8)
SNAPTYPE(SnapToBeatDiv7)
SNAPTYPE(SnapToBeatDiv6)
SNAPTYPE(SnapToBeatDiv5)
SNAPTYPE(SnapToBeatDiv4)
SNAPTYPE(SnapToBeatDiv3)
SNAPTYPE(SnapToBeatDiv2)
SNAPTYPE(SnapToBeat)
SNAPTYPE(SnapToBar)
SNAPTYPE(SnapToMark)
SNAPTYPE(SnapToRegionStart)
SNAPTYPE(SnapToRegionEnd)
SNAPTYPE(SnapToRegionSync)
SNAPTYPE(SnapToRegionBoundary)

/* Changing this order will break the menu */
SNAPMODE(SnapOff)
SNAPMODE(SnapNormal)
SNAPMODE(SnapMagnetic)

REGIONLISTSORTTYPE(ByEndInFile)
REGIONLISTSORTTYPE(ByLength)
REGIONLISTSORTTYPE(ByName)
REGIONLISTSORTTYPE(ByPosition)
REGIONLISTSORTTYPE(BySourceFileCreationDate)
REGIONLISTSORTTYPE(BySourceFileFS)
REGIONLISTSORTTYPE(BySourceFileLength)
REGIONLISTSORTTYPE(BySourceFileName)
REGIONLISTSORTTYPE(ByStartInFile)
REGIONLISTSORTTYPE(ByTimestamp)

MOUSEMODE(MouseGain)
MOUSEMODE(MouseObject)
MOUSEMODE(MouseRange)
MOUSEMODE(MouseDraw)
MOUSEMODE(MouseTimeFX)
MOUSEMODE(MouseZoom)
MOUSEMODE(MouseAudition)

/* Changing this order will break the menu */
ZOOMFOCUS(ZoomFocusLeft)
ZOOMFOCUS(ZoomFocusRight)
ZOOMFOCUS(ZoomFocusCenter)
ZOOMFOCUS(ZoomFocusPlayhead)
ZOOMFOCUS(ZoomFocusMouse)
ZOOMFOCUS(ZoomFocusEdit)

DISPLAYCONTROL(FollowPlayhead)
DISPLAYCONTROL(ShowMeasures)
DISPLAYCONTROL(ShowWaveforms)
DISPLAYCONTROL(ShowWaveformsRecording)

// if this is changed, remember to update the string table in sfdb_ui.cc
IMPORTMODE(ImportAsRegion=0)
IMPORTMODE(ImportToTrack=1)
IMPORTMODE(ImportAsTrack=2)
IMPORTMODE(ImportAsTapeTrack=3)

// if this is changed, remember to update the string table in sfdb_ui.cc
IMPORTPOSITION(ImportAtTimestamp=0)
IMPORTPOSITION(ImportAtEditPoint=1)
IMPORTPOSITION(ImportAtPlayhead=2)
IMPORTPOSITION(ImportAtStart=3)

// if this is changed, remember to update the string table in sfdb_ui.cc
IMPORTDISPOSITION(ImportDistinctFiles=0)
IMPORTDISPOSITION(ImportMergeFiles=1)
IMPORTDISPOSITION(ImportSerializeFiles=2)
IMPORTDISPOSITION(ImportDistinctChannels=3)

EDITPOINT(EditAtPlayhead)
EDITPOINT(EditAtSelectedMarker)
EDITPOINT(EditAtMouse)

INSERTTIMEOPT(LeaveIntersected)
INSERTTIMEOPT(MoveIntersected)
INSERTTIMEOPT(SplitIntersected)
