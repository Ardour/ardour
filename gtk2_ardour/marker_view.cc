/*
    Copyright (C) 2003 Paul Davis

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

#include <gtkmm.h>

#include "imageframe_time_axis.h"
#include "imageframe_view.h"
#include "public_editor.h"
#include "marker_view.h"

using namespace ARDOUR ;

PBD::Signal1<void,MarkerView*> MarkerView::CatchDeletion

//---------------------------------------------------------------------------------------//
// Constructor / Desctructor

/**
 * Constructs a new MarkerView
 *
 * @param parent the parent canvas item
 * @param tv the parent TimeAxisView of this item
 * @param tavi the TimeAxisViewItem that this item is to be assciated (marking) with
 * @param spu the current samples per unit
 * @param base_color
 * @param mark_type the marker type/name text, eg fade out, pan up etc.
 * @param mark_id unique name/id of this item
 * @param start the start time of this item
 * @param duration the duration of this item
 */
MarkerView::MarkerView(ArdourCanvas::Group *parent,
		       TimeAxisView* tv,
		       ImageFrameView* marked,
		       double spu,
		       Gdk::Color& basic_color,
		       std::string mark_type,
		       std::string mark_id,
		       framepos_t start,
		       framecnt_t duration)
  : TimeAxisViewItem(mark_id, *parent,*tv,spu,basic_color,start,duration)
{
	mark_type_text = mark_type ;
	marked_item = marked ;

	// set the canvas item text to the marker type, not the id
	set_name_text(mark_type_text) ;

	// hook up our canvas events

	if (frame_handle_start) {
		frame_handle_start->signal_event().connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_markerview_start_handle_event), frame_handle_start, this));
		frame_handle_end->signal_event().connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_markerview_end_handle_event), frame_handle_end, this));
	}
	group->signal_event().connect (sigc::bind (sigc::mem_fun (PublicEditor::instance(), &PublicEditor::canvas_markerview_item_view_event), group, this));

	set_position(start, this) ;
	set_duration(duration, this) ;
}

/**
 * Destructor
 * Destroys this Marker Item and removes the association between itself and the item it is marking.
 */
MarkerView::~MarkerView()
{
	// remove the association our marked may still have to us
	if(marked_item)
	{
		marked_item->remove_marker_view_item(this, this) ;
	}
}


//---------------------------------------------------------------------------------------//
// Marker Type Methods

/**
 * Sets the marker Type text of this this MarkerItem, eg fade_out, pan up etc.
 *
 * @param type_text the marker type text of this item
 */
void
MarkerView::set_mark_type_text(std::string type_text)
{
	mark_type_text = type_text ;
	 MarkTypeChanged(mark_type_text, this) ; /* EMIT_SIGNAL */
}

/**
 * Returns the marker Type of this this MarkerItem, eg fade_out, pan up etc.
 *
 * @return the marker type text of this item
 */
std::string
MarkerView::get_mark_type_text() const
{
	return(mark_type_text) ;
}


//---------------------------------------------------------------------------------------//
// Marked Item Methods

ImageFrameView*
MarkerView::set_marked_item(ImageFrameView* item)
{
	ImageFrameView* temp = marked_item ;
	marked_item = item ;

	 MarkedItemChanged(marked_item, this) ; /* EMIT_SIGNAL */
	return(temp) ;
}

ImageFrameView*
MarkerView::get_marked_item()
{
	return(marked_item) ;
}
