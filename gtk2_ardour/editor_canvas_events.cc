/*
    Copyright (C) 2000 Paul Davis 

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

    $Id$
*/

#include <cstdlib>
#include <cmath>

#include <libgnomecanvas/libgnomecanvas.h>

#include <ardour/diskstream.h>
#include <ardour/audioplaylist.h>

#include "editor.h"
#include "public_editor.h"
#include "regionview.h"
#include "streamview.h"
#include "crossfade_view.h"
#include "audio_time_axis.h"
#include "region_gain_line.h"
#include "automation_gain_line.h"
#include "automation_pan_line.h"
#include "automation_time_axis.h"
#include "redirect_automation_line.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace Gtk;

gint
Editor::_canvas_copy_region_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	Editor* editor = (Editor*)data;
	return editor->canvas_copy_region_event (item, event);
}

gint
Editor::_canvas_crossfade_view_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	CrossfadeView* xfv = static_cast<CrossfadeView*> (data);
	Editor* editor = dynamic_cast<Editor*>(&xfv->get_time_axis_view().editor);
	return editor->canvas_crossfade_view_event (item, event, xfv);
}

gint
Editor::_canvas_fade_in_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	AudioRegionView* rv = static_cast<AudioRegionView*> (data);
	Editor* editor = dynamic_cast<Editor*>(&rv->get_time_axis_view().editor);
	return editor->canvas_fade_in_event (item, event, rv);
}

gint
Editor::_canvas_fade_in_handle_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	AudioRegionView* rv = static_cast<AudioRegionView*> (data);
	Editor* editor = dynamic_cast<Editor*>(&rv->get_time_axis_view().editor);
	return editor->canvas_fade_in_handle_event (item, event, rv);
}

gint
Editor::_canvas_fade_out_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	AudioRegionView* rv = static_cast<AudioRegionView*> (data);
	Editor* editor = dynamic_cast<Editor*>(&rv->get_time_axis_view().editor);
	return editor->canvas_fade_out_event (item, event, rv);
}

gint
Editor::_canvas_fade_out_handle_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	AudioRegionView* rv = static_cast<AudioRegionView*> (data);
	Editor* editor = dynamic_cast<Editor*>(&rv->get_time_axis_view().editor);
	return editor->canvas_fade_out_handle_event (item, event, rv);
}

gint
Editor::_canvas_region_view_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	AudioRegionView *rv = reinterpret_cast<AudioRegionView *>(data);
	Editor* editor = dynamic_cast<Editor*>(&rv->get_time_axis_view().editor);

	return editor->canvas_region_view_event (item, event, rv);
}

gint
Editor::_canvas_region_view_name_highlight_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	AudioRegionView *rv = reinterpret_cast<AudioRegionView *> (data);
	Editor* editor = dynamic_cast<Editor*>(&rv->get_time_axis_view().editor);

	return editor->canvas_region_view_name_highlight_event (item, event);
}

gint
Editor::_canvas_region_view_name_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	AudioRegionView *rv = reinterpret_cast<AudioRegionView *> (data);
	Editor* editor = dynamic_cast<Editor*>(&rv->get_time_axis_view().editor);
	
	return editor->canvas_region_view_name_event (item, event);
}

gint
Editor::_canvas_stream_view_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	/* note that stream views are by definition audio track views */

	AudioTimeAxisView *tv = (AudioTimeAxisView *) data;
	Editor* editor = dynamic_cast<Editor*>(&tv->editor);

	return editor->canvas_stream_view_event (item, event, tv);
}

gint
Editor::_canvas_automation_track_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	AutomationTimeAxisView* atv = (AutomationTimeAxisView*) data;
	Editor* editor = dynamic_cast<Editor*>(&atv->editor);

	return editor->canvas_automation_track_event (item, event, atv);
}

gint
Editor::_canvas_control_point_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	ControlPoint *cp = reinterpret_cast<ControlPoint *>(data);
	Editor* editor = dynamic_cast<Editor*>(&cp->line.trackview.editor);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_control_point = cp;
		clicked_trackview = &cp->line.trackview;
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		clicked_regionview = 0;
		break;

	default:
		break;
	}

	return editor->canvas_control_point_event (item, event);
}

gint
Editor::_canvas_line_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	AutomationLine *line = reinterpret_cast<AutomationLine*> (data);
	Editor* editor = dynamic_cast<Editor*>(&line->trackview.editor);

	return editor->canvas_line_event (item, event);
}

gint
Editor::_canvas_tempo_marker_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	Editor* editor = dynamic_cast<Editor*>((PublicEditor*) data);
	return editor->canvas_tempo_marker_event (item, event);
}

gint
Editor::_canvas_meter_marker_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	Editor* editor = dynamic_cast<Editor*>((PublicEditor *) data);
	return editor->canvas_meter_marker_event (item, event);
}

gint
Editor::_canvas_tempo_bar_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	/* XXX NO CAST */
	Editor* editor = (Editor*) data;
	return editor->canvas_tempo_bar_event (item, event);
}

gint
Editor::_canvas_meter_bar_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	/* XXX NO CAST */
	Editor* editor = (Editor*) data;
	return editor->canvas_meter_bar_event (item, event);
}

gint
Editor::_canvas_marker_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	Editor* editor = dynamic_cast<Editor*>((PublicEditor*) data);
	return editor->canvas_marker_event (item, event);
}

gint
Editor::_canvas_marker_bar_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	/* NO CAST */
	Editor* editor = (Editor*) data;
	return editor->canvas_marker_bar_event (item, event);
}

gint
Editor::_canvas_range_marker_bar_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	/* NO CAST */
	Editor* editor = (Editor*) data;
	return editor->canvas_range_marker_bar_event (item, event);
}

gint
Editor::_canvas_transport_marker_bar_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	/* NO CAST */
	Editor* editor = (Editor*) data;
	return editor->canvas_transport_marker_bar_event (item, event);
}

gint
Editor::_canvas_playhead_cursor_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	/* NO CAST */
	Editor* editor = (Editor*) data;
	return editor->canvas_playhead_cursor_event (item, event);
}

gint
Editor::_canvas_edit_cursor_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	/* NO CAST */
	Editor* editor = (Editor*) data;
	return editor->canvas_edit_cursor_event (item, event);
}

gint
Editor::_canvas_zoom_rect_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	Editor* editor = dynamic_cast<Editor*>((PublicEditor*) data);
	return editor->canvas_zoom_rect_event (item, event);
}

gint
Editor::_canvas_selection_rect_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	Editor* editor = dynamic_cast<Editor*>((PublicEditor*) data);
	return editor->canvas_selection_rect_event (item, event);
}

gint
Editor::_canvas_selection_start_trim_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	Editor* editor = dynamic_cast<Editor*>((PublicEditor*) data);
	return editor->canvas_selection_start_trim_event (item, event);
}

gint
Editor::_canvas_selection_end_trim_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	Editor* editor = dynamic_cast<Editor*>((PublicEditor*) data);
	return editor->canvas_selection_end_trim_event (item, event);
}

gint
Editor::_track_canvas_event (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	/* NO CAST */

	Editor* editor = (Editor*) data;
	return editor->track_canvas_event (item, event);
}

/********** END OF.TATIC EVENT HANDLERS */

gint
Editor::track_canvas_event (GnomeCanvasItem *item, GdkEvent *event)
{
	gint x, y;

	switch (event->type) {
	case GDK_MOTION_NOTIFY:
		/* keep those motion events coming */
		track_canvas->get_pointer (x, y);
		return track_canvas_motion (item, event);

	case GDK_BUTTON_RELEASE:
		switch (event->button.button) {
		case 4:
		case 5:
			button_release_handler (item, event, NoItem);
			break;
		}
		break;

	default:
		break;
	}

	return FALSE;
}

gint
Editor::track_canvas_motion (GnomeCanvasItem *item, GdkEvent *ev)
{
	if (verbose_cursor_visible) {
		verbose_canvas_cursor->set_property ("x", ev->motion.x + 20);
		verbose_canvas_cursor->set_property ("y", ev->motion.y + 20);
	}
	return FALSE;
}

gint
Editor::typed_event (GnomeCanvasItem *item, GdkEvent *event, ItemType type)
{
	gint ret = FALSE;
	
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		ret = button_press_handler (item, event, type);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, type);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, type);
		break;

	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, type);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, type);
		break;

	default:
		break;
	}
			
	return ret;
}

gint
Editor::canvas_region_view_event (GnomeCanvasItem *item, GdkEvent *event, AudioRegionView *rv)
{
	gint ret = FALSE;
	
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_trackview = &rv->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		ret = button_press_handler (item, event, RegionItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, RegionItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, RegionItem);
		break;

	case GDK_ENTER_NOTIFY:
		set_entered_regionview (rv);
		break;

	case GDK_LEAVE_NOTIFY:
		set_entered_regionview (0);
		break;

	default:
		break;
	}

	return ret;
}

gint
Editor::canvas_stream_view_event (GnomeCanvasItem *item, GdkEvent *event, AudioTimeAxisView *tv)
{
	gint ret = FALSE;
	
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = 0;
		clicked_control_point = 0;
		clicked_trackview = tv;
		clicked_audio_trackview = tv;
		ret = button_press_handler (item, event, StreamItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, StreamItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, StreamItem);
		break;

	case GDK_ENTER_NOTIFY:
		break;

	default:
		break;
	}

	return ret;
}



gint
Editor::canvas_automation_track_event (GnomeCanvasItem *item, GdkEvent *event, AutomationTimeAxisView *atv)
{
	gint ret = FALSE;
	
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = 0;
		clicked_control_point = 0;
		clicked_trackview = atv;
		clicked_audio_trackview = 0;
		ret = button_press_handler (item, event, AutomationTrackItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, AutomationTrackItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, AutomationTrackItem);
		break;

	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, AutomationTrackItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, AutomationTrackItem);
		break;

	default:
		break;
	}

	return ret;
}

gint
Editor::canvas_fade_in_event (GnomeCanvasItem *item, GdkEvent *event, AudioRegionView *rv)
{
	/* we handle only button 3 press/release events */

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_trackview = &rv->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		if (event->button.button == 3) {
			return button_press_handler (item, event, FadeInItem);
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (event->button.button == 3) {
			return button_release_handler (item, event, FadeInItem);
		}
		break;

	default:
		break;
		
	}

	/* proxy for the regionview */
	
	return canvas_region_view_event (rv->get_canvas_group(), event, rv);
}

gint
Editor::canvas_fade_in_handle_event (GnomeCanvasItem *item, GdkEvent *event, AudioRegionView *rv)
{
	gint ret = FALSE;
	
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_trackview = &rv->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		ret = button_press_handler (item, event, FadeInHandleItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, FadeInHandleItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, FadeInHandleItem);
		break;

	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, FadeInHandleItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, FadeInHandleItem);
		break;

	default:
		break;
	}

	return ret;
}

gint
Editor::canvas_fade_out_event (GnomeCanvasItem *item, GdkEvent *event, AudioRegionView *rv)
{
	/* we handle only button 3 press/release events */

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_trackview = &rv->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		if (event->button.button == 3) {
			return button_press_handler (item, event, FadeOutItem);
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (event->button.button == 3) {
			return button_release_handler (item, event, FadeOutItem);
		}
		break;

	default:
		break;
		
	}

	/* proxy for the regionview */
	
	return canvas_region_view_event (rv->get_canvas_group(), event, rv);
}

gint
Editor::canvas_fade_out_handle_event (GnomeCanvasItem *item, GdkEvent *event, AudioRegionView *rv)
{
	gint ret = FALSE;
	
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_trackview = &rv->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		ret = button_press_handler (item, event, FadeOutHandleItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, FadeOutHandleItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, FadeOutHandleItem);
		break;

	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, FadeOutHandleItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, FadeOutHandleItem);
		break;

	default:
		break;
	}

	return ret;
}

struct DescendingRegionLayerSorter {
    bool operator()(Region* a, Region* b) {
	    return a->layer() > b->layer();
    }
};

gint
Editor::canvas_crossfade_view_event (GnomeCanvasItem* item, GdkEvent* event, CrossfadeView* xfv)
{
	/* we handle only button 3 press/release events */

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		clicked_crossfadeview = xfv;
		clicked_trackview = &clicked_crossfadeview->get_time_axis_view();
		if (event->button.button == 3) {
			return button_press_handler (item, event, CrossfadeViewItem);
		} 
		break;

	case GDK_BUTTON_RELEASE:
		if (event->button.button == 3) {
			gint ret = button_release_handler (item, event, CrossfadeViewItem);
			return ret;
		}
		break;

	default:
		break;
		
	}

	
	/* proxy for the upper most regionview */

	/* XXX really need to check if we are in the name highlight,
	   and proxy to that when required.
	*/
	
	TimeAxisView& tv (xfv->get_time_axis_view());
	AudioTimeAxisView* atv;

	if ((atv = dynamic_cast<AudioTimeAxisView*>(&tv)) != 0) {

		if (atv->is_audio_track()) {

			AudioPlaylist* pl = atv->get_diskstream()->playlist();
			Playlist::RegionList* rl = pl->regions_at (event_frame (event));

			if (!rl->empty()) {
				DescendingRegionLayerSorter cmp;
				rl->sort (cmp);

				AudioRegionView* arv = atv->view->find_view (*(dynamic_cast<AudioRegion*> (rl->front())));

				/* proxy */
				
				delete rl;

				return canvas_region_view_event (arv->get_canvas_group(), event, arv);
			}
		}
	}

	return TRUE;
}

gint
Editor::canvas_control_point_event (GnomeCanvasItem *item, GdkEvent *event)
{
	ItemType type;
	ControlPoint *cp;
	
	if ((cp = static_cast<ControlPoint *> (gtk_object_get_data (GTK_OBJECT(item), "control_point"))) == 0) {
		fatal << _("programming error: control point canvas item has no control point object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	if (dynamic_cast<AudioRegionGainLine*> (&cp->line) != 0) {
		type = GainControlPointItem;
	} else if (dynamic_cast<AutomationGainLine*> (&cp->line) != 0) {
		type = GainAutomationControlPointItem;
	} else if (dynamic_cast<AutomationPanLine*> (&cp->line) != 0) {
		type = PanAutomationControlPointItem;
	} else if (dynamic_cast<RedirectAutomationLine*> (&cp->line) != 0) {
		type = RedirectAutomationControlPointItem;
	} else {
		return FALSE;
	}

	return typed_event (item, event, type);
}

gint
Editor::canvas_line_event (GnomeCanvasItem *item, GdkEvent *event)
{
	ItemType type;
	AutomationLine *al;
	
	if ((al = static_cast<AutomationLine *> (gtk_object_get_data (GTK_OBJECT(item), "line"))) == 0) {
		fatal << _("programming error: line canvas item has no line object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	if (dynamic_cast<AudioRegionGainLine*> (al) != 0) {
		type = GainLineItem;
	} else if (dynamic_cast<AutomationGainLine*> (al) != 0) {
		type = GainAutomationLineItem;
	} else if (dynamic_cast<AutomationPanLine*> (al) != 0) {
		type = PanAutomationLineItem;
	} else if (dynamic_cast<RedirectAutomationLine*> (al) != 0) {
		type = RedirectAutomationLineItem;
	} else {
		return FALSE;
	}

	return typed_event (item, event, type);
}


gint
Editor::canvas_selection_rect_event (GnomeCanvasItem *item, GdkEvent *event)
{
	gint ret = FALSE;
	SelectionRect *rect = 0;

	if ((rect = reinterpret_cast<SelectionRect*> (gtk_object_get_data (GTK_OBJECT(item), "rect"))) == 0) {
		fatal << _("programming error: no \"rect\" pointer associated with selection item") << endmsg;
		/*NOTREACHED*/
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_selection = rect->id;
		ret = button_press_handler (item, event, SelectionItem);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, SelectionItem);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, SelectionItem);
		break;
		/* Don't need these at the moment. */
	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, SelectionItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, SelectionItem);
		break;

	default:
		break;
	}
			
	return ret;
}

gint
Editor::canvas_selection_start_trim_event (GnomeCanvasItem *item, GdkEvent *event)
{
	gint ret = FALSE;
	SelectionRect *rect = 0;

	if ((rect = reinterpret_cast<SelectionRect*> (gtk_object_get_data (GTK_OBJECT(item), "rect"))) == 0) {
		fatal << _("programming error: no \"rect\" pointer associated with selection item") << endmsg;
		/*NOTREACHED*/
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_selection = rect->id;
		ret = button_press_handler (item, event, StartSelectionTrimItem);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, StartSelectionTrimItem);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, StartSelectionTrimItem);
		break;
	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, StartSelectionTrimItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, StartSelectionTrimItem);
		break;

	default:
		break;
	}
			
	return ret;
}

gint
Editor::canvas_selection_end_trim_event (GnomeCanvasItem *item, GdkEvent *event)
{
	gint ret = FALSE;
	SelectionRect *rect = 0;

	if ((rect = reinterpret_cast<SelectionRect*> (gtk_object_get_data (GTK_OBJECT(item), "rect"))) == 0) {
		fatal << _("programming error: no \"rect\" pointer associated with selection item") << endmsg;
		/*NOTREACHED*/
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_selection = rect->id;
		ret = button_press_handler (item, event, EndSelectionTrimItem);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, EndSelectionTrimItem);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, EndSelectionTrimItem);
		break;
	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, EndSelectionTrimItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, EndSelectionTrimItem);
		break;

	default:
		break;
	}
			
	return ret;
}


gint
Editor::canvas_region_view_name_highlight_event (GnomeCanvasItem *item, GdkEvent *event)
{
	gint ret = FALSE;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = reinterpret_cast<AudioRegionView *> (gtk_object_get_data(GTK_OBJECT(item), "regionview"));
		clicked_control_point = 0;
		clicked_trackview = &clicked_regionview->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		ret = button_press_handler (item, event, AudioRegionViewNameHighlight);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, AudioRegionViewNameHighlight);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, AudioRegionViewNameHighlight);
		break;
	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, AudioRegionViewNameHighlight);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, AudioRegionViewNameHighlight);
		break;

	default:
		break;
	}

	return ret;
}

gint
Editor::canvas_region_view_name_event (GnomeCanvasItem *item, GdkEvent *event)
{
	gint ret = FALSE;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = reinterpret_cast<AudioRegionView *> (gtk_object_get_data(GTK_OBJECT(item), "regionview"));
		clicked_control_point = 0;
		clicked_trackview = &clicked_regionview->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		ret = button_press_handler (item, event, AudioRegionViewName);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, AudioRegionViewName);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, AudioRegionViewName);
		break;
	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, AudioRegionViewName);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, AudioRegionViewName);
		break;

	default:
		break;
	}

	return ret;
}

gint
Editor::canvas_marker_event (GnomeCanvasItem *item, GdkEvent *event)
{
	return typed_event (item, event, MarkerItem);
}

gint
Editor::canvas_marker_bar_event (GnomeCanvasItem *item, GdkEvent *event)
{
	return typed_event (item, event, MarkerBarItem);
}

gint
Editor::canvas_range_marker_bar_event (GnomeCanvasItem *item, GdkEvent *event)
{
	return typed_event (item, event, RangeMarkerBarItem);
}

gint
Editor::canvas_transport_marker_bar_event (GnomeCanvasItem *item, GdkEvent *event)
{
	return typed_event (item, event, TransportMarkerBarItem);
}

gint
Editor::canvas_tempo_marker_event (GnomeCanvasItem *item, GdkEvent *event)
{
	return typed_event (item, event, TempoMarkerItem);
}

gint
Editor::canvas_meter_marker_event (GnomeCanvasItem *item, GdkEvent *event)
{
	return typed_event (item, event, MeterMarkerItem);
}

gint
Editor::canvas_tempo_bar_event (GnomeCanvasItem *item, GdkEvent *event)
{
	return typed_event (item, event, TempoBarItem);
}

gint
Editor::canvas_meter_bar_event (GnomeCanvasItem *item, GdkEvent *event)
{
	return typed_event (item, event, MeterBarItem);
}

gint
Editor::canvas_playhead_cursor_event (GnomeCanvasItem *item, GdkEvent *event)
{
	return typed_event (item, event, PlayheadCursorItem);
}

gint
Editor::canvas_edit_cursor_event (GnomeCanvasItem *item, GdkEvent *event)
{
	return typed_event (item, event, EditCursorItem);
}

gint
Editor::canvas_zoom_rect_event (GnomeCanvasItem *item, GdkEvent *event)
{
	return typed_event (item, event, NoItem);
}

gint
Editor::canvas_copy_region_event (GnomeCanvasItem *item GdkEvent *event)
{
	return typed_event (item, event, RegionItem);
}

