#ifndef __gtk2_ardour_editor_items_h__
#define __gtk2_ardour_editor_items_h__

enum ItemType {
	RegionItem,
	StreamItem,
	PlayheadCursorItem,
	EditCursorItem,
	MarkerItem,
	MarkerBarItem,
	RangeMarkerBarItem,
	TransportMarkerBarItem,
	SelectionItem,
	GainControlPointItem,
	GainLineItem,
	GainAutomationControlPointItem,
	GainAutomationLineItem,
	PanAutomationControlPointItem,
	PanAutomationLineItem,
	RedirectAutomationControlPointItem,
	RedirectAutomationLineItem,
	MeterMarkerItem,
	TempoMarkerItem,
	MeterBarItem,
	TempoBarItem,
	AudioRegionViewNameHighlight,
	AudioRegionViewName,
	StartSelectionTrimItem,
	EndSelectionTrimItem,
	AutomationTrackItem,
	FadeInItem,
	FadeInHandleItem,
	FadeOutItem,
	FadeOutHandleItem,
	
	/* <CMT Additions> */
	MarkerViewItem,
	MarkerTimeAxisItem,
	MarkerViewHandleStartItem,
	MarkerViewHandleEndItem,
	ImageFrameItem,
	ImageFrameTimeAxisItem,
	ImageFrameHandleStartItem,
	ImageFrameHandleEndItem,
	/* </CMT Additions> */
	
	CrossfadeViewItem,
	
	/* don't remove this */
	
	NoItem
};

#endif /* __gtk2_ardour_editor_items_h__ */
