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

#ifndef __ardour_gtk_imageframe_socket_handler_h__
#define __ardour_gtk_imageframe_socket_handler_h__

#include <string>
#include <gtk--.h>
#include <list>
#include "editor.h"
#include "ardour_image_compositor_socket.h"

class TimeAxisViewItem ;
class ImageFrameView ;
class MarkerView ;
class ImageFrameTimeAxisGroup ;

/**
 * ImageFrameSocketHandler defines the handler between Ardour and an Image Compositor
 * As this is purely visual, we do all processing within the main gtk loop via
 * message passing through a socket.
 *
 */
class ImageFrameSocketHandler : public SigC::Object
{
	public:
		/**
		 * Constructs a new ImageFrameSocketHandler to handle communication between Ardour and the Image Compositor
		 *
		 * @param ed the PublicEditor
		 */
		ImageFrameSocketHandler(PublicEditor& ed) ;
		
		/**
		 * Descructor
		 * this will shutdown the socket if open
		 */
		virtual ~ImageFrameSocketHandler() ;

		/**
		 * Returns the instance of the ImageFrameSocketHandler
		 * the instance should first be created with createInstance
		 *
		 * @return the instance of the ImageFrameSocketHandler
		 */
		static ImageFrameSocketHandler* get_instance() ;
		
		/**
		 * call back to handle doing the processing work
		 * This method is added to the gdk main loop and called when there is data
		 * upon the socket.
		 *
		 */
		static void image_socket_callback(void *arg, int32_t fd, GdkInputCondition cond) ;
		
		/**
		 * Attempt to connect to the image compositor on the specified host and port
		 *
		 * @param hostIp the ip address of the image compositor host
		 * @param port the oprt number to attemp the connection on
		 * @return true if the connection was a succees
		 *         false otherwise
		 */
		bool connect(std::string hostIp, int32_t port) ;
		
		/**
		 * Closes the connection to th Image Compositor
		 *
		 */
		 void close_connection() ;		
		/**
		 * Returns true if this ImagFrameSocketHandler is currently connected to rthe image compositor
		 *
		 * @return true if connected to the image compositor
		 */
		bool is_connected() ;
		
		/**
		 * Sets the tag used to describe this input within gtk
		 * this is returned when gdk_input_add is called and is required to remove the input
		 *
		 * @param tag the gdk input tag of this input
		 */
		void set_gdk_input_tag(int tag) ;
		
		/**
		 * Returns the gdk input tag of this input
		 *
		 * @return the gdk input tag of this input
		 * @see setGdkInputTag
		 */
		int get_gdk_input_tag() ;
		
		
		/**
		 * Returns the socket file descriptor
		 *
		 * @return the Sockt file descriptor
		 */
		int get_socket_descriptor() ;
		
		
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
		void send_imageframe_time_axis_removed(std::string track_id, void* src) ;
		
		/**
		 * Sends a message indicating that an ImageFrameTimeAxis has been renamed
		 *
		 * @param new_id the new name, or Id, of the track
		 * @param old_id the old name, or Id, of the track
		 * @param src the identity of the object that initiated the change
		 * @param time_axis the time axis that has changed
		 */
		void send_imageframe_time_axis_renamed(std::string new_id, std::string old_id, void* src, ImageFrameTimeAxis* time_axis) ;
		
		//------------------------
		// MarkerTimeAxis Messages
		
		/**
		 * Sends a message stating that the named marker time axis has been removed
		 *
		 * @param track_id the unique id of the removed image frame time axis
		 * @param src the identity of the object that initiated the change
		 */
		void send_marker_time_axis_removed(std::string track_id, void* src) ;
		
		/**
		 * Sends a message indicating that an MarkerTimeAxis has been renamed
		 *
		 * @param new_id the new name, or Id, of the track
		 * @param old_id the old name, or Id, of the track
		 * @param src the identity of the object that initiated the change
		 * @param time_axis the time axis that has changed
		 */
		void send_marker_time_axis_renamed(std::string new_id, std::string old_id, void* src, MarkerTimeAxis* time_axis) ;
		
		
		//---------------------------------
		// ImageFrameTimeAxisGroup Messages
		
		/**
		 * Sends a message stating that the group has been removed
		 *
		 * @param group_id the unique id of the removed image frame time axis
		 * @param src the identity of the object that initiated the change
		 * @param group the group that has changed
		 */
		void send_imageframe_time_axis_group_removed(std::string group_id, void* src, ImageFrameTimeAxisGroup* group) ;
		
		/**
		 * Send a message indicating that an ImageFrameTimeAxisGroup has been renamed
		 *
		 * @param new_id the new name, or Id, of the group
		 * @param old_id the old name, or Id, of the group
		 * @param src the identity of the object that initiated the change
		 * @param group the group that has changed
		 */
		void send_imageframe_time_axis_group_renamed(std::string new_id, std::string old_id, void* src, ImageFrameTimeAxisGroup* group) ;


		//---------------------------------
		// ImageFrameView Messages
		
		/**
		 * Send an Image Frame View Item position changed message
		 *
		 * @param pos the new position value
		 * @param src the identity of the object that initiated the change
		 * @param item the time axis item whos position has changed
		 */
		void send_imageframe_view_position_change(jack_nframes_t pos, void* src, ImageFrameView* item) ;
		
		/**
		 * Send a Image Frame View item duration changed message
		 *
		 * @param dur the the new duration value
		 * @param src the identity of the object that initiated the change
		 * @param item the item which has had a duration change
		 */
		void send_imageframe_view_duration_change(jack_nframes_t dur, void* src, ImageFrameView* item) ;
		
		/**
		 * Send a message indicating that an ImageFrameView has been renamed
		 *
		 * @param item the ImageFrameView which has been renamed
		 * @param src the identity of the object that initiated the change
		 * @param item the renamed item
		 */
		void send_imageframe_view_renamed(std::string new_id, std::string old_id, void* src, ImageFrameView* item) ;
		
		/**
		 * Send a message indicating that an ImageFrameView item has been removed message
		 *
		 * @param item_id the id of the item that was removed
		 * @param src the identity of the object that initiated the change
		 * @param item the removed item
		 */
		void send_imageframe_view_removed(std::string item_id, void* src, ImageFrameView* item) ;
		
		//---------------------------------
		// MarkerView Messages
		
		/**
		 * Send a Marker View Item position changed message
		 *
		 * @param pos the new position value
		 * @param src the identity of the object that initiated the change
		 * @param item the time axis item whos position has changed
		 */
		void send_marker_view_position_change(jack_nframes_t pos, void* src, MarkerView* item) ;
		
		/**
		 * Send a Marker View item duration changed message
		 *
		 * @param dur the new duration value
		 * @param src the identity of the object that initiated the change
		 * @param item the time axis item whos position has changed
		 */
		void send_marker_view_duration_change(jack_nframes_t dur, void* src, MarkerView* item) ;
		
		/**
		 * Send a message indicating that a MarkerView has been renamed
		 *
		 * @param new_id the new_id of the object
		 * @param old_id the old_id of the object
		 * @param src the identity of the object that initiated the change
		 * @param item the MarkerView which has been renamed
		 */
		void send_marker_view_renamed(std::string new_id, std::string old_id, void* src, MarkerView* item) ;
		
		/**
		 * Send a message indicating that a MarkerView  item has been removed message
		 *
		 * @param item_id the id of the item that was removed
		 * @param src the identity of the object that initiated the change
		 * @param item the MarkerView which has been removed
		 */
		void send_marker_view_removed(std::string item_id, void* src, MarkerView* item) ;

		
		//---------------------------------------------------------------------------------------//
		// Emitted Signals
		
		/** Emitted if the socket connection is shutdown at the other end */
		SigC::Signal0<void> CompositorSocketShutdown ;
		
		/** Emitted as a generic error is captured from the socket connection to the animatic compositor */
		SigC::Signal0<void> CompositorSocketError ;
		
		
	protected:
		
	
	private:
		/* I dont like friends :-( */
		friend class Editor;
			
		/**
		 * Create an new instance of the ImageFrameSocketHandler, if one does not already exist
		 *
		 * @param ed the Ardour PublicEditor
		 */
		static ImageFrameSocketHandler* create_instance(PublicEditor& ed) ;
	
		//---------------------------------------------------------------------------------------//
		// Message breakdown ie avoid a big if...then...else
	
		/**
	 	 * Handle insert item requests
		 *
		 * @param msg the received message
		 */
		void handle_insert_message(const char* msg) ;
	
		/**
		 * Handle remove item requests
		 *
		 * @param msg the received message
		 */
		void handle_remove_message(const char* msg) ;
	
		/**
		 * Handle rename item requests
		 *
		 * @param msg the received message
		 */
		void handle_rename_message(const char* msg) ;
		
		/**
	 	 * Handle a request for session information
	 	 *
	 	 * @param msg the received message
	 	 */
		void handle_request_data(const char* msg) ;
		
		/**
		 * Handle the update of a particular item
		 *
		 * @param msg the received message
		 */
		void handle_item_update_message(const char* msg) ;
		
		/**
		 * Handle the selection of an Item
		 *
		 * @param msg the received message
		 */
		void handle_item_selected(const char* msg) ;
		
		/**
		 * Handle s session action message
		 *
		 * @param msg the received message
		 */
		void handle_session_action(const char* msg) ;
	
		//---------------------------------------------------------------------------------------//
		// handlers for specific insert procedures
	
		/**
		 * Handle the insertion of a new ImaegFrameTimeAxis
		 *
		 * @param msg the received message
		 */
		void handle_insert_imageframe_time_axis(const char* msg) ;
		
		/**
		 * Handle the insertion of a new MarkerTimeAxis
		 *
		 * @param msg the received message
		 */
		void handle_insert_marker_time_axis(const char* msg) ;
		
		/**
		 * Handle the insertion of a time axis group (a scene)
		 *
		 * @param msg the received message
		 */
		void handle_insert_imageframe_group(const char* msg) ;
		
		/**
		 * Handle the insertion of a new ImageFrameItem
		 *
		 * @param msg the received message
		 */
		void handle_insert_imageframe_view(const char* msg) ;
		
		/**
		 * Handle the insertion of a new MarkerItem
		 *
		 * @param msg the received message
		 */
		void handle_insert_marker_view(const char* msg) ;
	
		//---------------------------------------------------------------------------------------//
		// handlers for specific removal procedures
	
		/**
		 * Handle the removal of an ImageTimeAxis
		 *
		 * @param msg the received message
		 */
		void handle_remove_imageframe_time_axis(const char* msg) ;
		
		/**
		 * Handle the removal of an MarkerTimeAxis
		 *
		 * @param msg the received message
		 */
		void handle_remove_marker_time_axis(const char* msg) ;
		
		/**
		 * Handle the removal of an ImageFrameTimeAxisGroup
		 *
		 * @param msg the received message
		 */
		void handle_remove_imageframe_time_axis_group(const char* msg) ;
		
		/**
		 * Handle the removal of an ImageFrameItem
		 *
		 * @param msg the received message
		 */
		void handle_remove_imageframe_view(const char* msg) ;
		
		/**
		 * Handle the removal of an MarkerItem
		 *
		 * @param msg the received message
		 */
		void handle_remove_marker_view(const char* msg) ;
	
		//---------------------------------------------------------------------------------------//
		// handlers for the specific rename procedures
	
		/**
		 * Handle the renaming of an ImageTimeAxis
		 *
		 * @param msg the received message
		 */
		void handle_rename_imageframe_time_axis(const char* msg) ;
		
		/**
		 * Handle the renaming of an MarkerTimeAxis
		 *
		 * @param msg the received message
		 */
		void handle_rename_marker_time_axis(const char* msg) ;
		
		/**
		 * Handle the renaming of an ImageFrameItem
		 *
		 * @param msg the received message
		 */
		void handle_rename_imageframe_time_axis_group(const char* msg) ;
		
		/**
		 * Handle the renaming of an ImageFrameItem
		 *
		 * @param msg the received message
		 */
		void handle_rename_imageframe_view(const char* msg) ;
		
		/**
		 * Handle the renaming of an Marker
		 *
		 * @param msg the received message
		 */
		void handle_rename_marker_view(const char* msg) ;
		
		//---------------------------------------------------------------------------------------//
		// handlers for data request
	
		/**
	 	 * Handle a request for the sessnio naem fo the current session
		 * We return a failure state if no session is open
		 *
		 * @param msg the received message
		 */
		void handle_session_name_request(const char* msg) ;
	
		 
		//---------------------------------------------------------------------------------------//
		// handlers for specific item update changes
	
		/**
		 * Handle ImageFrameView positional changes
		 *
		 * @param msg the received message
		 */
		void handle_imageframe_view_position_update(const char* msg) ;
		
		/**
		 * Handle ImageFrameView Duration changes
		 *
		 * @param msg the received message
		 */
		void handle_imageframe_view_duration_update(const char* msg) ;
		
		/**
		 * Handle ImageFrameView Position Lock Constraint changes
		 *
		 * @param msg the received message
		 */
		void handle_imageframe_position_lock_update(const char* msg) ;
		
		/**
		 * Handle ImageFrameView Maximum Duration changes
		 *
		 * @param msg the received message
		 */
		void handle_imageframe_view_max_duration_update(const char* msg) ;
		
		/**
		 * Handle image frame max duration enable constraint changes
		 *
		 * @param msg the received message
		 */
		void handle_imageframe_view_max_duration_enable_update(const char* msg) ;
		
		/**
		 * Handle ImageFrameView Minimum Duration changes
		 *
		 * @param msg the received message
		 */
		void handle_imageframe_view_min_duration_update(const char* msg) ;
		
		/**
		 * Handle image frame min duration enable constraint changes
		 *
		 * @param msg the received message
		 */
		void handle_imageframe_view_min_duration_enable_update(const char* msg) ;
	
	
		/**
		 * Handle MarkerView position changes
		 *
		 * @param msg the received message
		 */
		void handle_marker_view_position_update(const char* msg) ;
		
		/**
		 * Handle MarkerView duration changes
		 *
		 * @param msg the received message
		 */
		void handle_marker_view_duration_update(const char* msg) ;
		
		/**
		 * Handle MarkerView Position Lock Constraint changes
		 *
		 * @param msg the received message
		 */
		void handle_marker_view_position_lock_update(const char* msg) ;
		
		/**
		 * Handle MarkerView maximum duration changes
		 *
		 * @param msg the received message
		 */
		void handle_marker_view_max_duration_update(const char* msg) ;
		
		/**
		 * Handle MarkerView minimum duration changes
		 *
		 * @param msg the received message
		 */
		void handle_marker_view_min_duration_update(const char* msg) ;
	
	

		//---------------------------------------------------------------------------------------//
		// handlers for Session Actions
	
		/**
		 * Handle the opening of a named audio session
		 *
		 * @param msg the received message
		 */
		void handle_open_session(const char* msg) ;
		
		/**
		 * Handle the closing of a named audio session
		 *
		 * @param msg the received message
		 */
		void handle_closed_session(const char* msg) ;
	
		//---------------------------------------------------------------------------------------//
		// handlers for the shutdown of the Image Compositor
		
		/**
		 * Handle the shutdown message from the image compositor
		 *
		 * @param msg the received message
		 */
		void handle_shutdown(const char* msg) ;
	
	
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
		std::string get_message_part(int start, int32_t num_chars, const char* msg) ;
		
		
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
		void decompose_imageframe_item_desc(const char* msg, int& position, std::string& track_id, int& track_id_size, std::string& scene_id, int& scene_id_size, std::string& item_id, int& item_id_size) ;
	
		/**
		 * Compose a description of the specified image frame view
		 * The description consists of the parent track name size and name,
		 * the parent group name size and name, and the item name size and name
		 *
		 * @param ifv the item to compose a description of
		 * @param buffer the buffer to write the description
		 */
		void compose_imageframe_item_desc(ImageFrameView* ifv, std::ostringstream& buffer) ;

		/**
		 * Compose a description of the specified marker view
		 * The description consists of the parent track name size and name,
		 * and the item name size and name
		 *
		 * @param mv the item to compose a description of
		 * @param buffer the buffer to write the description
		 */
		void compose_marker_item_desc(MarkerView* mv, std::ostringstream& buffer) ;
		
		
		/**
		 * Returns the ImageFrameView from the specified description
		 * The errcode parameter is used to indicate the item which caused
		 * an error on failure of this method
		 * 0 = suces
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
		ImageFrameView* get_imageframe_view_from_desc(const std::string& track_id, const std::string& group_ud, const std::string& item_id, int& errcode, std::string& errmsg) ;
	 
		//---------------------------------------------------------------------------------------//
		// Convenince Message Send Methods
		
		/**
		 * Sends a message throught the socket
		 *
		 * @param msg the message to send
		 * @return the return value of the socket call
		 */
		int send_message(const std::string& msg) ;
		
		/**
		 * Reads a message from the Socket
		 *
		 * @param msg a string to populate with the received message
		 * @return the return value from the socket call
		 */
		int read_message(std::string& msg) ;
		
		/**
		 * Convenience method to compose and send a success messasge back to the Image Compositor
		 *
		 */
		void send_return_success() ;
		
		/**
		 * Convenience method to compose and send a failure messasge back to the Image Compositor
		 *
		 * @param msg the failure message
		 */
		void send_return_failure(const std::string& msg) ;
	
		//---------------------------------------------------------------------------------------//
		// Memebr Data
		
		/** Our instance of the socket handler, singleton */
		static ImageFrameSocketHandler* _instance ;
		
		/** The Ardour PublicEditor */
		PublicEditor& thePublicEditor ;
		
		/** the socket file descriptor */
		int theArdourToCompositorSocket ;
		
		/** This stores the 'tag' returned from gdk_input_add, which is required for removing the input */
		int theGdkInputTag ;

} ; /* class ImageFrameSocketHandler */

#endif /* __ardour_gtk_imageframe_socket_handler_h__ */
