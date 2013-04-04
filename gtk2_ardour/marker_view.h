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

#ifndef __gtk_ardour_marker_view_h__
#define __gtk_ardour_marker_view_h__

#include <string>
#include "time_axis_view_item.h"


namespace Gdk {
	class Color;
}

class MarkerTimeAxisView ;
class ImageFrameView ;

/**
 * MarkerView defines a marker item that may be placed upon a MarkerTimeAxis.
 *
 * The aim of the MarkerView is to provide additional timing details for visual based time axis.
 * The MarkerView item is associated with one other TimeAxisViewItem and has a start and a duration.
 */
class MarkerView : public TimeAxisViewItem
{
	public:
		//---------------------------------------------------------------------------------------//
		// Constructor / Desctructor

		/**
		 * Constructs a new MarkerView
		 *
		 * @param parent the parent canvas item
		 * @param tv the parent TimeAxisView of this item
		 * @param marked the Item that this item is to be assciated (marking) with
		 * @param spu the current samples per unit
		 * @param base_color
		 * @param mark_type the marker type/name text, eg fade out, pan up etc.
		 * @param mark_id unique name/id of this item
		 * @param start the start time of this item
		 * @param duration the duration of this item
		 */
                 MarkerView(ArdourCanvas::Group *parent,
			TimeAxisView *tv,
			ImageFrameView* marked,
			double spu,
		        Gdk::Color& base_color,
			std::string mark_type,
			std::string mark_id,
			nframes_t start,
			nframes_t duration) ;

		/**
		 * Destructor
		 * Destroys this Marker Item and removes the association between itself and the item it is marking.
		 */
		~MarkerView() ;

		static PBD::Signal1<void,MarkerView*> CatchDeletion;

		//---------------------------------------------------------------------------------------//
		// Marker Type Methods

		/**
		 * Sets the marker Type text of this this MarkerItem, eg fade_out, pan up etc.
		 *
		 * @param type_text the marker type text of this item
		 */
		void set_mark_type_text(std::string type_text) ;

		/**
		 * Returns the marker Type of this this MarkerItem, eg fade_out, pan up etc.
		 *
		 * @return the marker type text of this item
		 */
		std::string get_mark_type_text() const ;


		//---------------------------------------------------------------------------------------//
		// Marked Item Methods

		/**
		 * Returns the time axis item being marked by this item
		 *
		 * @return the time axis item being marked by this item
		 */
		ImageFrameView* get_marked_item() ;

		/**
		 * Sets the time axis item being marker by this item
		 *
		 * @param item the time axis item to be marked by this item
		 * @return the previously marked item, or 0 if no previous marked item exists
		 */
		ImageFrameView* set_marked_item(ImageFrameView* item) ;

		//---------------------------------------------------------------------------------//
		// Emitted Signals

		/** Emitted when the mark type text is changed */
		sigc::signal<void,std::string,void*> MarkTypeChanged ;

		/** Emitted when the Marked Item is changed */
		sigc::signal<void,ImageFrameView*,void*> MarkedItemChanged ;


	protected:

	private:
		/** the unique name/id of this item */
		std::string mark_type_text ;

		/* a pointer to the time axis item this marker is assoiated(marking up) with */
		ImageFrameView* marked_item ;

} ; /* class MarkerView */


#endif /* __gtk_ardour_imageframe_view_h__ */
