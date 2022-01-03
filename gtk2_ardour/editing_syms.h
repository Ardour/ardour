/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
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

/* Changing this order will break the menu */
GRIDTYPE(GridTypeNone)
GRIDTYPE(GridTypeBar)
GRIDTYPE(GridTypeBeat)
GRIDTYPE(GridTypeBeatDiv2)
GRIDTYPE(GridTypeBeatDiv4)
GRIDTYPE(GridTypeBeatDiv8)
GRIDTYPE(GridTypeBeatDiv16)
GRIDTYPE(GridTypeBeatDiv32)
GRIDTYPE(GridTypeBeatDiv3)  //Triplet eighths
GRIDTYPE(GridTypeBeatDiv6)
GRIDTYPE(GridTypeBeatDiv12)
GRIDTYPE(GridTypeBeatDiv24)
GRIDTYPE(GridTypeBeatDiv5)  //Quintuplet eighths
GRIDTYPE(GridTypeBeatDiv10)
GRIDTYPE(GridTypeBeatDiv20)
GRIDTYPE(GridTypeBeatDiv7)  //Septuplet eighths
GRIDTYPE(GridTypeBeatDiv14)
GRIDTYPE(GridTypeBeatDiv28)
GRIDTYPE(GridTypeTimecode)
GRIDTYPE(GridTypeMinSec)
GRIDTYPE(GridTypeCDFrame)


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
IMPORTMODE(ImportAsTrigger)

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
