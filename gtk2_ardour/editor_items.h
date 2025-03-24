/*
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2014 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk2_ardour_editor_items_h__
#define __gtk2_ardour_editor_items_h__

enum ItemType {
	RegionItem,
	StreamItem,
	WaveItem,
	PlayheadCursorItem,
	MarkerItem,
	SceneMarkerItem,
	MarkerBarItem,
	RangeMarkerBarItem,
	SectionMarkerBarItem,
	VideoBarItem,
	SelectionItem,
	ControlPointItem,
	GainLineItem,
	EditorAutomationLineItem,
	MeterMarkerItem,
	BBTMarkerItem,
	TempoCurveItem,
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
	FadeInTrimHandleItem,
	FadeOutItem,
	FadeOutHandleItem,
	FadeOutTrimHandleItem,
	NoteItem,
	FeatureLineItem,
	LeftFrameHandle,
	RightFrameHandle,
	StartCrossFadeItem,
	EndCrossFadeItem,
	CrossfadeViewItem,
	TimecodeRulerItem,
	MinsecRulerItem,
	BBTRulerItem,
	SamplesRulerItem,
	SelectionMarkerItem,
	DropZoneItem,
	GridZoneItem,
	VelocityItem,
	VelocityBaseItem,
	ClipStartItem,
	ClipEndItem,

	/* don't remove this */

	NoItem
};

#endif /* __gtk2_ardour_editor_items_h__ */
