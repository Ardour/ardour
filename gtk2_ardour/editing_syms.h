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
SNAPTYPE(SnapToBeatDiv128)
SNAPTYPE(SnapToBeatDiv64)
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

/* Changing this order will break step_mouse_mode */
MOUSEMODE(MouseObject)
MOUSEMODE(MouseRange)
MOUSEMODE(MouseCut)
MOUSEMODE(MouseTimeFX)
MOUSEMODE(MouseAudition)
MOUSEMODE(MouseDraw)
MOUSEMODE(MouseContent)

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
IMPORTMODE(ImportAsRegion)
IMPORTMODE(ImportToTrack)
IMPORTMODE(ImportAsTrack)
IMPORTMODE(ImportAsTapeTrack)

// if this is changed, remember to update the string table in sfdb_ui.cc
IMPORTPOSITION(ImportAtTimestamp)
IMPORTPOSITION(ImportAtEditPoint)
IMPORTPOSITION(ImportAtPlayhead)
IMPORTPOSITION(ImportAtStart)

// if this is changed, remember to update the string table in sfdb_ui.cc
IMPORTDISPOSITION(ImportDistinctFiles)
IMPORTDISPOSITION(ImportMergeFiles)
IMPORTDISPOSITION(ImportSerializeFiles)
IMPORTDISPOSITION(ImportDistinctChannels)

EDITPOINT(EditAtPlayhead)
EDITPOINT(EditAtSelectedMarker)
EDITPOINT(EditAtMouse)

INSERTTIMEOPT(LeaveIntersected)
INSERTTIMEOPT(MoveIntersected)
INSERTTIMEOPT(SplitIntersected)
