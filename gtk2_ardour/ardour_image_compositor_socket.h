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

    $Id$
*/


#ifndef __ardour_image_comp_h__
#define __ardour_image_comp_h__

#include <string> 

namespace ardourvis
{
	/** Simple version number */
	const int32_t MSG_VERSION = 1 ;
	
	/** the default port we use */
	const int32_t DEFAULT_PORT = 30000 ;
	
	/** the maximum buffer size we will use to send receive a message (image data handled differently) */
	const int32_t MAX_MSG_SIZE = 256 ;
	
	/** the number of characters used for a value describing the characters within a textual data element */
	const int32_t TEXT_SIZE_CHARS = 3 ;
	
	/** the number of characters we use for time values within a message */
	const int32_t TIME_VALUE_CHARS = 10 ;
	
	/** the number of charachters we use for other value data, ie image width/height values */
	const int32_t IMAGE_SIZE_CHARS = 3 ;
	
	/** the number of characters used to for the size of the image data message */
	const int32_t IMAGE_DATA_MESSAGE_SIZE_CHARS = 32 ;

	// ------------------------------------------------------------------------- //
	// Main Actions
	// we join the action chars with items to create the message
	// with the exception of the return values, all messages begin with one
	// of these message parts
	
	/** Insert an Item */
	const std::string INSERT_ITEM = "IN" ;
	
	/** Remove an Item */
	const std::string REMOVE_ITEM = "RM" ;
	
	/** Rename a named item */
	const std::string RENAME_ITEM = "MV" ;
	
	/** Request some aditional data */
	const std::string REQUEST_DATA = "RQ" ;
	
	/** Return of a data request */
	const std::string RETURN_DATA = "RD" ;
	
	/** Update a item */
	const std::string ITEM_UPDATE = "IU" ;
	
	/** Select an Item */
	const std::string ITEM_SELECTED = "IS" ;
	
	/** Sesion Action */
	const std::string SESSION_ACTION = "SA" ;
	
	/** Sesion Action */
	const std::string SHUTDOWN = "SD" ;
	

	// ------------------------------------------------------------------------- //
	// Return values
	const std::string RETURN_TRUE  = "RT1" ;
	const std::string RETURN_FALSE = "RT0" ;
	
	
	
	// ------------------------------------------------------------------------- //
	// Updateable attributes
	
	/** Update the position of a time axis item */
	const std::string POSITION_CHANGE = "PC" ;
	
	/** Update the duration of a time axis item */
	const std::string DURATION_CHANGE = "DC" ;
	
	/** Enable the position lock constraint no a time axis item */
	const std::string POSITION_LOCK_CHANGE = "PL" ;
	
	/** Enable the duration lock constraint no a time axis item */
	const std::string DURATION_LOCK_CHANGE = "PL" ;
	
	/** Update the Maximum duration of a time axis item (_Upper _Duration) */
	const std::string MAX_DURATION_CHANGE = "UD" ;
	
	/** Enable the Maximum duration constraint of a time axis item (_Enable _Upper (Duration)) */
	const std::string MAX_DURATION_ENABLE_CHANGE = "EU" ;
	
	/** Update the Minimum duration of a time axis item (_Lowerr _Duration) */
	const std::string MIN_DURATION_CHANGE = "LD" ;
	
	/** Enable the Minimum duration constraint of a time axis item (_Enable _Lower (Duration)) */
	const std::string MIN_DURATION_ENABLE_CHANGE = "EL" ;
	
	/** Refresh the image data of an imageframe item (original image has been altered?) */
	const std::string IMAGE_REFRESH = "IR" ;
	
	/** the session sample rate has changed */
	const std::string SAMPLE_RATE_CHANGE = "RC" ;
	
	
	
	// ------------------------------------------------------------------------- //
	// Requestable data items
	
	/** RGB data of the iamge */
	// this is probably a bad choice of string !
	const std::string IMAGE_RGB_DATA = "ID" ;
	
	/** the (path) name of the Ardour session */
	const std::string SESSION_NAME = "SN" ;
	
	/** the current sample rate */
	const std::string SAMPLE_RATE = "SR" ;
	
	/** the (path) name of the image compositor session */
	const std::string COMPOSITOR_SESSION = "CS" ;
	
	
	// ------------------------------------------------------------------------- //
	// Session Actions - follwed by session path
	
	/** Close a session */
	const std::string CLOSE_SESSION = "CS" ;
	
	/** Open a session */
	const std::string OPEN_SESSION = "OS" ;
	
	
	
	// ------------------------------------------------------------------------- //
	// Items
	
	const std::string IMAGEFRAME_TIME_AXIS = "IT" ;
	const std::string MARKER_TIME_AXIS = "MT" ;
	const std::string IMAGEFRAME_ITEM = "II" ;
	const std::string MARKER_ITEM = "MI" ;
	
	/** or an ImageFrameTimeAxisGroup */
	const std::string IMAGEFRAME_GROUP = "IG" ;
	
} /* namespace ardour_visual */

#endif /* __ardour_image_comp_socket_h__ */
