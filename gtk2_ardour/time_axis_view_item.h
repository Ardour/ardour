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

#ifndef __gtk_ardour_time_axis_view_item_h__
#define __gtk_ardour_time_axis_view_item_h__

#include <jack/jack.h>
#include <string>

#include <libgnomecanvasmm/text.h>

#include "selectable.h"
#include "simplerect.h"
#include "canvas.h"

class TimeAxisView;

/**
 * A base class for 'items' that may appear upon a TimeAxisView
 *
 */
class TimeAxisViewItem : public Selectable
{
   public:
       virtual ~TimeAxisViewItem() ;
    
    /**
     * Set the position of this item upon the timeline to the specified value
     *
     * @param pos the new position
     * @param src the identity of the object that initiated the change
     * @return true if the position change was a success, false otherwise
     */
    virtual bool set_position(nframes_t pos, void* src, double* delta = 0) ;
    
    /**
     * Return the position of this item upon the timeline
     *
     * @return the position of this item
     */
    nframes_t get_position() const ; 
    
    /**
     * Sets the duration of this item
     *
     * @param dur the new duration of this item
     * @param src the identity of the object that initiated the change
     * @return true if the duration change was succesful, false otherwise
     */
    virtual bool set_duration(nframes_t dur, void* src) ;
    
    /**
     * Returns the duration of this item
     *
     */
    nframes_t get_duration() const ;
    
    /**
     * Sets the maximum duration that this item make have.
     *
     * @param dur the new maximum duration
     * @param src the identity of the object that initiated the change
     */
    virtual void set_max_duration(nframes_t dur, void* src) ;
    
    /**
     * Returns the maxmimum duration that this item may be set to
     *
     * @return the maximum duration that this item may be set to
     */
    nframes_t get_max_duration() const ;
    
    /**
     * Sets the minimu duration that this item may be set to
     *
     * @param the minimum duration that this item may be set to
     * @param src the identity of the object that initiated the change
     */
    virtual void set_min_duration(nframes_t dur, void* src) ;
    
    /**
     * Returns the minimum duration that this item mey be set to
     *
     * @return the nimum duration that this item mey be set to
     */
    nframes_t get_min_duration() const ;
    
    /**
     * Sets whether the position of this Item is locked to its current position
     * Locked items cannot be moved until the item is unlocked again.
     *
     * @param yn set to true to lock this item to its current position
     * @param src the identity of the object that initiated the change
     */
    virtual void set_position_locked(bool yn, void* src) ;
    
    /**
     * Returns whether this item is locked to its current position
     *
     * @return true if this item is locked to its current posotion
     *         false otherwise
     */
    bool get_position_locked() const ;
    
    /**
     * Sets whether the Maximum Duration constraint is active and should be enforced
     *
     * @param active set true to enforce the max duration constraint
     * @param src the identity of the object that initiated the change
     */
    void set_max_duration_active(bool active, void* src) ;
    
    /**
     * Returns whether the Maximum Duration constraint is active and should be enforced
     *
     * @return true if the maximum duration constraint is active, false otherwise
     */
    bool get_max_duration_active() const ;
    
    /**
     * Sets whether the Minimum Duration constraint is active and should be enforced
     *
     * @param active set true to enforce the min duration constraint
     * @param src the identity of the object that initiated the change
     */
    void set_min_duration_active(bool active, void* src) ;
    
    /**
     * Returns whether the Maximum Duration constraint is active and should be enforced
     *
     * @return true if the maximum duration constraint is active, false otherwise
     */
    bool get_min_duration_active() const ;
    
    /**
     * Set the name/Id of this item.
     *
     * @param new_name the new name of this item
     * @param src the identity of the object that initiated the change
     */
    void set_item_name(std::string new_name, void* src) ;
    
    /**
     * Returns the name/id of this item
     *
     * @return the name/id of this item
     */
    virtual std::string get_item_name() const ;
    
    /**
     * Set to true to indicate that this item is currently selected
     *
     * @param yn true if this item is currently selected
     */
    virtual void set_selected(bool yn) ;

    /**
     * Set to true to indicate that this item should show its selection status
     *
     * @param yn true if this item should show its selected status
     */
    virtual void set_should_show_selection (bool yn) ;

    void set_sensitive (bool yn) { _sensitive = yn; }
    bool sensitive () const { return _sensitive; }
    
    //---------------------------------------------------------------------------------------//
    // Parent Component Methods
    
    /**
     * Returns the TimeAxisView that this item is upon
     *
     * @return the timeAxisView that this item is placed upon
     */
    TimeAxisView& get_time_axis_view() ;
    
    //---------------------------------------------------------------------------------------//
    // ui methods & data
    
    /**
     * Sets the displayed item text
     * This item is the visual text name displayed on the canvas item, this can be different to the name of the item
     *
     * @param new_name the new name text to display
     */
    void set_name_text(const Glib::ustring& new_name) ;
    
    void    set_y_position_and_height (double, double);    

    /**
     * 
     */
    void set_color(Gdk::Color& color) ;
    
    /**
     * 
     */
    ArdourCanvas::Item* get_canvas_frame() ;

    /**
     * 
     */
    ArdourCanvas::Item* get_canvas_group();

    /**
     * 
     */
    ArdourCanvas::Item* get_name_highlight();

    /**
     * 
     */
    ArdourCanvas::Text* get_name_text();


    /**
     * Returns the time axis that this item is upon
     */
    TimeAxisView& get_trackview() const { return trackview; }

    /**
     * Sets the samples per unit of this item.
     * this item is used to determine the relative visual size and position of this item
     * based upon its duration and start value.
     *
     * @param spu the new samples per unit value
     */
    virtual void set_samples_per_unit(double spu) ;
    
    /**
     * Returns the current samples per unit of this item
     *
     * @return the samples per unit of this item
     */
    double get_samples_per_unit() ;

    virtual void raise () { return; }
    virtual void raise_to_top () { return; }
    virtual void lower () { return; }
    virtual void lower_to_bottom () { return; }
    
    /**
     * returns true if the name area should respond to events.
     */
    bool name_active() const { return name_connected; }

    // Default sizes, font and spacing
    static Pango::FontDescription* NAME_FONT ;
    static bool have_name_font;
    static const double NAME_X_OFFSET ;
    static const double GRAB_HANDLE_LENGTH ;
    /* these are not constant, but vary with the pixel size
       of the font used to display the item name.
    */
    static double NAME_Y_OFFSET ;
    static double NAME_HIGHLIGHT_SIZE ;
    static double NAME_HIGHLIGHT_THRESH ;

    /**
     * Handles the Removal of this time axis item
     * This _needs_ to be called to alert others of the removal properly, ie where the source
     * of the removal came from.
     *
     * XXX Although im not too happy about this method of doing things, I cant think of a cleaner method
     *     just now to capture the source of the removal
     *
     * @param src the identity of the object that initiated the change
     */
    virtual void remove_this_item(void* src) ;
    
    /**
     * Emitted when this Group has been removed
     * This is different to the GoingAway signal in that this signal
     * is emitted during the deletion of this Time Axis, and not during
     * the destructor, this allows us to capture the source of the deletion
     * event
     */

    sigc::signal<void,std::string,void*> ItemRemoved ;
    
    /** Emitted when the name/Id of this item is changed */
    sigc::signal<void,std::string,std::string,void*> NameChanged ;
    
    /** Emiited when the position of this item changes */
    sigc::signal<void,nframes_t,void*> PositionChanged ;
    
    /** Emitted when the position lock of this item is changed */
    sigc::signal<void,bool,void*> PositionLockChanged ;
    
    /** Emitted when the duration of this item changes */
    sigc::signal<void,nframes_t,void*> DurationChanged ;
    
    /** Emitted when the maximum item duration is changed */
    sigc::signal<void,nframes_t,void*> MaxDurationChanged ;
    
    /** Emitted when the mionimum item duration is changed */
    sigc::signal<void,nframes_t,void*> MinDurationChanged ;
    
    enum Visibility {
	    ShowFrame = 0x1,
	    ShowNameHighlight = 0x2,
	    ShowNameText = 0x4,
	    ShowHandles = 0x8,
	    HideFrameLeft = 0x10,
	    HideFrameRight = 0x20,
	    HideFrameTB = 0x40,
	    FullWidthNameHighlight = 0x80
    };
  protected:
    /**
     * Constructs a new TimeAxisViewItem.
     *
     * @param it_name the unique name/Id of this item
     * @param parent the parent canvas group
     * @param tv the TimeAxisView we are going to be added to
     * @param spu samples per unit
     * @param base_color
     * @param start the start point of this item
     * @param duration the duration of this item
     */
    TimeAxisViewItem(const std::string & it_name, ArdourCanvas::Group& parent, TimeAxisView& tv, double spu, Gdk::Color& base_color, 
		     nframes_t start, nframes_t duration, bool recording = false, Visibility v = Visibility (0));

    TimeAxisViewItem (const TimeAxisViewItem& other);

    void init (const std::string& it_name, double spu, Gdk::Color& base_color, nframes_t start, nframes_t duration, Visibility vis);
    
    /**
     * Calculates some contrasting color for displaying various parts of this item, based upon the base color
     *
     * @param color the base color of the item
     */
    virtual void compute_colors(Gdk::Color& color) ;
    
    /**
     * convenience method to set the various canvas item colors
     */
    virtual void set_colors() ;
    
    /**
     * Sets the frame color depending on whether this item is selected
     */
    virtual void set_frame_color() ;
    
    /**
     * Sets the colors of the start and end trim handle depending on object state
     *
     */
    void set_trim_handle_colors() ;

    virtual void reset_width_dependent_items (double pixel_width);
    void reset_name_width (double pixel_width);

    /**
     * Callback used to remove this item during the gtk idle loop
     * This is used to avoid deleting the obejct while inside the remove_this_group
     * method
     *
     * @param item the time axis item to remove
     * @param src the identity of the object that initiated the change
     */
    static gint idle_remove_this_item(TimeAxisViewItem* item, void* src) ;
    
    /** The time axis that this item is upon */
    TimeAxisView& trackview ;
    
    /** indicates whether this item is locked to its current position */
    bool position_locked ;
    
    /** The posotion of this item on the timeline */
    nframes_t frame_position ;
    
    /** the duration of this item upon the timeline */
    nframes_t item_duration ;
    
    /** the maximum duration that we allow this item to take */
    nframes_t max_item_duration ;
    
    /** the minimu duration that we allow this item to take */
    nframes_t min_item_duration ;
    
    /** indicates whether this Max Duration constraint is active */
    bool max_duration_active ;
    
    /** indicates whether this Min Duration constraint is active */
    bool min_duration_active ;
    
    /** the curretn samples per canvas unit */
    double samples_per_unit ;
    
    /** indicates if this item is currently selected */
    bool selected ;

    /** should the item show its selected status */
    bool should_show_selection;

    /** should the item respond to events */
    bool _sensitive;
    
    /**
     * The unique item name of this Item
     * Each item upon a time axis must have a unique id
     */
    std::string item_name ;
    
    /**
     * true if the name should respond to events
     */
    bool name_connected;

    /**
     * true if a small vestigial rect should be shown when the item gets very narrow
     */

    bool show_vestigial;

    uint32_t fill_opacity;
    uint32_t fill_color ;
    uint32_t frame_color_r ;
    uint32_t frame_color_g ;
    uint32_t frame_color_b ;
    uint32_t selected_frame_color_r ;
    uint32_t selected_frame_color_g ;
    uint32_t selected_frame_color_b ;
    uint32_t label_color ;
    
    uint32_t handle_color_r ;
    uint32_t handle_color_g ;
    uint32_t handle_color_b ;
    uint32_t lock_handle_color_r ;
    uint32_t lock_handle_color_g ;
    uint32_t lock_handle_color_b ;
    
    ArdourCanvas::Group*      group;
    ArdourCanvas::SimpleRect* vestigial_frame;
    ArdourCanvas::SimpleRect* frame;
    ArdourCanvas::Text*       name_text;
    ArdourCanvas::SimpleRect* name_highlight;
    ArdourCanvas::SimpleRect* frame_handle_start;
    ArdourCanvas::SimpleRect* frame_handle_end;

    int name_text_width;
    double last_name_text_width;

    std::map<Glib::ustring::size_type,int> name_text_size_cache;
    
    Visibility visibility;
	bool _recregion;


}; /* class TimeAxisViewItem */

#endif /* __gtk_ardour_time_axis_view_item_h__ */
