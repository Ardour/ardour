/*
    Copyright (C) 2000-2003 Paul Davis
    Written by Colin Law, CMT, Glasgow

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

#include "imageframe_view.h"
#include "imageframe_time_axis.h"
#include "imageframe_time_axis_view.h"
#include "imageframe_time_axis_group.h"
#include "marker_time_axis_view.h"
#include "marker_time_axis.h"
#include "marker_view.h"
#include "editor.h"
#include "i18n.h"

#include <gtkmm2ext/gtk_ui.h>
#include "pbd/error.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "imageframe_socket_handler.h"
#include "ardour_image_compositor_socket.h"
#include "public_editor.h"
#include "gui_thread.h"

using namespace Gtk;
using namespace PBD;
using namespace std;

TimeAxisView*
Editor::get_named_time_axis(const string & name)
{
	TimeAxisView* tav = 0 ;

	for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i)
	{
		if (((TimeAxisView*)*i)->name() == name)
		{
			tav = ((TimeAxisView*)*i) ;
			break ;
		}
	}
	return(tav) ;
}

/* <CMT Additions file="editor.cc"> */

void
Editor::add_imageframe_time_axis(const string & track_name, void* src)
{
	// check for duplicate name
	if(get_named_time_axis(track_name))
	{
		warning << "Repeated time axis name" << std::endl ;
	}
	else
	{
		Gtkmm2ext::UI::instance()->call_slot (boost::bind (&Editor::handle_new_imageframe_time_axis_view, this,track_name, src));
	}
}

void
Editor::connect_to_image_compositor()
{
	if(image_socket_listener == 0)
	{
		image_socket_listener = ImageFrameSocketHandler::create_instance(*this) ;
	}

	if(image_socket_listener->is_connected() == true)
	{
		return ;
	}

	// XXX should really put this somewhere safe
	const char * host_ip = "127.0.0.1" ;

	bool retcode = image_socket_listener->connect(host_ip, ardourvis::DEFAULT_PORT) ;

	if(retcode == false)
	{
		// XXX need to get some return status here
		warning << "Image Compositor Connection attempt failed" << std::endl ;
		return ;
	}

	// add the socket to the gui loop, and keep the retuned tag value of the input
	gint tag = gdk_input_add(image_socket_listener->get_socket_descriptor(), GDK_INPUT_READ,ImageFrameSocketHandler::image_socket_callback,image_socket_listener) ;
	image_socket_listener->set_gdk_input_tag(tag) ;
}

void
Editor::scroll_timeaxis_to_imageframe_item(const TimeAxisViewItem* item)
{
	// GTK2FIX
	//framepos_t offset = static_cast<framepos_t>(frames_per_unit * (edit_hscroll_slider_width/2)) ;
	framepos_t offset = 0;

	framepos_t x_pos = 0 ;

	if (item->get_position() < offset) {
		x_pos = 0 ;
	} else {
		x_pos = item->get_position() - offset + (item->get_duration() / 2);
	}

	reset_x_origin (x_pos);
}

void
Editor::add_imageframe_marker_time_axis(const string & track_name, TimeAxisView* marked_track, void* src)
{
	// Can we only sigc::bind 2 data Items?
	// @todo we really want to sigc::bind the src attribute too, for the moment tracks can only be added remotely,
	//       so this is not too much of an issue, however will need to be looked at again
	Gtkmm2ext::UI::instance()->call_slot (boost::bind (&Editor::handle_new_imageframe_marker_time_axis_view, this, track_name, marked_track));
}

void
Editor::popup_imageframe_edit_menu(int button, int32_t time, ArdourCanvas::Item* ifv, bool with_item)
{
	ImageFrameTimeAxis* ifta = dynamic_cast<ImageFrameTimeAxis*>(clicked_axisview) ;

	if(ifta)
	{
		ImageFrameTimeAxisGroup* iftag = ifta->get_view()->get_selected_imageframe_group() ;

		if(iftag)
		{
			ImageFrameView* selected_ifv = ifta->get_view()->get_selected_imageframe_view() ;
			ifta->popup_imageframe_edit_menu(button, time, selected_ifv, with_item) ;
		}
	}
}

void
Editor::popup_marker_time_axis_edit_menu(int button, int32_t time, ArdourCanvas::Item* ifv, bool with_item)
{
	MarkerTimeAxis* mta = dynamic_cast<MarkerTimeAxis*>(clicked_axisview) ;

	if(mta)
	{
		MarkerView* selected_mv = mta->get_view()->get_selected_time_axis_item() ;
		if(selected_mv)
		{
			mta->popup_marker_time_axis_edit_menu(button,time, selected_mv, with_item) ;
		}
	}
}
/* </CMT Additions file="editor.cc"> */

/* <CMT Additions file="editor_canvas_events.cc"> */
bool
Editor::canvas_imageframe_item_view_event (GdkEvent *event, ArdourCanvas::Item* item, ImageFrameView *ifv)
{
	gint ret = FALSE ;
	ImageFrameTimeAxisGroup* iftag = 0 ;

	switch (event->type)
	{
		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
			clicked_axisview = &ifv->get_time_axis_view();
			iftag = ifv->get_time_axis_group() ;
			dynamic_cast<ImageFrameTimeAxis*>(clicked_axisview)->get_view()->set_selected_imageframe_view(iftag, ifv);
			ret = button_press_handler (item, event, ImageFrameItem) ;
			break ;
		case GDK_BUTTON_RELEASE:
			ret = button_release_handler (item, event, ImageFrameItem) ;
			break ;
		case GDK_MOTION_NOTIFY:
			ret = motion_handler (item, event, ImageFrameItem) ;
			break ;
		default:
			break ;
	}
	return(ret) ;
}

bool
Editor::canvas_imageframe_start_handle_event (GdkEvent *event, ArdourCanvas::Item* item, ImageFrameView *ifv)
{
	gint ret = FALSE ;
	ImageFrameTimeAxisGroup* iftag = 0 ;

	switch (event->type)
	{
		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
			clicked_axisview = &ifv->get_time_axis_view() ;
			iftag = ifv->get_time_axis_group() ;
			dynamic_cast<ImageFrameTimeAxis*>(clicked_axisview)->get_view()->set_selected_imageframe_view(iftag, ifv);

			ret = button_press_handler (item, event, ImageFrameHandleStartItem) ;
			break ;
		case GDK_BUTTON_RELEASE:
			ret = button_release_handler (item, event, ImageFrameHandleStartItem) ;
			break;
		case GDK_MOTION_NOTIFY:
			ret = motion_handler (item, event, ImageFrameHandleStartItem) ;
			break ;
		case GDK_ENTER_NOTIFY:
			ret = enter_handler (item, event, ImageFrameHandleStartItem) ;
			break ;
		case GDK_LEAVE_NOTIFY:
			ret = leave_handler (item, event, ImageFrameHandleStartItem) ;
			break ;
		default:
			break ;
	}
	return(ret) ;
}

bool
Editor::canvas_imageframe_end_handle_event (GdkEvent *event, ArdourCanvas::Item* item, ImageFrameView *ifv)
{
	gint ret = FALSE ;
	ImageFrameTimeAxisGroup* iftag = 0 ;

	switch (event->type)
	{
		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
			clicked_axisview = &ifv->get_time_axis_view() ;
			iftag = ifv->get_time_axis_group() ;
			dynamic_cast<ImageFrameTimeAxis*>(clicked_axisview)->get_view()->set_selected_imageframe_view(iftag, ifv);

			ret = button_press_handler (item, event, ImageFrameHandleEndItem) ;
			break ;
		case GDK_BUTTON_RELEASE:
			ret = button_release_handler (item, event, ImageFrameHandleEndItem) ;
			break ;
		case GDK_MOTION_NOTIFY:
			ret = motion_handler (item, event, ImageFrameHandleEndItem) ;
			break ;
		case GDK_ENTER_NOTIFY:
			ret = enter_handler (item, event, ImageFrameHandleEndItem) ;
			break ;
		case GDK_LEAVE_NOTIFY:
			ret = leave_handler (item, event, ImageFrameHandleEndItem);
			break ;
		default:
			break ;
	}
	return(ret) ;
}

bool
Editor::canvas_imageframe_view_event (GdkEvent* event, ArdourCanvas::Item* item, ImageFrameTimeAxis* ifta)
{
	gint ret = FALSE ;
	switch (event->type)
	{
		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
			clicked_axisview = ifta ;
			ret = button_press_handler (item, event, ImageFrameTimeAxisItem) ;
			break ;
		case GDK_BUTTON_RELEASE:
			ret = button_release_handler (item, event, ImageFrameTimeAxisItem) ;
			break ;
		case GDK_MOTION_NOTIFY:
			break ;
		default:
			break ;
	}
	return(ret) ;
}

bool
Editor::canvas_marker_time_axis_view_event (GdkEvent* event, ArdourCanvas::Item* item, MarkerTimeAxis* mta)
{
	gint ret = FALSE ;
	switch (event->type)
	{
		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
			clicked_axisview = mta ;
			ret = button_press_handler(item, event, MarkerTimeAxisItem) ;
			break ;
		case GDK_BUTTON_RELEASE:
			ret = button_release_handler(item, event, MarkerTimeAxisItem) ;
			break ;
		case GDK_MOTION_NOTIFY:
		default:
			break ;
	}
	return(ret) ;
}


bool
Editor::canvas_markerview_item_view_event (GdkEvent* event, ArdourCanvas::Item* item, MarkerView* mta)
{
	gint ret = FALSE ;
	switch (event->type)
	{
		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
			clicked_axisview = &mta->get_time_axis_view() ;
			dynamic_cast<MarkerTimeAxis*>(clicked_axisview)->get_view()->set_selected_time_axis_item(mta);
			ret = button_press_handler(item, event, MarkerViewItem) ;
			break ;
		case GDK_BUTTON_RELEASE:
			ret = button_release_handler(item, event, MarkerViewItem) ;
			break ;
		case GDK_MOTION_NOTIFY:
			ret = motion_handler(item, event, MarkerViewItem) ;
			break ;
		default:
			break ;
	}
	return(ret) ;
}

bool
Editor::canvas_markerview_start_handle_event (GdkEvent* event, ArdourCanvas::Item* item, MarkerView* mta)
{
	gint ret = FALSE ;
	switch (event->type)
	{
		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
			clicked_axisview = &mta->get_time_axis_view() ;
			dynamic_cast<MarkerTimeAxis*>(clicked_axisview)->get_view()->set_selected_time_axis_item(mta) ;
			ret = button_press_handler(item, event, MarkerViewHandleStartItem) ;
			break ;
		case GDK_BUTTON_RELEASE:
			ret = button_release_handler(item, event, MarkerViewHandleStartItem) ;
			break ;
		case GDK_MOTION_NOTIFY:
			ret = motion_handler(item, event, MarkerViewHandleStartItem) ;
			break ;
		case GDK_ENTER_NOTIFY:
			ret = enter_handler(item, event, MarkerViewHandleStartItem) ;
			break ;
		case GDK_LEAVE_NOTIFY:
			ret = leave_handler(item, event, MarkerViewHandleStartItem) ;
			break ;
		default:
			break ;
	}
	return(ret) ;
}

bool
Editor::canvas_markerview_end_handle_event (GdkEvent* event, ArdourCanvas::Item* item, MarkerView* mta)
{
	gint ret = FALSE ;
	switch (event->type)
	{
		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
			clicked_axisview = &mta->get_time_axis_view() ;
			dynamic_cast<MarkerTimeAxis*>(clicked_axisview)->get_view()->set_selected_time_axis_item(mta) ;
			ret = button_press_handler(item, event, MarkerViewHandleEndItem) ;
			break ;
		case GDK_BUTTON_RELEASE:
			ret = button_release_handler(item, event, MarkerViewHandleEndItem) ;
			break ;
		case GDK_MOTION_NOTIFY:
			ret = motion_handler(item, event, MarkerViewHandleEndItem) ;
			break ;
		case GDK_ENTER_NOTIFY:
			ret = enter_handler(item, event, MarkerViewHandleEndItem) ;
			break ;
		case GDK_LEAVE_NOTIFY:
			ret = leave_handler(item, event, MarkerViewHandleEndItem) ;
			break ;
		default:
			break ;
	}
	return(ret) ;
}


/* </CMT Additions file="editor_canvas_events.cc"> */


/*
	---------------------------------------------------------------------------------------------------
	---------------------------------------------------------------------------------------------------
	---------------------------------------------------------------------------------------------------
*/



/* <CMT Additions file="editor_mouse.cc"> */

void
Editor::start_imageframe_grab(ArdourCanvas::Item* item, GdkEvent* event)
{
	ImageFrameView* ifv = ((ImageFrameTimeAxis*)clicked_axisview)->get_view()->get_selected_imageframe_view() ;
	drag_info.copy = false ;
	drag_info.item = item ;
	drag_info.data = ifv ;
	drag_info.motion_callback = &Editor::imageframe_drag_motion_callback;
	drag_info.finished_callback = &Editor::timeaxis_item_drag_finished_callback;
	drag_info.last_frame_position = ifv->get_position() ;

	drag_info.source_trackview = &ifv->get_time_axis_view() ;
	drag_info.dest_trackview = drag_info.source_trackview;

	/* this is subtle. raising the regionview itself won't help,
	   because raise_to_top() just puts the item on the top of
	   its parent's stack. so, we need to put the trackview canvas_display group
	   on the top, since its parent is the whole canvas.

	   however, this hides the measure bars within that particular trackview,
	   so move them to the top afterwards.
	*/

	drag_info.item->raise_to_top();
	drag_info.source_trackview->canvas_display->raise_to_top();
	//time_line_group->raise_to_top();
	cursor_group->raise_to_top ();

	start_grab(event) ;

	drag_info.pointer_frame_offset = pixel_to_frame(drag_info.grab_x) - drag_info.last_frame_position;
}


void
Editor::start_markerview_grab(ArdourCanvas::Item* item, GdkEvent* event)
{
	MarkerView* mv = ((MarkerTimeAxis*)clicked_axisview)->get_view()->get_selected_time_axis_item() ;
	drag_info.copy = false ;
	drag_info.item = item ;
	drag_info.data = mv ;
	drag_info.motion_callback = &Editor::markerview_drag_motion_callback;
	drag_info.finished_callback = &Editor::timeaxis_item_drag_finished_callback;
	drag_info.last_frame_position = mv->get_position() ;

	drag_info.source_trackview = &mv->get_time_axis_view() ;
	drag_info.dest_trackview = drag_info.source_trackview;

	/* this is subtle. raising the regionview itself won't help,
	   because raise_to_top() just puts the item on the top of
	   its parent's stack. so, we need to put the trackview canvas_display group
	   on the top, since its parent is the whole canvas.

	   however, this hides the measure bars within that particular trackview,
	   so move them to the top afterwards.
	*/

	drag_info.item->raise_to_top();
	drag_info.source_trackview->canvas_display->raise_to_top();
	//time_line_group->raise_to_top();
	cursor_group->raise_to_top ();

	start_grab(event) ;

	drag_info.pointer_frame_offset = pixel_to_frame(drag_info.grab_x) - drag_info.last_frame_position ;
}


void
Editor::markerview_drag_motion_callback(ArdourCanvas::Item*, GdkEvent* event)
{
	double cx, cy ;

  	MarkerView* mv = reinterpret_cast<MarkerView*>(drag_info.data) ;
	framepos_t pending_region_position ;
	framepos_t pointer_frame ;

	pointer_frame = canvas_event_frame(event, &cx, &cy) ;

  	snap_to(pointer_frame) ;

	if (pointer_frame > (framepos_t) drag_info.pointer_frame_offset)
	{
		pending_region_position = pointer_frame - drag_info.pointer_frame_offset ;
		snap_to(pending_region_position) ;

		// we dont allow marker items to extend beyond, or in front of the marked items so
		// cap the value to the marked items position and duration
		if((pending_region_position + mv->get_duration()) >= ((mv->get_marked_item()->get_position()) + (mv->get_marked_item()->get_duration())))
		{
			pending_region_position = (mv->get_marked_item()->get_position() + mv->get_marked_item()->get_duration()) - (mv->get_duration()) ;
		}
		else if(pending_region_position <= mv->get_marked_item()->get_position())
		{
			pending_region_position = mv->get_marked_item()->get_position() ;
		}
  	}
	else
	{
		pending_region_position = mv->get_marked_item()->get_position() ;
  	}

	drag_info.last_frame_position = pending_region_position ;

	// we treat this as a special case, usually we want to send the identitiy of the caller
	// but in this case, that would trigger our socket handler to handle the event, sending
	// notification to the image compositor. This would be fine, except that we have not
	// finished the drag, we therefore do not want to sent notification until we have
	// completed the drag, only then do we want the image compositor notofied.
	// We therefore set the caller identity to the special case of 0
	mv->set_position(pending_region_position, 0) ;

	show_verbose_time_cursor(pending_region_position) ;
}

void
Editor::imageframe_drag_motion_callback(ArdourCanvas::Item*, GdkEvent* event)
{
	double cx, cy ;

	ImageFrameView* ifv = reinterpret_cast<ImageFrameView*>(drag_info.data) ;

	framepos_t pending_region_position;
	framepos_t pointer_frame;

	pointer_frame = canvas_event_frame(event, &cx, &cy) ;

	snap_to(pointer_frame) ;

	if (pointer_frame > (framepos_t) drag_info.pointer_frame_offset)
	{
		pending_region_position = pointer_frame - drag_info.pointer_frame_offset ;
		snap_to(pending_region_position) ;
	}
	else
	{
		pending_region_position = 0 ;
	}

	drag_info.grab_x = cx;
	//drag_info.last_frame_position = pending_region_position ;
	drag_info.current_pointer_frame = pending_region_position ;

	// we treat this as a special case, usually we want to send the identitiy of the caller
	// but in this case, that would trigger our socket handler to handle the event, sending
	// notification to the image compositor. This would be fine, except that we have not
	// finished the drag, we therefore do not want to sent notification until we have
	// completed the drag, only then do we want the image compositor notofied.
	// We therefore set the caller identity to the special case of 0
	ifv->set_position(pending_region_position, 0) ;

	show_verbose_time_cursor(pending_region_position) ;
}

void
Editor::timeaxis_item_drag_finished_callback(ArdourCanvas::Item*, GdkEvent* event)
{
	framepos_t where ;
	TimeAxisViewItem* tavi = reinterpret_cast<TimeAxisViewItem*>(drag_info.data) ;

	bool item_x_movement = (drag_info.last_frame_position != tavi->get_position()) ;

	hide_verbose_canvas_cursor() ;

	/* no x or y movement either means the regionview hasn't been moved, or has been moved
	   but is back in it's original position/trackview.*/

	if(!item_x_movement && event && event->type == GDK_BUTTON_RELEASE)
	{
		/* No motion: either set the current region, or align the clicked region
		   with the current one.
		 */
		 return;
	}

	if(item_x_movement)
	{
		/* base the new region position on the current position of the regionview.*/
		where = drag_info.current_pointer_frame ;

		// final call to set position after the motion to tell interested parties of the new position
		tavi->set_position(where, this) ;
	}
	else
	{
		//where = tavi->get_position() ;
	}

/*
 	//locate so user can audition the edit
	if ( !session->transport_rolling() && Config->get_always_play_range()) {
		locate_with_edit_preroll ( arv->region()->position() );
	}
*/
}


void
Editor::imageframe_start_handle_op(ArdourCanvas::Item* item, GdkEvent* event)
{
	// get the selected item from the parent time axis
	ImageFrameTimeAxis* ifta = dynamic_cast<ImageFrameTimeAxis*>(clicked_axisview) ;
	if(ifta)
	{
		ImageFrameView* ifv = ifta->get_view()->get_selected_imageframe_view() ;

		if (ifv == 0) {
			fatal << _("programming error: no ImageFrameView selected") << endmsg;
			/*NOTREACHED*/
			return ;
		}

		drag_info.item = ifv->get_canvas_frame() ;
		drag_info.data = ifv;
		drag_info.grab_x = event->motion.x;
		drag_info.cumulative_x_drag = 0;
		drag_info.motion_callback = &Editor::imageframe_start_handle_trim_motion ;
		drag_info.finished_callback = &Editor::imageframe_start_handle_end_trim ;

		start_grab(event) ;

		show_verbose_time_cursor(ifv->get_position(), 10) ;
	}
}

void
Editor::imageframe_end_handle_op(ArdourCanvas::Item* item, GdkEvent* event)
{
	// get the selected item from the parent time axis
	ImageFrameTimeAxis* ifta = dynamic_cast<ImageFrameTimeAxis*>(clicked_axisview) ;

	if(ifta)
	{
		ImageFrameView* ifv = ifta->get_view()->get_selected_imageframe_view() ;

		if (ifv == 0)
		{
			fatal << _("programming error: no ImageFrameView selected") << endmsg ;
			/*NOTREACHED*/
			return ;
		}

		drag_info.item = ifv->get_canvas_frame() ;
		drag_info.data = ifv ;
		drag_info.grab_x = event->motion.x ;
		drag_info.cumulative_x_drag = 0 ;
		drag_info.motion_callback = &Editor::imageframe_end_handle_trim_motion ;
		drag_info.finished_callback = &Editor::imageframe_end_handle_end_trim ;

		start_grab(event, trimmer_cursor) ;

		show_verbose_time_cursor(ifv->get_position() + ifv->get_duration(), 10) ;
	}
}

void
Editor::imageframe_start_handle_trim_motion(ArdourCanvas::Item* item, GdkEvent* event)
{
	ImageFrameView* ifv = reinterpret_cast<ImageFrameView*> (drag_info.data) ;

	framepos_t start = 0 ;
	framepos_t end = 0 ;
	framepos_t pointer_frame = canvas_event_frame(event) ;

	// chekc th eposition of the item is not locked
	if(!ifv->get_position_locked()) {
		snap_to(pointer_frame) ;

		if(pointer_frame != drag_info.last_pointer_frame) {
			start = ifv->get_position() ;
			end = ifv->get_position() + ifv->get_duration() ;

			if (pointer_frame > end) {
				start = end ;
			} else {
				start = pointer_frame ;
			}

			// are we getting bigger or smaller?
			framepos_t new_dur_val = end - start ;

			// start handle, so a smaller pointer frame increases our component size
			if(pointer_frame <= drag_info.grab_frame)
			{
				if(ifv->get_max_duration_active() && (new_dur_val > ifv->get_max_duration()))
				{
					new_dur_val = ifv->get_max_duration() ;
					start = end - new_dur_val ;
				}
				else
				{
					// current values are ok
				}
			}
			else
			{
				if(ifv->get_min_duration_active() && (new_dur_val < ifv->get_min_duration()))
				{
					new_dur_val = ifv->get_min_duration() ;
					start = end - new_dur_val ;
				}
				else
				{
					// current values are ok
				}
			}

			drag_info.last_pointer_frame = pointer_frame ;

			/* re-calculatethe duration and position of the imageframeview */
			drag_info.cumulative_x_drag = new_dur_val ;

			// we treat this as a special case, usually we want to send the identitiy of the caller
			// but in this case, that would trigger our socket handler to handle the event, sending
			// notification to the image compositor. This would be fine, except that we have not
			// finished the drag, we therefore do not want to sent notification until we have
			// completed the drag, only then do we want the image compositor notofied.
			// We therefore set the caller identity to the special case of 0
			ifv->set_duration(new_dur_val, 0) ;
			ifv->set_position(start, 0) ;
		}
	}

	show_verbose_time_cursor(start, 10) ;
}

void
Editor::imageframe_start_handle_end_trim(ArdourCanvas::Item* item, GdkEvent* event)
{
	ImageFrameView* ifv = reinterpret_cast<ImageFrameView *> (drag_info.data) ;

	if (drag_info.cumulative_x_drag == 0)
	{
		/* just a click */
	}
	else
	{
		framepos_t temp = ifv->get_position() + ifv->get_duration() ;

		ifv->set_position((framepos_t) (temp - drag_info.cumulative_x_drag), this) ;
		ifv->set_duration((framepos_t) drag_info.cumulative_x_drag, this) ;
	}
}

void
Editor::imageframe_end_handle_trim_motion(ArdourCanvas::Item* item, GdkEvent* event)
{
	ImageFrameView* ifv = reinterpret_cast<ImageFrameView *> (drag_info.data) ;

	framepos_t start = 0 ;
	framepos_t end = 0 ;
	framepos_t pointer_frame = canvas_event_frame(event) ;
	framepos_t new_dur_val = 0 ;

	snap_to(pointer_frame) ;

	if (pointer_frame != drag_info.last_pointer_frame)
	{
		start = ifv->get_position() ;
		end = ifv->get_position() + ifv->get_duration() ;
		if (pointer_frame < start)
		{
			end = start ;
		}
		else
		{
			end = pointer_frame ;
		}

		new_dur_val = end - start ;

		// are we getting bigger or smaller?
		if(pointer_frame >= drag_info.last_pointer_frame)
		{
			if(ifv->get_max_duration_active() && (new_dur_val > ifv->get_max_duration()))
			{
				new_dur_val = ifv->get_max_duration() ;
			}
		}
		else
		{
			if(ifv->get_min_duration_active() && (new_dur_val < ifv->get_min_duration()))
			{
				new_dur_val = ifv->get_min_duration() ;
			}
		}

		drag_info.last_pointer_frame = pointer_frame ;
		drag_info.cumulative_x_drag = new_dur_val ;

		// we treat this as a special case, usually we want to send the identitiy of the caller
		// but in this case, that would trigger our socket handler to handle the event, sending
		// notification to the image compositor. This would be fine, except that we have not
		// finished the drag, we therefore do not want to sent notification until we have
		// completed the drag, only then do we want the image compositor notofied.
		// We therefore set the caller identity to the special case of 0
		ifv->set_duration(new_dur_val, 0) ;
	}

	show_verbose_time_cursor(new_dur_val, 10) ;
}


void
Editor::imageframe_end_handle_end_trim (ArdourCanvas::Item* item, GdkEvent* event)
{
	ImageFrameView* ifv = reinterpret_cast<ImageFrameView *> (drag_info.data) ;

	if (drag_info.cumulative_x_drag == 0)
	{
		/* just a click */
	}
	else
	{
		framepos_t new_duration = (framepos_t)drag_info.cumulative_x_drag ;
		if((new_duration <= ifv->get_max_duration()) && (new_duration >= ifv->get_min_duration()))
		{
			ifv->set_duration(new_duration, this) ;
		}
	}
}


void
Editor::markerview_item_start_handle_op(ArdourCanvas::Item* item, GdkEvent* event)
{
	MarkerView* mv = reinterpret_cast<MarkerTimeAxis*>(clicked_axisview)->get_view()->get_selected_time_axis_item() ;

	if (mv == 0)
	{
		fatal << _("programming error: no MarkerView selected") << endmsg ;
		/*NOTREACHED*/
		return ;
	}

	drag_info.item = mv->get_canvas_frame() ;
 	drag_info.data = mv;
 	drag_info.grab_x = event->motion.x;

 	drag_info.cumulative_x_drag = 0 ;
	drag_info.motion_callback = &Editor::markerview_start_handle_trim_motion ;
 	drag_info.finished_callback = &Editor::markerview_start_handle_end_trim ;

	start_grab(event, trimmer_cursor) ;
}

void
Editor::markerview_item_end_handle_op(ArdourCanvas::Item* item, GdkEvent* event)
{
	MarkerView* mv = reinterpret_cast<MarkerTimeAxis*>(clicked_axisview)->get_view()->get_selected_time_axis_item() ;
	if (mv == 0)
	{
		fatal << _("programming error: no MarkerView selected") << endmsg ;
		/*NOTREACHED*/
		return ;
	}

	drag_info.item = mv->get_canvas_frame() ;
	drag_info.data = mv ;
	drag_info.grab_x = event->motion.x ;
 	drag_info.cumulative_x_drag = 0 ;

	drag_info.motion_callback = &Editor::markerview_end_handle_trim_motion ;
 	drag_info.finished_callback = &Editor::markerview_end_handle_end_trim ;

	start_grab(event, trimmer_cursor) ;
}


void
Editor::markerview_start_handle_trim_motion(ArdourCanvas::Item* item, GdkEvent* event)
{
	MarkerView* mv = reinterpret_cast<MarkerView*> (drag_info.data) ;

	framepos_t start = 0 ;
	framepos_t end = 0 ;
	framepos_t pointer_frame = canvas_event_frame(event) ;

	// chekc th eposition of the item is not locked
	if(!mv->get_position_locked())
	{
		snap_to(pointer_frame) ;
		if(pointer_frame != drag_info.last_pointer_frame)
		{
			start = mv->get_position() ;
			end = mv->get_position() + mv->get_duration() ;

			if (pointer_frame > end)
			{
				start = end ;
			}
			else
			{
				start = pointer_frame ;
			}

			// are we getting bigger or smaller?
			framepos_t new_dur_val = end - start ;

			if(pointer_frame <= drag_info.grab_frame)
			{
				if(mv->get_max_duration_active() && (new_dur_val > mv->get_max_duration()))
				{
					new_dur_val = mv->get_max_duration() ;
					start = end - new_dur_val ;
				}
				else
				{
					// current values are ok
				}
			}
			else
			{
				if(mv->get_min_duration_active() && (new_dur_val < mv->get_min_duration()))
				{
					new_dur_val = mv->get_min_duration() ;
					start = end - new_dur_val ;
				}
				else
				{
					// current values are ok
				}
			}

			drag_info.last_pointer_frame = pointer_frame ;

			/* re-calculatethe duration and position of the imageframeview */
			drag_info.cumulative_x_drag = new_dur_val ;

			// we treat this as a special case, usually we want to send the identitiy of the caller
			// but in this case, that would trigger our socket handler to handle the event, sending
			// notification to the image compositor. This would be fine, except that we have not
			// finished the drag, we therefore do not want to sent notification until we have
			// completed the drag, only then do we want the image compositor notofied.
			// We therefore set the caller identity to the special case of 0
			mv->set_duration(new_dur_val, 0) ;
			mv->set_position(start, 0) ;
		}
	}

	show_verbose_time_cursor(start, 10) ;
}

void
Editor::markerview_start_handle_end_trim(ArdourCanvas::Item* item, GdkEvent* event)
{
	MarkerView* mv = reinterpret_cast<MarkerView*> (drag_info.data) ;

	if (drag_info.cumulative_x_drag == 0)
	{
		/* just a click */
	}
	else
	{
		framepos_t temp = mv->get_position() + mv->get_duration() ;

		mv->set_position((framepos_t) (temp - drag_info.cumulative_x_drag), this) ;
		mv->set_duration((framepos_t) drag_info.cumulative_x_drag, this) ;
	}
}

void
Editor::markerview_end_handle_trim_motion(ArdourCanvas::Item* item, GdkEvent* event)
{
	MarkerView* mv = reinterpret_cast<MarkerView*> (drag_info.data) ;

	framepos_t start = 0 ;
	framepos_t end = 0 ;
	framepos_t pointer_frame = canvas_event_frame(event) ;
	framepos_t new_dur_val = 0 ;

	snap_to(pointer_frame) ;

	if (pointer_frame != drag_info.last_pointer_frame)
	{
		start = mv->get_position() ;
		end = mv->get_position() + mv->get_duration() ;

		if(pointer_frame < start)
		{
			end = start ;
		}
		else
		{
			end = pointer_frame ;
		}

		new_dur_val = end - start ;

		// are we getting bigger or smaller?
		if(pointer_frame >= drag_info.last_pointer_frame)
		{
			// we cant extend beyond the item we are marking
			ImageFrameView* marked_item = mv->get_marked_item() ;
			framepos_t marked_end = marked_item->get_position() + marked_item->get_duration() ;

			if(mv->get_max_duration_active() && (new_dur_val > mv->get_max_duration()))
			{
				if((start + mv->get_max_duration()) > marked_end)
				{
					new_dur_val = marked_end - start ;
				}
				else
				{
					new_dur_val = mv->get_max_duration() ;
				}
			}
			else if(end > marked_end)
			{
				new_dur_val = marked_end - start ;
			}
		}
		else
		{
			if(mv->get_min_duration_active() && (new_dur_val < mv->get_min_duration()))
			{
				new_dur_val = mv->get_min_duration() ;
			}
		}


		drag_info.last_pointer_frame = pointer_frame ;
		drag_info.cumulative_x_drag = new_dur_val ;

		// we treat this as a special case, usually we want to send the identitiy of the caller
		// but in this case, that would trigger our socket handler to handle the event, sending
		// notification to the image compositor. This would be fine, except that we have not
		// finished the drag, we therefore do not want to sent notification until we have
		// completed the drag, only then do we want the image compositor notofied.
		// We therefore set the caller identity to the special case of 0
		mv->set_duration(new_dur_val, 0) ;
	}

	show_verbose_time_cursor(new_dur_val, 10) ;
}


void
Editor::markerview_end_handle_end_trim (ArdourCanvas::Item* item, GdkEvent* event)
{
	MarkerView* mv = reinterpret_cast<MarkerView*> (drag_info.data) ;

	if (drag_info.cumulative_x_drag == 0)
	{
		/* just a click */
	}
	else
	{
		framepos_t new_duration = (framepos_t)drag_info.cumulative_x_drag ;
		mv->set_duration(new_duration, this) ;
	}
}


/* </CMT Additions file="editor_mouse.cc"> */







/* <CMT Additions file="editor_route_list.cc"> */

void
Editor::handle_new_imageframe_time_axis_view(const string & track_name, void* src)
{
	ImageFrameTimeAxis* iftav ;
	iftav = new ImageFrameTimeAxis(track_name, *this, *session, *track_canvas) ;
	iftav->set_time_axis_name(track_name, this) ;
	track_views.push_back(iftav) ;

	TreeModel::Row row = *(route_display_model->append());

	row[route_display_columns.text] = iftav->name();
	row[route_display_columns.tv] = iftav;
	route_list_display.get_selection()->select (row);

	iftav->gui_changed.connect(sigc::mem_fun(*this, &Editor::handle_gui_changes)) ;
}

void
Editor::handle_new_imageframe_marker_time_axis_view(const string & track_name, TimeAxisView* marked_track)
{
	MarkerTimeAxis* mta = new MarkerTimeAxis (*this, *this->session(), *track_canvas, track_name, marked_track) ;
	((ImageFrameTimeAxis*)marked_track)->add_marker_time_axis(mta, this) ;
	track_views.push_back(mta) ;

	TreeModel::Row row = *(route_display_model->append());

	row[route_display_columns.text] = mta->name();
	row[route_display_columns.tv] = mta;
	route_list_display.get_selection()->select (row);
}


/* </CMT Additions file="editor_route_list.cc"> */
