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

#include <iostream>
#include <iomanip>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstdio>

#include <sigc++/bind.h>

#include <gtkmm2ext/gtk_ui.h>

#include "imageframe_socket_handler.h"
#include "imageframe_time_axis.h"
#include "imageframe_time_axis_view.h"
#include "imageframe_time_axis_group.h"
#include "imageframe_view.h"
#include "marker_time_axis.h"
#include "marker_time_axis_view.h"
#include "ardour_ui.h"
#include "public_editor.h"
#include "gui_thread.h"

#include "i18n.h"

#include <ardour/session.h>

#include <unistd.h>

using namespace std;
using namespace ardourvis ;
using namespace sigc;
using namespace ARDOUR;

ImageFrameSocketHandler* ImageFrameSocketHandler::_instance = 0 ;

/**
 * Constructs a new ImageFrameSocketHandler to handle communication between Ardour and the Image Compositor
 *
 * @param ed the PublicEditor
 */
ImageFrameSocketHandler::ImageFrameSocketHandler(PublicEditor& ed) : thePublicEditor(ed), theArdourToCompositorSocket(-1)
{
	
}

/**
 * Descructor
 * this will shutdown the socket if open
 */
ImageFrameSocketHandler::~ImageFrameSocketHandler()
{
	close_connection() ;
}


/**
 * Returns the instance of the ImageFrameSocketHandler
 * the instance should first be created with createInstance
 *
 * @return the instance of the ImageFrameSocketHandler
 */
ImageFrameSocketHandler* 
ImageFrameSocketHandler::get_instance()
{
	return(_instance) ;
}

/**
 * Create an new instance of the ImageFrameSocketHandler, if one does not already exist
 *
 * @param ed the Ardour PublicEditor
 */
ImageFrameSocketHandler*
ImageFrameSocketHandler::create_instance(PublicEditor& ed)
{
	if(_instance)
	{
		return(_instance) ;
	}
	else
	{
		_instance = new ImageFrameSocketHandler(ed) ;
		return(_instance) ;
	}
}

/**
 * call back to handle doing the processing work
 * This method is added to the gdk main loop and called when there is data
 * upon the socket.
 *
 */
void
ImageFrameSocketHandler::image_socket_callback(void *arg, int32_t fd, GdkInputCondition cond)
{
	char buf[ardourvis::MAX_MSG_SIZE + 1] ;
	memset(buf, 0, (ardourvis::MAX_MSG_SIZE + 1)) ;
	buf[ardourvis::MAX_MSG_SIZE] = '\0' ;

	int retcode = ::recv(fd, buf, MAX_MSG_SIZE, 0) ;
	if (retcode == 0)
	{
		//end-of-file, other end closed or shutdown?
		ARDOUR_UI::instance()->popup_error(_("Image Compositor Socket has been shutdown/closed"));
		
		// assume socket has been shutdown, tell, someone interested,
		// and remove the socket from the event loop
		ImageFrameSocketHandler* ifsh = ImageFrameSocketHandler::get_instance() ;
		gdk_input_remove(ifsh->theGdkInputTag) ;
		ifsh->close_connection() ;
		 ifsh->CompositorSocketShutdown() ; /* EMIT_SIGNAL */
	}
	if(retcode > 0)
	{
		//std::cout << "Received Msg [" << buf << "]\n" ;
		ImageFrameSocketHandler* ifsh = ImageFrameSocketHandler::get_instance() ;
		
		std::string mType = ifsh->get_message_part(0,2,buf) ;

		if(mType == ardourvis::INSERT_ITEM)
		{
			ifsh->handle_insert_message(buf) ;
		}
		else if (mType == ardourvis::REMOVE_ITEM)
		{
			ifsh->handle_remove_message(buf) ;
		}
		else if (mType == ardourvis::RENAME_ITEM)
		{
			ifsh->handle_rename_message(buf) ;
		}
		else if (mType == ardourvis::ITEM_UPDATE)
		{
			ifsh->handle_item_update_message(buf) ;
		}
		else if (mType == ardourvis::REQUEST_DATA)
		{
			ifsh->handle_request_data(buf) ;
		}
		else if (mType == ardourvis::ITEM_SELECTED)
		{
			ifsh->handle_item_selected(buf) ;
		}
		else if(mType == ardourvis::SESSION_ACTION)
		{
			ifsh->handle_session_action(buf) ;
		}
		else
		{
			std::string errMsg = "Unknown Message type : " ; 
			errMsg.append(mType) ;
			ifsh->send_return_failure(errMsg) ;
		}	
	}
}

/**
 * Attempt to connect to the image compositor on the specified host and port
 *
 * @param hostIp the ip address of the image compositor host
 * @param port the oprt number to attemp the connection on
 * @return true if the connection was a succees
 *         false otherwise
 */
bool
ImageFrameSocketHandler::connect(const char * hostIp, int32_t port)
{
	if (is_connected())
	{
		//already connected...
		return(true) ;
	}
	
	theArdourToCompositorSocket = socket(AF_INET, SOCK_STREAM, 0) ;
	if(theArdourToCompositorSocket == -1)
	{
		return(false) ;
	}
	
	int on = 1 ;
	setsockopt(theArdourToCompositorSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on)) ;
	
	sockaddr_in m_addr ;
	m_addr.sin_family = AF_INET ;
	m_addr.sin_port = htons(port) ;
	m_addr.sin_addr.s_addr = inet_addr(hostIp) ;
	
	int status = ::connect(theArdourToCompositorSocket, (sockaddr *) &m_addr, sizeof(m_addr)) ;
	
	if(status == -1)
	{
		theArdourToCompositorSocket = -1 ;	
		return(false) ;
	}
	
	return(true) ;
}

/**
 * Closes the connection to th Image Compositor
 *
 */
 void
 ImageFrameSocketHandler::close_connection()
 {
	if(is_connected())
	{
	 	::close(theArdourToCompositorSocket) ;
		theArdourToCompositorSocket = -1 ;
	}
 }

/**
 * Returns true if this ImagFrameSocketHandler is currently connected to rthe image compositor
 *
 * @return true if connected to the image compositor
 */
bool
ImageFrameSocketHandler::is_connected()
{
	return(theArdourToCompositorSocket == -1 ? false : true) ;
}

/**
 * Sets the tag used to describe this input within gtk
 * this is returned when gdk_input_add is called and is required to remove the input
 *
 * @param tag the gdk input tag of this input
 */
void
ImageFrameSocketHandler::set_gdk_input_tag(int tag)
{
	theGdkInputTag = tag ;
}

/**
 * Returns the gdk input tag of this input
 *
 * @return the gdk input tag of this input
 * @see setGdkInputTag
 */
int
ImageFrameSocketHandler::get_gdk_input_tag()
{
	return(theGdkInputTag) ;
}

/**
 * Returns the socket file descriptor
 *
 * @return the Sockt file descriptor
 */
int
ImageFrameSocketHandler::get_socket_descriptor()
{
	return(theArdourToCompositorSocket) ;
}




//---------------------------------------------------------------------------------------//
// Handle Sending messages to the Image Compositor
		
//----------------------------
// ImageFrameTimeAxis Messages

/**
 * Sends a message stating that the named image frame time axis has been removed
 *
 * @param track_id the unique id of the removed image frame time axis
 * @param src the identity of the object that initiated the change
 */
void
ImageFrameSocketHandler::send_imageframe_time_axis_removed(const string & track_id, void* src)
{
	if(this == src || src == 0)
	{
		// ie the change originated from us, then dont send any message back
		return ;
	}
	
	// create a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::REMOVE_ITEM << ardourvis::IMAGEFRAME_TIME_AXIS ;
	
	// add the id length, and the id
	msgBuffer << std::setw(3) << track_id.length() ;
	msgBuffer << track_id ;
	
	send_message(msgBuffer.str()) ;

	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}
		
/**
 * Sends a message indicating that an ImageFrameTimeAxis has been renamed
 *
 * @param new_id the new name, or Id, of the track
 * @param old_id the old name, or Id, of the track
 * @param src the identity of the object that initiated the change
 * @param time_axis the time axis that has changed
 */
void
ImageFrameSocketHandler::send_imageframe_time_axis_renamed(const string & new_id, const string & old_id, void* src, ImageFrameTimeAxis* time_axis)
{
	// ENSURE_GUI_THREAD(SigC::bind (mem_fun(*this, &ImageFrameSocketHandler::send_imageframe_time_axis_renamed), new_id, old_id, src, time_axis));
	
	if(this == src || src == 0)
	{
		// ie the change originated from us, then dont send any message back
		return ;
	}
	
	// create a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::RENAME_ITEM << ardourvis::IMAGEFRAME_TIME_AXIS ;
	
	// add the old id and length
	msgBuffer << std::setw(3) << old_id.length() ;
	msgBuffer << old_id ;
	
	// add the new id and length
	msgBuffer << std::setw(3) << new_id.length() ;
	msgBuffer << new_id ;
	
	send_message(msgBuffer.str()) ;

	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}

//------------------------
// MarkerTimeAxis Messages
		
/**
 * Sends a message stating that the named marker time axis has been removed
 *
 * @param track_id the unique id of the removed image frame time axis
 * @param src the identity of the object that initiated the change
 */
void
ImageFrameSocketHandler::send_marker_time_axis_removed(const string & track_id, void* src)
{
	if(this == src || src == 0)
	{
		// ie the change originated from us, then dont send any message back
		return ;
	}
	
	// create a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::REMOVE_ITEM << ardourvis::MARKER_TIME_AXIS ;
	
	// add the id length, and the id
	msgBuffer << std::setw(3) << track_id.length() ;
	msgBuffer << track_id ;
	
	send_message(msgBuffer.str()) ;

	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}
		
/**
 * Sends a message indicating that an MarkerTimeAxis has been renamed
 *
 * @param new_id the new name, or Id, of the track
 * @param old_id the old name, or Id, of the track
 * @param src the identity of the object that initiated the change
 * @param time_axis the time axis that has changed
 */
void
ImageFrameSocketHandler::send_marker_time_axis_renamed(const string & new_id, const string & old_id, void* src, MarkerTimeAxis* time_axis)
{
	// ENSURE_GUI_THREAD(bind (mem_fun(*this, &ImageFrameSocketHandler::send_marker_time_axis_renamed), new_id, old_id, src, time_axis));
	
	if(this == src || src == 0)
	{
		// ie the change originated from us, then dont send any message back
		return ;
	}
	
	// ctreate a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::RENAME_ITEM << ardourvis::MARKER_TIME_AXIS ;
	
	// add the old id and length
	msgBuffer << std::setw(3) << old_id.length() ;
	msgBuffer << old_id ;
	
	// add the new id and length
	msgBuffer << std::setw(3) << new_id.length() ;
	msgBuffer << new_id ;
	
	send_message(msgBuffer.str()) ;
	
	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}

//---------------------------------
// ImageFrameTimeAxisGroup Messages

/**
 * Sends a message stating that the group has been removed
 *
 * @param group_id the unique id of the removed image frame time axis
 * @param src the identity of the object that initiated the change
 * @param group the group that has changed
 */
void
ImageFrameSocketHandler::send_imageframe_time_axis_group_removed(const string & group_id, void* src, ImageFrameTimeAxisGroup* group)
{
	if(this == src || src == 0)
	{
		// ie the change originated from us, then dont send any message back
		return ;
	}
	
	// create a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::REMOVE_ITEM << ardourvis::IMAGEFRAME_GROUP ;
	
	// add the id length, and the id of the parent image time axis
	std::string track_id = group->get_view().trackview().name() ;
	msgBuffer << std::setw(3) << track_id.length() ;
	msgBuffer << track_id ;
	
	// add the group id and length
	msgBuffer << std::setw(3) << group_id.length() ;
	msgBuffer << group_id ;
	
	send_message(msgBuffer.str()) ;

	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}

/**
 * Send a message indicating that an ImageFrameTimeAxisGroup has been renamed
 *
 * @param new_id the new name, or Id, of the group
 * @param old_id the old name, or Id, of the group
 * @param src the identity of the object that initiated the change
 * @param group the group that has changed
 */
void
ImageFrameSocketHandler::send_imageframe_time_axis_group_renamed(const string & new_id, const string & old_id, void* src, ImageFrameTimeAxisGroup* group)
{
	// ENSURE_GUI_THREAD(bind (mem_fun(*this, &ImageFrameSocketHandler::send_imageframe_time_axis_group_renamed), new_id, old_id, src, group));
	
	if(this == src || src == 0)
	{
		// ie the change originated from us, then dont send any message back
		return ;
	}
	
	// ctreate a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::RENAME_ITEM << ardourvis::IMAGEFRAME_GROUP ;
	
	// add the track this group is upon
	std::string track_id = group->get_view().trackview().name() ;
	msgBuffer << std::setw(3) << track_id.length() << track_id ; 
	
	// add the old id and length
	msgBuffer << std::setw(3) << old_id.length() ;
	msgBuffer << old_id ;
	
	// add the new id and length
	msgBuffer << std::setw(3) << new_id.length() ;
	msgBuffer << new_id ;
	
	send_message(msgBuffer.str()) ;
	
	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}


//---------------------------------
// ImageFrameView Messages
		
/**
 * Send an Image Frame View Item position changed message
 *
 * @param pos the new position value
 * @param src the identity of the object that initiated the change
 * @param item the time axis item whos position has changed
 */
void
ImageFrameSocketHandler::send_imageframe_view_position_change(jack_nframes_t pos, void* src, ImageFrameView* item)
{
	// ENSURE_GUI_THREAD(bind (mem_fun(*this, &ImageFrameSocketHandler::send_imageframe_view_position_change), pos, src, item));
	
	if(this == src || src == 0)
	{
		return ;
	}
	
	// create a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::ITEM_UPDATE << ardourvis::IMAGEFRAME_ITEM << ardourvis::POSITION_CHANGE ;
	
	// add the item description
	this->compose_imageframe_item_desc(item, msgBuffer) ;

	msgBuffer << std::setw(ardourvis::TIME_VALUE_CHARS) << pos ;
	
	send_message(msgBuffer.str()) ;
	
	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}
		
/**
 * Send a Image Frame View item duration changed message
 *
 * @param dur the the new duration value
 * @param src the identity of the object that initiated the change
 * @param item the item which has had a duration change
 */
void
ImageFrameSocketHandler::send_imageframe_view_duration_change(jack_nframes_t dur, void* src, ImageFrameView* item)
{
	// ENSURE_GUI_THREAD(bind (mem_fun(*this, &ImageFrameSocketHandler::send_imageframe_view_duration_change), dur, src, item));
	
	if(this == src || src == 0)
	{
		return ;
	}
	
	// create a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::ITEM_UPDATE << ardourvis::IMAGEFRAME_ITEM << ardourvis::DURATION_CHANGE ;
	
	this->compose_imageframe_item_desc(item, msgBuffer) ;

	msgBuffer << std::setw(ardourvis::TIME_VALUE_CHARS) << dur ;
	
	send_message(msgBuffer.str()) ;
	
	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}
		
/**
 * Send a message indicating that an ImageFrameView has been renamed
 *
 * @param item the ImageFrameView which has been renamed
 * @param src the identity of the object that initiated the change
 * @param item the renamed item
 */
void
ImageFrameSocketHandler::send_imageframe_view_renamed(const string & new_id, const string & old_id, void* src, ImageFrameView* item)
{
	if(this == src || src == 0)
	{
		// ie the change originated from us, then dont send any message back
		return ;
	}
	
	// ctreate a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::RENAME_ITEM << ardourvis::IMAGEFRAME_ITEM ;
	
	this->compose_imageframe_item_desc(item, msgBuffer) ;
	
	// add the old id and length
	msgBuffer << std::setw(3) << old_id.length() ;
	msgBuffer << old_id ;
	
	send_message(msgBuffer.str()) ;
	
	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}
		
/**
 * Send a message indicating that an ImageFrameView item has been removed message
 *
 * @param item_id the id of the item that was removed
 * @param item the removed item
 */
void
ImageFrameSocketHandler::send_imageframe_view_removed(const string & item_id, void* src, ImageFrameView* item)
{
	if(this == src || src == 0)
	{
		// ie the change originated from us, then dont send any message back
		return ;
	}
	
	// create a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::REMOVE_ITEM << ardourvis::IMAGEFRAME_ITEM ;
	
	// add the id length, and the id
	ImageFrameTimeAxisGroup* parentGroup = item->get_time_axis_group() ;
	std::string group_id = parentGroup->get_group_name() ;
	std::string track_id = parentGroup->get_view().trackview().name() ;
	msgBuffer << std::setw(3) << track_id.length() << track_id ;
	msgBuffer << std::setw(3) << group_id.length() << group_id ;
	msgBuffer << std::setw(3) << item_id.length() << item_id ;
	
	send_message(msgBuffer.str()) ;

	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}




//---------------------------------
// MarkerView Messages
		
/**
 * Send a Marker View Item position changed message
 *
 * @param pos the new position value
 * @param src the identity of the object that initiated the change
 * @param item the time axis item whos position has changed
 */
void
ImageFrameSocketHandler::send_marker_view_position_change(jack_nframes_t pos, void* src, MarkerView* item)
{
	if(this == src || src == 0)
	{
		return ;
	}
	
	// create a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::ITEM_UPDATE << ardourvis::MARKER_ITEM << ardourvis::POSITION_CHANGE ;
	
	// add the item description
	this->compose_marker_item_desc(item, msgBuffer) ;

	msgBuffer << std::setw(ardourvis::TIME_VALUE_CHARS) << pos ;
	
	send_message(msgBuffer.str()) ;

	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}
		
/**
 * Send a Marker View item duration changed message
 *
 * @param dur the new duration value
 * @param src the identity of the object that initiated the change
 * @param item the time axis item whos position has changed
 */
void
ImageFrameSocketHandler::send_marker_view_duration_change(jack_nframes_t dur, void* src, MarkerView* item)
{
	if(this == src || src == 0)
	{
		return ;
	}
	
	// create a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::ITEM_UPDATE << ardourvis::MARKER_ITEM << ardourvis::DURATION_CHANGE ;
	
	this->compose_marker_item_desc(item, msgBuffer) ;

	msgBuffer << std::setw(ardourvis::TIME_VALUE_CHARS) << dur ;
	
	send_message(msgBuffer.str()) ;

	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}	

		
/**
 * Send a message indicating that a MarkerView has been renamed
 *
 * @param new_id the new_id of the object
 * @param old_id the old_id of the object
 * @param src the identity of the object that initiated the change
 * @param item the MarkerView which has been renamed
 */
void
ImageFrameSocketHandler::send_marker_view_renamed(const string & new_id, const string & old_id, void* src, MarkerView* item)
{
	if(this == src || src == 0)
	{
		// ie the change originated from us, then dont send any message back
		return ;
	}
	
	// ctreate a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::RENAME_ITEM << ardourvis::MARKER_ITEM ;
	
	this->compose_marker_item_desc(item, msgBuffer) ;
	
	// add the old id and length
	msgBuffer << std::setw(3) << old_id.length() ;
	msgBuffer << old_id ;
	
	send_message(msgBuffer.str()) ;
	
	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}
		
/**
 * Send a message indicating that a MarkerView  item has been removed message
 *
 * @param item_id the id of the item that was removed
 * @param src the identity of the object that initiated the change
 * @param item the MarkerView which has been removed
 */
void
ImageFrameSocketHandler::send_marker_view_removed(const string & item_id, void* src, MarkerView* item) 
{
	if(this == src || src == 0)
	{
		// ie the change originated from us, then dont send any message back
		return ;
	}
	
	// create a message buffer
	std::ostringstream msgBuffer ;
	msgBuffer << std::setfill('0') ;
	
	// add the msg type
	msgBuffer << ardourvis::REMOVE_ITEM << ardourvis::MARKER_ITEM ;
	
	// add the id length, and the id
	std::string track_id = item->get_time_axis_view().name() ;
	msgBuffer << std::setw(3) << track_id.length() << track_id ;
	msgBuffer << std::setw(3) << item_id.length() << item_id ;
	
	send_message(msgBuffer.str()) ;

	// XXX should do something with the return
	std::string retmsg ;
	read_message(retmsg) ;
}











//---------------------------------------------------------------------------------------//
//---------------------------------------------------------------------------------------//
//---------------------------------------------------------------------------------------//
// Message breakdown ie avoid a big if...then...else
	

/**
 * Handle insert item requests
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_insert_message(const char* msg)
{
	// handle the insert item message
	// determine the object type to insert based upon characters 2-3
	
	std::string oType = get_message_part(2,2,msg) ;

	if(oType == ardourvis::IMAGEFRAME_TIME_AXIS)
	{
		this->handle_insert_imageframe_time_axis(msg) ;
	}
	else if (oType == ardourvis::MARKER_TIME_AXIS)
	{
		this->handle_insert_marker_time_axis(msg) ;
	}
	else if (oType == ardourvis::IMAGEFRAME_GROUP)
	{
		this->handle_insert_imageframe_group(msg) ;
	}
	else if (oType == ardourvis::IMAGEFRAME_ITEM)
	{
		this->handle_insert_imageframe_view(msg) ;
	}
	else if (oType == ardourvis::MARKER_ITEM)
	{
		this->handle_insert_marker_view(msg) ;
	}
	else
	{
		std::string errMsg = "Unknown Object type during insert: " ; 
		errMsg.append(oType) ;
		send_return_failure(errMsg) ;
	}
}

/**
 * Handle remove item requests
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_remove_message(const char* msg)
{
	// handle the removal of an item message
	// determine the object type to remove based upon characters 2-3
	
	std::string oType = get_message_part(2,2,msg) ;

	if(oType == ardourvis::IMAGEFRAME_TIME_AXIS)
	{
		this->handle_remove_imageframe_time_axis(msg) ;
	}
	else if (oType == ardourvis::MARKER_TIME_AXIS)
	{
		this->handle_remove_marker_time_axis(msg) ;
	}
	else if (oType == ardourvis::IMAGEFRAME_ITEM)
	{
		this->handle_remove_imageframe_view(msg) ;
	}
	else if (oType == ardourvis::MARKER_ITEM)
	{
		this->handle_remove_marker_view(msg) ;
	}
	else
	{
		std::string errMsg = "Unknown Object type during Remove: " ; 
		errMsg.append(oType) ;
		send_return_failure(errMsg) ;
	}
}

/**
 * Handle rename item requests
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_rename_message(const char* msg)
{
	// handle the renaming of an item message
	// determine the object type to rename based upon characters 2-3
	
	std::string oType = get_message_part(2,2,msg) ;
	
	if(oType == ardourvis::IMAGEFRAME_TIME_AXIS)
	{
		this->handle_rename_imageframe_time_axis(msg) ;
	}
	else if (oType == ardourvis::MARKER_TIME_AXIS)
	{
		this->handle_rename_marker_time_axis(msg) ;
	}
	else if (oType == ardourvis::IMAGEFRAME_ITEM)
	{
		this->handle_rename_imageframe_view(msg) ;
	}
	else if (oType == ardourvis::MARKER_ITEM)
	{
		this->handle_rename_marker_view(msg) ;
	}
	else
	{
		std::string errMsg = "Unknown Object type during Rename: " ; 
		errMsg.append(oType) ;
		send_return_failure(errMsg) ;
	}
}

/**
 * Handle a request for session information
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_request_data(const char* msg)
{
	// determine the request type
	std::string reqType = get_message_part(2,2,msg) ;
	
	if(reqType == ardourvis::SESSION_NAME)
	{
		handle_session_name_request(msg) ;
	}
}

/**
 * Handle the update of a particular item
 *
 * @param msg the received message
 */
void 
ImageFrameSocketHandler::handle_item_update_message(const char* msg)
{
	// determin the object that requires updating, characters 2-3
	std::string oType = get_message_part(2,2,msg) ;
	
	// What needs updating? chars 4-5
	std::string  attr = get_message_part(4,2,msg) ;
	
	if(oType == ardourvis::IMAGEFRAME_ITEM)
	{
		if(attr == ardourvis::POSITION_CHANGE)
		{
			handle_imageframe_view_position_update(msg) ;
		}
		else if(attr == ardourvis::DURATION_CHANGE)
		{
			handle_imageframe_view_duration_update(msg) ;
		}
		else if(attr == ardourvis::POSITION_LOCK_CHANGE)
		{
			handle_imageframe_position_lock_update(msg) ;
		}
		else if(attr == ardourvis::MAX_DURATION_CHANGE)
		{
			handle_imageframe_view_max_duration_update(msg) ;
		}
		else if(attr == ardourvis::MAX_DURATION_ENABLE_CHANGE)
		{
			handle_imageframe_view_max_duration_enable_update(msg) ;
		}
		else if(attr == ardourvis::MIN_DURATION_CHANGE)
		{
			handle_imageframe_view_min_duration_update(msg) ;
		}
		else if(attr == ardourvis::MIN_DURATION_ENABLE_CHANGE)
		{
			handle_imageframe_view_min_duration_enable_update(msg) ;
		}
		else
		{
			std::string errMsg = "Unknown Attribute during Item Update: " ; 
			errMsg.append(oType) ;
			send_return_failure(errMsg) ;
		}
	}
	else if(oType == ardourvis::MARKER_ITEM)
	{
		if(attr == ardourvis::POSITION_CHANGE)
		{
			handle_marker_view_position_update(msg) ;
		}
		else if(attr == ardourvis::DURATION_CHANGE)
		{
			handle_marker_view_duration_update(msg) ;
		}
		else
		{
			std::string errMsg = "Unknown Attribute during Item Update: " ; 
			errMsg.append(oType) ;
			send_return_failure(errMsg) ;
		}
	}
	else
	{
		std::string errMsg = "Unknown Object type during Item Update: " ; 
		errMsg.append(oType) ;
		send_return_failure(errMsg) ;
	}
}

/**
 * Handle the selection of an Item
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_item_selected(const char* msg)
{
	// determine the object that requires updating, characters 2-3
	std::string oType = get_message_part(2,2,msg) ;
	
	if(oType == std::string(ardourvis::IMAGEFRAME_ITEM))
	{
		int position = 4 ; // message type chars
	
		std::string track_id ;
		std::string scene_id ;
		std::string item_id ;
		int track_id_size ;
		int scene_id_size ;
		int item_id_size ;
	
		this->decompose_imageframe_item_desc(msg, position, track_id, track_id_size, scene_id, scene_id_size, item_id, item_id_size) ;
		
		// get the named time axis
		ImageFrameTimeAxis* ifta = dynamic_cast<ImageFrameTimeAxis*>(thePublicEditor.get_named_time_axis(track_id)) ;
		
		if(!ifta)
		{
			send_return_failure(std::string("No parent Image Track found : ").append(track_id)) ;
		}
		else
		{
			// get the parent scene
			ImageFrameTimeAxisGroup* iftag = ifta->get_view()->get_named_imageframe_group(scene_id) ;
			if(!iftag)
			{
				send_return_failure(std::string("No parent Scene found : ").append(scene_id)) ;
			}
			else
			{
				ImageFrameView* ifv = iftag->get_named_imageframe_item(item_id) ;
				if(!ifv)
				{
					send_return_failure(std::string("No Image Frame Item found : ").append(item_id)) ;
				}
				else
				{
					ifv->set_selected(true, this) ;
					ifta->get_view()->set_selected_imageframe_view(iftag, ifv) ;

					thePublicEditor.scroll_timeaxis_to_imageframe_item(ifv) ;
					send_return_success() ;
				}
			}
		}
	}
}

/**
 * Handle s session action message
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_session_action(const char* msg)
{
	std::string actionType = get_message_part(2,2,msg) ;
	
	if(actionType == ardourvis::OPEN_SESSION)
	{
		this->handle_open_session(msg) ;
	}
}









//---------------------------------------------------------------------------------------//
// handlers for specific insert procedures
	
/**
 * Handle the insertion of a new ImaegFrameTimeAxis
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_insert_imageframe_time_axis(const char* msg)
{
	int position = 4 ; // message type chars
	
	// get the ImageFrameTrack name size
	int track_name_size = atoi(get_message_part(position, ardourvis::TEXT_SIZE_CHARS, msg).c_str()) ;
	position += ardourvis::TEXT_SIZE_CHARS ;
	
	// get the image frame track name
	std::string track_name = get_message_part(position, track_name_size, msg) ;
	position += track_name_size ;
	
	// check we dont already have an time axis with that name
	TimeAxisView* tav = thePublicEditor.get_named_time_axis(track_name) ;
	if(tav)
	{
		std::string errmsg("Track already exists: ") ;
		errmsg.append(track_name) ;
		send_return_failure(errmsg) ;
	}
	else
	{
		thePublicEditor.add_imageframe_time_axis(track_name, this) ;
		TimeAxisView* new_tav = thePublicEditor.get_named_time_axis(track_name) ;
	
		if(new_tav)
		{
			ImageFrameTimeAxis* ifta = (ImageFrameTimeAxis*)new_tav ;
			ifta->VisualTimeAxisRemoved.connect(sigc::mem_fun(*this, &ImageFrameSocketHandler::send_imageframe_time_axis_removed)) ;
			ifta->NameChanged.connect(sigc::bind(sigc::mem_fun(*this, &ImageFrameSocketHandler::send_imageframe_time_axis_renamed), ifta)) ;
			
			send_return_success() ;
		}
		else
		{
			std::string msg("Addition Failed: ") ;
			msg.append(track_name) ; 
			send_return_failure(msg) ;
		}
	}
}


/**
 * Handle the insertion of a new MarkerTimeAxis
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_insert_marker_time_axis(const char* msg)
{
	int position = 4 ; // message type chars
	
	// get the ImageFrameTrack name size
	int track_name_size = atoi(get_message_part(position, ardourvis::TEXT_SIZE_CHARS, msg).c_str()) ;
	position += ardourvis::TEXT_SIZE_CHARS ;
	
	// get the image frame track name
	std::string track_name = get_message_part(position, track_name_size, msg) ;
	position += track_name_size ;
	
	// get the size of the name of the associated track
	int assoc_track_name_size = atoi(get_message_part(position, ardourvis::TEXT_SIZE_CHARS, msg).c_str()) ;
	position += ardourvis::TEXT_SIZE_CHARS ;
	
	// get the name of the track we associate the marker track with	
	std::string assoc_track_name = get_message_part(position, assoc_track_name_size, msg) ;
	position += assoc_track_name_size ;

	// check that we dont already have a time axis with that name
	TimeAxisView* checkTav = thePublicEditor.get_named_time_axis(track_name) ;
	if(checkTav)
	{
		std::string errmsg("Track already exists: ") ;
		errmsg.append(track_name) ;
		send_return_failure(errmsg) ;
	}
	else
	{
		// check the associated time axis exists
		TimeAxisView* assoc_tav = thePublicEditor.get_named_time_axis(assoc_track_name) ;
		if(assoc_tav)
		{
			thePublicEditor.add_imageframe_marker_time_axis(track_name, assoc_tav, this) ;
			TimeAxisView* new_tav = thePublicEditor.get_named_time_axis(track_name) ;
			
			bool added = false ;
			
			if(new_tav)
			{
				MarkerTimeAxis* mta = dynamic_cast<MarkerTimeAxis*>(new_tav) ;
				if(mta)
				{
					added = true ;
					mta->VisualTimeAxisRemoved.connect(sigc::mem_fun(*this, &ImageFrameSocketHandler::send_marker_time_axis_removed)) ;
					mta->NameChanged.connect(sigc::bind(sigc::mem_fun(*this, &ImageFrameSocketHandler::send_marker_time_axis_renamed), mta)) ;
				}
			}
			
			if(added)
			{
				std::string msg("Addition Failed: ") ;
				msg.append(track_name) ; 
				send_return_failure(msg) ;
			}
		}
		else
		{
			std::string errmsg("No associated Track Found: ") ;
			errmsg.append(track_name) ;
			send_return_failure(errmsg) ;
		}
	}
}

/**
 * Handle the insertion of a time axis group (a scene)
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_insert_imageframe_group(const char* msg)
{
	int position = 4 ; // message type chars
	
	// get the ImageFrameTrack name size
	int track_name_size = atoi(get_message_part(position, ardourvis::TEXT_SIZE_CHARS, msg).c_str()) ;
	position += ardourvis::TEXT_SIZE_CHARS ;
	
	// get the image frame track name
	std::string track_name = get_message_part(position, track_name_size, msg) ;
	position += track_name_size ;
	
	// get the scene id size
	int scene_id_size = atoi(get_message_part(position, ardourvis::TEXT_SIZE_CHARS, msg).c_str()) ;
	position += ardourvis::TEXT_SIZE_CHARS ;
	
	// get the scene id
	std::string scene_id = get_message_part(position, scene_id_size, msg) ;
	position += scene_id_size ;
	
	
	// get the named ImageFrameTrack
	ImageFrameTimeAxis* ifta = dynamic_cast<ImageFrameTimeAxis*>(thePublicEditor.get_named_time_axis(track_name)) ;
	
	// check we got a valid ImageFrameTimeAxis
	if(!ifta)
	{
		send_return_failure(std::string("No Image Frame Time Axis Found: ").append(track_name)) ;
		return ;
	}
	
	ImageFrameTimeAxisGroup* iftag = ifta->get_view()->add_imageframe_group(scene_id, this) ;
	if(!iftag)
	{
		send_return_failure(std::string("Image Frame Group insert failed")) ;
	}
	else
	{
		iftag->NameChanged.connect(sigc::bind(sigc::mem_fun(*this, &ImageFrameSocketHandler::send_imageframe_time_axis_group_renamed), iftag)) ;
		iftag->GroupRemoved.connect(sigc::bind(sigc::mem_fun(*this, &ImageFrameSocketHandler::send_imageframe_time_axis_group_removed), iftag)) ;
		send_return_success() ;
	}
}


/**
 * Handle the insertion of a new ImageFrameItem
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_insert_imageframe_view(const char* msg)
{
	int position = 4 ; // message type chars
	
	// get the ImageFrameTrack name size
	int track_name_size = atoi(get_message_part(position,3,msg).c_str()) ;
	position += 3 ;
	
	// get the ImageFrameTrack Name
	std::string imageframe_track_name = get_message_part(position,track_name_size,msg) ;
	position += track_name_size ;
	
	// get the scene name size
	int scene_size = atoi(get_message_part(position,3,msg).c_str()) ;
	position += 3 ;
	
	// get the scene Name
	std::string scene_name = get_message_part(position,scene_size,msg) ;
	position += scene_size ;
	
	// get the image frame_id size
	int image_id_size = atoi(get_message_part(position,3,msg).c_str()) ;
	position += 3 ;
	
	// get the image frame_id
	std::string image_id = get_message_part(position,image_id_size,msg) ;
	position += image_id_size ;
	
	// get the start frame value
	jack_nframes_t start = strtoul((get_message_part(position,10,msg).c_str()),0,10) ;
	position += 10 ;
	
	// get the duration value
	jack_nframes_t duration = strtoul((get_message_part(position,10,msg).c_str()),0,10) ;
	position += 10 ;

	//get the named time axis view we about to add an image to
	TimeAxisView* tav = thePublicEditor.get_named_time_axis(imageframe_track_name) ;
	ImageFrameTimeAxis* ifta = 0 ;
	
	if(tav)
	{
		ifta = dynamic_cast<ImageFrameTimeAxis*>(tav) ;
	}
	
	if(!ifta)
	{
		std::string errmsg("No Parent Image Track Found: ") ;
		errmsg.append(imageframe_track_name) ;
		send_return_failure(errmsg) ;
		
		// dont really like all these returns mid-way
		// but this is goinf to get awfully if..then nested if not
		return ;
	}
	
	// check the parent group exists
	ImageFrameTimeAxisGroup* iftag = ifta->get_view()->get_named_imageframe_group(scene_name) ;
	if(!iftag)
	{
		std::string errmsg("No Image Frame Group Found: ") ;
		errmsg.append(scene_name) ;
		send_return_failure(errmsg) ;
		return ;
	}
	
	// ok, so we have the parent group and track, now we need dome image data
	
	
	//
	// request the image data from the image compositor
	//
	
	// ctreate a message buffer
	std::ostringstream reqBuffer ;
	reqBuffer << std::setfill('0') ;
	
	// add the msg type
	reqBuffer << REQUEST_DATA << IMAGE_RGB_DATA ;
	
	// add the image track and size
	reqBuffer << std::setw(ardourvis::TEXT_SIZE_CHARS) << track_name_size ;
	reqBuffer << imageframe_track_name ;
	
	// add the scene id and size
	reqBuffer << std::setw(ardourvis::TEXT_SIZE_CHARS) << scene_size ;
	reqBuffer << scene_name ;
	
	// add the image id and size
	reqBuffer << std::setw(ardourvis::TEXT_SIZE_CHARS) << image_id_size ;
	reqBuffer << image_id ;
	
	// add the preferred image height
	reqBuffer << std::setw(ardourvis::TEXT_SIZE_CHARS) << ifta->get_image_display_height() ;
	
	// send the request message
	send_message(reqBuffer.str()) ;


	// read the reply, the inital image data message
	// this gives us the image dimensions and the expected size of the image data
	// msg type(4) + image width(3) + height(3) + num channels(3) + size of the image data (32)
	std::string init_image_data_msg ;
	read_message(init_image_data_msg) ;
	int init_msg_pos = 4 ;
	
	int imgWidth    = atoi(init_image_data_msg.substr(init_msg_pos, ardourvis::IMAGE_SIZE_CHARS).c_str()) ;
	init_msg_pos += ardourvis::IMAGE_SIZE_CHARS ;
	int imgHeight    = atoi(init_image_data_msg.substr(init_msg_pos, ardourvis::IMAGE_SIZE_CHARS).c_str()) ;
	init_msg_pos += ardourvis::IMAGE_SIZE_CHARS ;
	int imgChannels    = atoi(init_image_data_msg.substr(init_msg_pos, ardourvis::IMAGE_SIZE_CHARS).c_str()) ;
	init_msg_pos += ardourvis::IMAGE_SIZE_CHARS ; 
	int imgSize = atoi(init_image_data_msg.substr(init_msg_pos, ardourvis::IMAGE_DATA_MESSAGE_SIZE_CHARS).c_str()) ;

	// send a success msg
	// we need to do this to keep things moving
	send_return_success() ;

	// create our image rgb buffer, this holds the image data we receive
	unsigned char* rgb_img_buf = new unsigned char[imgSize] ;

	int retcode = ::recv(theArdourToCompositorSocket, rgb_img_buf, imgSize, MSG_WAITALL) ;

	if(retcode != imgSize)
	{
		delete [] rgb_img_buf ;
		send_return_failure("Could not create new Image Frame View : image data sizes did not match") ;
	}
	else
	{
		ImageFrameView* ifv = iftag->add_imageframe_item(image_id, start, duration, rgb_img_buf, (uint32_t)imgWidth, (uint32_t)imgHeight, (uint32_t)imgChannels, this) ;
		if(ifv)
		{
			ifv->PositionChanged.connect(sigc::bind(sigc::mem_fun(*this, &ImageFrameSocketHandler::send_imageframe_view_position_change), ifv)) ;
			ifv->DurationChanged.connect(sigc::bind(sigc::mem_fun(*this, &ImageFrameSocketHandler::send_imageframe_view_duration_change), ifv)) ;
			ifv->ItemRemoved.connect(sigc::bind(sigc::mem_fun(*this, &ImageFrameSocketHandler::send_imageframe_view_removed), ifv)) ;
		
			send_return_success() ;
		}
		else
		{
			//addition failed. assume duplicate item_id
			send_return_failure("Could not create new Image Frame View") ;
		}
	}
}



/**
 * Handle the insertion of a new MarkerItem
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_insert_marker_view(const char* msg)
{}
	
	
//---------------------------------------------------------------------------------------//
// handlers for specific removal procedures


/**
 * Handle the removal of an ImageTimeAxis
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_remove_imageframe_time_axis(const char* msg)
{}
		
/**
 * Handle the removal of an MarkerTimeAxis
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_remove_marker_time_axis(const char* msg)
{}
		
/**
 * Handle the removal of an ImageFrameTimeAxisGroup
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_remove_imageframe_time_axis_group(const char* msg)
{}
		
/**
 * Handle the removal of an ImageFrameItem
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_remove_imageframe_view(const char* msg)
{}
		
/**
 * Handle the removal of an MarkerItem
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_remove_marker_view(const char* msg)
{}


	
	

//---------------------------------------------------------------------------------------//
// handlers for the specific rename procedures	
	
/**
 * Handle the renaming of an ImageTimeAxis
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_rename_imageframe_time_axis(const char* msg)
{
	// msg [MVIT][oldSize][oldId][newSize][newId]
	
	int position = 4 ; // message type chars

	// get the old Id size
	int old_id_size = atoi(get_message_part(position,3,msg).c_str()) ;
	position += 3 ;
	
	// get the old id
	std::string old_id = get_message_part(position,old_id_size,msg) ;
	position += old_id_size ;
	
	//get the new Id size
	int new_id_size = atoi(get_message_part(position,3,msg).c_str()) ;
	position += 3 ;
	
	// get the new Id
	std::string new_id = get_message_part(position,new_id_size,msg) ;
	position += new_id_size ;
	
	// get the Named time axis
	TimeAxisView* tav = thePublicEditor.get_named_time_axis(old_id) ;
	if(dynamic_cast<ImageFrameTimeAxis*>(tav))
	{
		ImageFrameTimeAxis* ifta = dynamic_cast<ImageFrameTimeAxis*>(tav) ;
		ifta->set_time_axis_name(new_id, this) ;
		send_return_success() ;
	}
	else
	{
		std::string msg = "No Image Track Found: " ;
		msg.append(old_id) ;
		send_return_failure(msg) ;
	}
}
		
/**
 * Handle the renaming of an MarkerTimeAxis
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_rename_marker_time_axis(const char* msg)
{}
		
/**
 * Handle the renaming of an ImageFrameItem
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_rename_imageframe_time_axis_group(const char* msg)
{}
	
/**
 * Handle the renaming of an ImageFrameItem
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_rename_imageframe_view(const char* msg)
{}
		
/**
 * Handle the renaming of an Marker
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_rename_marker_view(const char* msg)
{}
	
	

	
//---------------------------------------------------------------------------------------//
// handlers for data request
	
/**
 * Handle a request for the sessnio naem fo the current session
 * We return a failure state if no session is open
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_session_name_request(const char* msg)
{
	ARDOUR::Session* currentSession = thePublicEditor.current_session() ;
	
	if(currentSession == 0)
	{
		// no current session, return failure
		std::string msg("No Current Session") ;
		send_return_failure(msg) ;
	}
	else
	{
		std::string sessionName = currentSession->name() ;
		std::string sessionPath = currentSession->path() ;
		
		if(sessionPath[sessionPath.length() -1] != '/')
		{
			sessionPath.append("/") ;
		}
		
		sessionPath.append(sessionName) ;
		
		std::ostringstream msgBuf ;
		msgBuf << ardourvis::RETURN_DATA << ardourvis::SESSION_NAME ;
		msgBuf << std::setfill('0') ;
		msgBuf << std::setw(ardourvis::TEXT_SIZE_CHARS) << sessionPath.length() ;
		msgBuf << sessionPath ;
		send_message(msgBuf.str()) ;
	}
}
	
	



//---------------------------------------------------------------------------------------//
// handlers for specific item update changes
	
/**
 * Handle ImageFrameView positional changes
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_imageframe_view_position_update(const char* msg)
{
	int position = 6 ; // message type chars
	
	std::string track_id ;
	std::string scene_id ;
	std::string item_id ;
	int track_id_size ;
	int scene_id_size ;
	int item_id_size ;
	
	this->decompose_imageframe_item_desc(msg, position, track_id, track_id_size, scene_id, scene_id_size, item_id, item_id_size) ;
	
	jack_nframes_t start_frame = strtoul(get_message_part(position, ardourvis::TIME_VALUE_CHARS, msg).c_str(), 0, 10) ;
	position += ardourvis::TIME_VALUE_CHARS ;
	
	// get the named time axis
	ImageFrameTimeAxis* ifta = dynamic_cast<ImageFrameTimeAxis*>(thePublicEditor.get_named_time_axis(track_id)) ;
	
	if(!ifta)
	{
		send_return_failure(std::string("No parent Image Track found: ").append(track_id)) ;
		return ;
	}
	
	// get the parent scene
	ImageFrameTimeAxisGroup* iftag = ifta->get_view()->get_named_imageframe_group(scene_id) ;
	if(!iftag)
	{
		send_return_failure(std::string("No parent Scene found: ").append(scene_id)) ;
		return ;
	}
	
	ImageFrameView* ifv = iftag->get_named_imageframe_item(item_id) ;
	
	if(!ifv)
	{
		send_return_failure(std::string("No Image Frame Item found: ").append(item_id)) ;
		return ;
	}
	

	ifv->set_position(start_frame, this) ;
	send_return_success() ;
}
		
/**
 * Handle ImageFrameView Duration changes
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_imageframe_view_duration_update(const char* msg)
{
	int position = 6 ; // message type chars
	
	std::string track_id ;
	std::string scene_id ;
	std::string item_id ;
	int track_id_size ;
	int scene_id_size ;
	int item_id_size ;
	
	this->decompose_imageframe_item_desc(msg, position, track_id, track_id_size, scene_id, scene_id_size, item_id, item_id_size) ;
	
	jack_nframes_t duration = strtoul(get_message_part(position,ardourvis::TIME_VALUE_CHARS,msg).c_str(),0,10) ;
	position += ardourvis::TIME_VALUE_CHARS ;
	
	// get the named time axis
	ImageFrameTimeAxis* ifta = dynamic_cast<ImageFrameTimeAxis*>(thePublicEditor.get_named_time_axis(track_id)) ;
	
	if(!ifta)
	{
		send_return_failure(std::string("No parent Image Track found : ").append(track_id)) ;
		return ;
	}
	
	// get the parent scene
	ImageFrameTimeAxisGroup* iftag = ifta->get_view()->get_named_imageframe_group(scene_id) ;
	if(!iftag)
	{
		send_return_failure(std::string("No parent Scene found : ").append(scene_id)) ;
		return ;
	}
	
	ImageFrameView* ifv = iftag->get_named_imageframe_item(item_id) ;
	
	if(!ifv)
	{
		send_return_failure(std::string("No Image Frame Item found : ").append(item_id)) ;
		return ;
	}
	
	ifv->set_duration(duration, this) ;
	send_return_success() ;
}

/**
 * Handle ImageFrameView Position Lock Constraint changes
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_imageframe_position_lock_update(const char* msg)
{
	int position = 6 ; // message type chars
	
	std::string track_id ;
	std::string group_id ;
	std::string item_id ;
	int track_id_size ;
	int group_id_size ;
	int item_id_size ;
	
	this->decompose_imageframe_item_desc(msg, position, track_id, track_id_size, group_id, group_id_size, item_id, item_id_size) ;
	
	std::string pos_lock = get_message_part(position,1,msg) ;
	bool pos_lock_active = false ;
	
	if(pos_lock == "0")
	{
		pos_lock_active = false ;
	}
	else if(pos_lock == "1")
	{
		pos_lock_active = true ;
	}
	else
	{
		send_return_failure(std::string("Unknown Value used during Position Loack: ").append(pos_lock)) ;
		return ;
	}
	
	position += 1 ;
	
	int errcode ;
	std::string errmsg ;
	ImageFrameView* ifv = get_imageframe_view_from_desc(track_id, group_id, item_id, errcode, errmsg) ;
	if(ifv)
	{
		ifv->set_position_locked(pos_lock_active, this) ;
		send_return_success() ;
	}
	else
	{
		send_return_failure(errmsg) ;
	}
}
		
/**
 * Handle ImageFrameView Maximum Duration changes
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_imageframe_view_max_duration_update(const char* msg)
{
	int position = 6 ; // message type chars
	
	std::string track_id ;
	std::string group_id ;
	std::string item_id ;
	int track_id_size ;
	int group_id_size ;
	int item_id_size ;
	
	this->decompose_imageframe_item_desc(msg, position, track_id, track_id_size, group_id, group_id_size, item_id, item_id_size) ;
	
	jack_nframes_t max_duration = strtoul(get_message_part(position,ardourvis::TIME_VALUE_CHARS,msg).c_str(),0,10) ;
	position += ardourvis::TIME_VALUE_CHARS ;
	
	int errcode ;
	std::string errmsg ;
	ImageFrameView* ifv = get_imageframe_view_from_desc(track_id, group_id, item_id, errcode, errmsg) ;
	if(ifv)
	{
		ifv->set_max_duration(max_duration, this) ;
		send_return_success() ;
	}
	else
	{
		send_return_failure(errmsg) ;
	}
}
	
/**
 * Handle image frame max duration enable constraint changes
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_imageframe_view_max_duration_enable_update(const char* msg)
{
	int position = 6 ; // message type chars
	
	std::string track_id ;
	std::string group_id ;
	std::string item_id ;
	int track_id_size ;
	int group_id_size ;
	int item_id_size ;
	
	this->decompose_imageframe_item_desc(msg, position, track_id, track_id_size, group_id, group_id_size, item_id, item_id_size) ;
	
	std::string active = get_message_part(position,1,msg) ;
	bool max_duration_active = false ;
	
	if(active == "0")
	{
		max_duration_active = false ;
	}
	else if(active == "1")
	{
		max_duration_active = true ;
	}
	else
	{
		send_return_failure(std::string("Unknown Value used during enable max duration: ").append(active)) ;
		return ;
	}
	
	position += 1 ;
	
	int errcode ;
	std::string errmsg ;
	ImageFrameView* ifv = get_imageframe_view_from_desc(track_id, group_id, item_id, errcode, errmsg) ;
	if(ifv)
	{
		ifv->set_max_duration_active(max_duration_active, this) ;
		send_return_success() ;
	}
	else
	{
		send_return_failure(errmsg) ;
	}
}
		
/**
 * Handle ImageFrameView Minimum Duration changes
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_imageframe_view_min_duration_update(const char* msg)
{
	int position = 6 ; // message type chars
	
	std::string track_id ;
	std::string group_id ;
	std::string item_id ;
	int track_id_size ;
	int group_id_size ;
	int item_id_size ;
	
	this->decompose_imageframe_item_desc(msg, position, track_id, track_id_size, group_id, group_id_size, item_id, item_id_size) ;
	
	jack_nframes_t min_duration = strtoul(get_message_part(position,ardourvis::TIME_VALUE_CHARS,msg).c_str(),0,10) ;
	position += ardourvis::TIME_VALUE_CHARS ;
	
	int errcode ;
	std::string errmsg ;
	ImageFrameView* ifv = get_imageframe_view_from_desc(track_id, group_id, item_id, errcode, errmsg) ;
	if(ifv)
	{
		ifv->set_min_duration(min_duration, this) ;
		send_return_success() ;
	}
	else
	{
		send_return_failure(errmsg) ;
	}
}
	
/**
 * Handle image frame min duration enable constraint changes
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_imageframe_view_min_duration_enable_update(const char* msg)
{
	int position = 6 ; // message type chars
	
	std::string track_id ;
	std::string group_id ;
	std::string item_id ;
	int track_id_size ;
	int group_id_size ;
	int item_id_size ;
	
	this->decompose_imageframe_item_desc(msg, position, track_id, track_id_size, group_id, group_id_size, item_id, item_id_size) ;
	
	std::string active = get_message_part(position,1,msg) ;
	bool min_duration_active = false ;
	
	if(active == "0")
	{
		min_duration_active = false ;
	}
	else if(active == "1")
	{
		min_duration_active = true ;
	}
	else
	{
		send_return_failure(std::string("Unknown Value used during enable max duration: ").append(active)) ;
		return ;
	}
	
	position += 1 ;
	
	int errcode ;
	std::string errmsg ;
	ImageFrameView* ifv = get_imageframe_view_from_desc(track_id, group_id, item_id, errcode, errmsg) ;
	if(ifv)
	{
		ifv->set_min_duration_active(min_duration_active, this) ;
		send_return_success() ;
	}
	else
	{
		send_return_failure(errmsg) ;
	}
}
	
/**
 * Handle MarkerView position changes
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_marker_view_position_update(const char* msg)
{}
		
/**
 * Handle MarkerView duration changes
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_marker_view_duration_update(const char* msg)
{}

/**
 * Handle MarkerView Position Lock Constraint changes
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_marker_view_position_lock_update(const char* msg)
{
}
		
/**
 * Handle MarkerView maximum duration changes
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_marker_view_max_duration_update(const char* msg)
{}
		
/**
 * Handle MarkerView minimum duration changes
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_marker_view_min_duration_update(const char* msg)
{}





//---------------------------------------------------------------------------------------//
// handlers for Session Actions
	
/**
 * Handle the opening of a named audio session
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_open_session(const char* msg)
{
	// msg [SAOS][sessionSize][sessionPath]
	
	int position = 4 ; // message type chars

	// get the session name size
	int session_name_size = atoi(get_message_part(position,3,msg).c_str()) ;
	position += 3 ;
	
	// get the session name
	std::string session_name = get_message_part(position,session_name_size,msg) ;
	position += session_name_size ;
	
	
	// open the session	
	std::string path, name ;
	bool isnew;

	if (ARDOUR::Session::find_session(session_name, path, name, isnew) == 0) {
		if (ARDOUR_UI::instance()->load_session (path, name) == 0) {
			send_return_success() ;
		} else {
			std::string retMsg = "Failed to load Session" ;
			send_return_failure(retMsg) ;
		}
	} else {
		std::string retMsg = "Failed to find Session" ;
		send_return_failure(retMsg) ;
	}
}

		
/**
 * Handle the closing of a named audio session
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_closed_session(const char* msg)
{}
	
//---------------------------------------------------------------------------------------//
// handlers for the shutdown of the Image Compositor
		
/**
 * Handle the shutdown message from the image compositor
 *
 * @param msg the received message
 */
void
ImageFrameSocketHandler::handle_shutdown(const char* msg)
{
	 CompositorSocketShutdown() ; /* EMIT_SIGNAL */
}








	
	
//---------------------------------------------------------------------------------------//
// convenince methods to break up messages
	
/**
 * Returns part of the received message as a std::string
 *
 * @param start the start character
 * @param num_chars the number of characters to read
 * @param the message to break apart
 * @return the sub string of the message
		 */
std::string
ImageFrameSocketHandler::get_message_part(int start, int32_t num_chars, const char* msg)
{
	char buf[num_chars + 1] ;
	strncpy(buf,msg+start,num_chars) ;
	buf[num_chars] = '\0' ;
	std::string s(buf) ;
	
	return(s) ;
}



/**
 * break up am image item description message
 * we break the mesage up into the parent Image Track id and size,
 * the parent group id and size, and the image id and size
 *
 * @param track_id
 * @param track_id_size
 * @param scene_id
 * @param scene_id_size
 * @param item_id
 * @param item_id_size
 */
void
ImageFrameSocketHandler::decompose_imageframe_item_desc(const char* msg, int& position, std::string& track_id,
	int& track_id_size, std::string& scene_id, int& scene_id_size, std::string& item_id, int& item_id_size)
{
	// get the track Id size
	track_id_size = atoi(get_message_part(position,ardourvis::TEXT_SIZE_CHARS,msg).c_str()) ;
	position += ardourvis::TEXT_SIZE_CHARS ;
	
	// get the track id
	track_id = get_message_part(position,track_id_size,msg) ;
	position += track_id_size ;
	
	// get the track Id size
	scene_id_size = atoi(get_message_part(position,ardourvis::TEXT_SIZE_CHARS,msg).c_str()) ;
	position += ardourvis::TEXT_SIZE_CHARS ;
	
	// get the scene id
	scene_id = get_message_part(position,scene_id_size,msg) ;
	position += scene_id_size ;
	
	// get the item id size
	item_id_size = atoi(get_message_part(position,ardourvis::TEXT_SIZE_CHARS,msg).c_str()) ;
	position += ardourvis::TEXT_SIZE_CHARS ;
	
	// get the item id
	item_id = get_message_part(position,item_id_size,msg) ;
	position += item_id_size ;
}

/**
 * Compose a description of the specified image frame view
 * The description consists of the parent track name size and name,
 * the parent group name size and name, and the item name size and name
 *
 * @param ifv the item to string_compose a description of
 * @param buffer the buffer to write the description
 */
void
ImageFrameSocketHandler::compose_imageframe_item_desc(ImageFrameView* ifv, std::ostringstream& buffer)
{
	buffer << std::setw(3) << ifv->get_time_axis_group()->get_view().trackview().name().length() ;
	buffer << ifv->get_time_axis_group()->get_view().trackview().name() ;
	
	// add the parent scene
	buffer << std::setw(3) << ifv->get_time_axis_group()->get_group_name().length() ;
	buffer << ifv->get_time_axis_group()->get_group_name() ;
	
	// add the ImageFrameItem id length and Id
	buffer << setw(3)  << ifv->get_item_name().length() ;
	buffer << ifv->get_item_name() ;
}

/**
 * Compose a description of the specified marker view
 * The description consists of the parent track name size and name,
 * and the item name size and name
 *
 * @param mv the item to string_compose a description of
 * @param buffer the buffer to write the description
 */
void
ImageFrameSocketHandler::compose_marker_item_desc(MarkerView* mv, std::ostringstream& buffer)
{
	MarkerTimeAxis* mta = dynamic_cast<MarkerTimeAxis*>(&mv->get_time_axis_view()) ;
	
	if(!mta)
	{
		return ;
	}
	
	buffer << std::setw(3) << mta->name().length() ;
	buffer << mta->name() ;
	
	buffer << std::setw(3) << mv->get_item_name().length() ;
	buffer << mv->get_item_name() ;	
}


/**
 * Returns the ImageFrameView from the specified description
 * The errcode parameter is used to indicate the item which caused
 * an error on failure of this method
 * 0 = success
 * 1 = the track item was not found
 * 2 = the group item was not found
 * 3 = the imageframe item was not found
 *
 * @paran track_id the track on which the item is placed
 * @param group_id the group in which the item is a member
 * @param item_id the id of the item
 * @param int32_t reference used for error codes on failure
 * @param errmsg populated with a description of the error on failure
 * @return the described item on success, 0 otherwise
 */
ImageFrameView*
ImageFrameSocketHandler::get_imageframe_view_from_desc(const string & track_id, const string & group_id, const string & item_id, int& errcode, std::string& errmsg)
{
	ImageFrameView* item = 0 ;
	
	// get the named time axis
	ImageFrameTimeAxis* ifta = dynamic_cast<ImageFrameTimeAxis*>(thePublicEditor.get_named_time_axis(track_id)) ;
	
	if(!ifta)
	{
		errcode = 1 ;
		errmsg = std::string("Image Frame Time Axis Not Found: ").append(track_id) ;
	}
	else
	{
		// get the parent scene
		ImageFrameTimeAxisGroup* iftag = ifta->get_view()->get_named_imageframe_group(group_id) ;
		if(!iftag)
		{
			errcode = 2 ;
			errmsg = std::string("Image Frame Group Not Found: ").append(group_id) ;
		}
		else
		{
			ImageFrameView* ifv = iftag->get_named_imageframe_item(item_id) ;
			if(!ifv)
			{
				errcode = 3 ;
				errmsg = std::string("Image Frame Item Not Found: ").append(item_id) ;
			}
			else
			{
				// yay!!
				item = ifv ;
				errcode = 0 ;
			}
		}
	}
	
	return(item) ;
}

//---------------------------------------------------------------------------------------//
// Convenince Message Send Methods

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/**
 * Sends a message throught the socket
 *
 * @param msg the message to send
 * @return the return value of the socket call
 */
int
ImageFrameSocketHandler::send_message(const string & msg)
{
	//std::cout << "Sending Message [" << msg << "]\n" ;
	int retcode = ::send(theArdourToCompositorSocket, msg.c_str(), msg.length(), MSG_NOSIGNAL) ;
	
	return(retcode) ;
}
		
/**
 * Reads a message from the Socket
 *
 * @param msg a string to populate with the received message
 * @return the return value from the socket call
 */
int
ImageFrameSocketHandler::read_message(std::string& msg)
{
	char buf[ardourvis::MAX_MSG_SIZE + 1] ;
	memset(buf, 0, (ardourvis::MAX_MSG_SIZE + 1)) ;
	
	msg = "" ;
	int retcode = ::recv(theArdourToCompositorSocket, buf, ardourvis::MAX_MSG_SIZE, 0) ;
	
	msg = buf ;
	//std::cout << "Received Message [" << msg << "]\n" ;
	
	return(retcode) ;
}


/**
 * Convenience method to string_compose and send a success messasge back to the Image Compositor
 *
 */
void
ImageFrameSocketHandler::send_return_success()
{
	send_message(ardourvis::RETURN_TRUE) ;
}

/**
 * Convenience method to string_compose and send a failure messasge back to the Image Compositor
 *
 * @param msg the failure message
 */
void
ImageFrameSocketHandler::send_return_failure(const std::string& msg)
{
	std::ostringstream buf ;
	buf << std::setfill('0') ;
	buf << ardourvis::RETURN_FALSE ;
	buf << std::setw(3) << msg.length(); ;
	buf << msg ;
	
	send_message(buf.str()) ;
}
