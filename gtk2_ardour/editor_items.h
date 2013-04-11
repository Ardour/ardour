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

#ifndef __gtk2_ardour_editor_items_h__
#define __gtk2_ardour_editor_items_h__

enum ItemType {
	RegionItem,
	StreamItem,
	PlayheadCursorItem,
	MarkerItem,
	MarkerBarItem,
	RangeMarkerBarItem,
	CdMarkerBarItem,
	VideoBarItem,
	TransportMarkerBarItem,
	SelectionItem,
	ControlPointItem,
	GainLineItem,
	AutomationLineItem,
	MeterMarkerItem,
	TempoMarkerItem,
	MeterBarItem,
	TempoBarItem,
	RegionViewNameHighlight,
	RegionViewName,
	StartSelectionTrimItem,
	EndSelectionTrimItem,
	AutomationTrackItem,
	FadeInItem,
	FadeInHandleItem,
	FadeOutItem,
	FadeOutHandleItem,
	NoteItem,
	FeatureLineItem,
        LeftFrameHandle,
        RightFrameHandle,
	StartCrossFadeItem,
	EndCrossFadeItem,

#ifdef WITH_CMT
	MarkerViewItem,
	MarkerTimeAxisItem,
	MarkerViewHandleStartItem,
	MarkerViewHandleEndItem,
	ImageFrameItem,
	ImageFrameTimeAxisItem,
	ImageFrameHandleStartItem,
	ImageFrameHandleEndItem,
#endif

	CrossfadeViewItem,

	/* don't remove this */

	NoItem
};

#endif /* __gtk2_ardour_editor_items_h__ */
