/*
    Copyright (C) 2009 Paul Davis 

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

#include <gtkmm/box.h>

namespace Gtkmm2ext {

/** Parent class for children of a DnDVBox */	
class DnDVBoxChild
{
public:
	virtual ~DnDVBoxChild () {}
	
	/** @return The widget that is to be put into the DnDVBox */
	virtual Gtk::Widget& widget () = 0;
	
	/** @return An EventBox containing the widget that should be used for selection, dragging etc. */
	virtual Gtk::EventBox& action_widget () = 0;

	/** @return Text to use in the icon that is dragged */
	virtual std::string drag_text () const = 0;
};

/** A VBox whose contents can be dragged and dropped */
template <class T>
class DnDVBox : public Gtk::EventBox
{
public:
	DnDVBox () : _drag_icon (0), _expecting_unwanted_button_event (false)
	{
		_targets.push_back (Gtk::TargetEntry ("processor"));

		add (_internal_vbox);
		add_events (Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK | Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);

		signal_button_press_event().connect (bind (mem_fun (*this, &DnDVBox::button_press), (T *) 0));
		signal_button_release_event().connect (bind (mem_fun (*this, &DnDVBox::button_release), (T *) 0));
		
		drag_dest_set (_targets);
		signal_drag_data_received().connect (mem_fun (*this, &DnDVBox::drag_data_received));
	}
	
	virtual ~DnDVBox ()
	{
		clear ();
		
		delete _drag_icon;
	}

	/** Add a child at the end of the widget.  The DnDVBox will take responsibility for deleting the child */
	void add_child (T* child)
	{
		child->action_widget().drag_source_set (_targets);
		child->action_widget().signal_drag_begin().connect (sigc::bind (mem_fun (*this, &DnDVBox::drag_begin), child));
		child->action_widget().signal_drag_data_get().connect (sigc::bind (mem_fun (*this, &DnDVBox::drag_data_get), child));
		child->action_widget().signal_drag_end().connect (sigc::bind (mem_fun (*this, &DnDVBox::drag_end), child));
		child->action_widget().signal_button_press_event().connect (sigc::bind (mem_fun (*this, &DnDVBox::button_press), child));
		child->action_widget().signal_button_release_event().connect (sigc::bind (mem_fun (*this, &DnDVBox::button_release), child));
		
		_internal_vbox.pack_start (child->widget(), false, false);
		
		_children.push_back (child);
		child->widget().show_all ();
	}

	/** @return Children, sorted into the order that they are currently being displayed in the widget */
	std::list<T*> children ()
	{
		std::list<T*> sorted_children;

		std::list<Gtk::Widget*> widget_children = _internal_vbox.get_children ();
		for (std::list<Gtk::Widget*>::iterator i = widget_children.begin(); i != widget_children.end(); ++i) {
			T* c = child_from_widget (*i);

			if (c) {
				sorted_children.push_back (c);
			}
		}

		return sorted_children;
	}

	/** @return Selected children */
	std::list<T*> selection () const {
		return _selection;
	}

	/** @param Child
	 *  @return true if the child is selected, otherwise false */
	bool selected (T* child) const {
		return (find (_selection.begin(), _selection.end(), child) != _selection.end());
	}

	/** Clear all children from the widget */
	void clear ()
	{
		_selection.clear ();

		for (typename std::list<T*>::iterator i = _children.begin(); i != _children.end(); ++i) {
			_internal_vbox.remove ((*i)->widget());
			delete *i;
		}

		_children.clear ();
	}


	void select_all ()
	{
		clear_selection ();
		for (typename std::list<T*>::iterator i = _children.begin(); i != _children.end(); ++i) {
			add_to_selection (*i);
		}

		SelectionChanged (); /* EMIT SIGNAL */
	}

	void select_none ()
	{
		clear_selection ();

		SelectionChanged (); /* EMIT SIGNAL */
	}

	/** @param x x coordinate.
	 *  @param y y coordinate.
	 *  @return Pair consisting of the child under (x, y) (or 0) and the index of the child under (x, y) (or -1)
	 */
	std::pair<T*, int> get_child_at_position (int /*x*/, int y) const
	{
		std::list<Gtk::Widget const *> children = _internal_vbox.get_children ();
		std::list<Gtk::Widget const *>::iterator i = children.begin();
		
		int n = 0;
		while (i != children.end()) {
			Gdk::Rectangle const a = (*i)->get_allocation ();
			if (y >= a.get_y() && y <= (a.get_y() + a.get_height())) {
				break;
			}
			++i;
			++n;
		}
		
		if (i == children.end()) {
			return std::make_pair ((T *) 0, -1);
		}

		return std::make_pair (child_from_widget (*i), n);
	}	

	/** Children have been reordered by a drag */
	sigc::signal<void> Reordered;

	/** A button has been pressed over the widget */
	sigc::signal<bool, GdkEventButton*, T*> ButtonPress;

	/** A button has been release over the widget */
	sigc::signal<bool, GdkEventButton*, T*> ButtonRelease;

	/** A child has been dropped onto this DnDVBox from another one;
	 *  Parameters are the source DnDVBox, our child which the other one was dropped on (or 0) and the DragContext.
	 */
	sigc::signal<void, DnDVBox*, T*, Glib::RefPtr<Gdk::DragContext> const & > DropFromAnotherBox;
	sigc::signal<void> SelectionChanged;

private:
	void drag_begin (Glib::RefPtr<Gdk::DragContext> const & context, T* child)
	{
		/* make up an icon for the drag */
		Gtk::Window* _drag_icon = new Gtk::Window (Gtk::WINDOW_POPUP);
		_drag_icon->set_name (get_name ());
		Gtk::Label* l = new Gtk::Label (child->drag_text ());
		l->set_padding (4, 4);
		_drag_icon->add (*Gtk::manage (l));
		_drag_icon->show_all_children ();
		int w, h;
		_drag_icon->get_size (w, h);
		_drag_icon->drag_set_as_icon (context, w / 2, h);
		
		_drag_source = this;
	}
	
	void drag_data_get (Glib::RefPtr<Gdk::DragContext> const &, Gtk::SelectionData & selection_data, guint, guint, T* child)
	{
		selection_data.set (selection_data.get_target(), 8, (const guchar *) &child, sizeof (&child));
	}
	
	void drag_data_received (
		Glib::RefPtr<Gdk::DragContext> const & context, int x, int y, Gtk::SelectionData const & selection_data, guint /*info*/, guint time
		)
	{
		/* work out where it was dropped */
		std::pair<T*, int> const drop = get_child_at_position (x, y);
		
		if (_drag_source == this) {

			/* dropped from ourselves onto ourselves */
			
			T* child = *((T **) selection_data.get_data());
			
			/* reorder child widgets accordingly */
			if (drop.first == 0) {
				_internal_vbox.reorder_child (child->widget(), -1);
			} else {
				_internal_vbox.reorder_child (child->widget(), drop.second);
			}
			
		} else {
			
			/* drag started in another DnDVBox; raise a signal to say what happened */
			
			std::list<T*> dropped = _drag_source->selection ();
			DropFromAnotherBox (_drag_source, drop.first, context);
		}
		
		context->drag_finish (false, false, time);
	}
	
	void drag_end (Glib::RefPtr<Gdk::DragContext> const &, T *)
	{
		delete _drag_icon;
		_drag_icon = 0;

		Reordered (); /* EMIT SIGNAL */
	}
	
	bool button_press (GdkEventButton* ev, T* child)
	{
		if (_expecting_unwanted_button_event == true && child == 0) {
			_expecting_unwanted_button_event = false;
			return true;
		}

		if (child) {
			_expecting_unwanted_button_event = true;
		}
			
		if (ev->button == 1 || ev->button == 3) {

			if (!selected (child)) {

				if ((ev->state & Gdk::SHIFT_MASK) && !_selection.empty()) {

					/* Shift-click; select all between the clicked child and any existing selections */

					bool selecting = false;
					bool done = false;
					for (typename std::list<T*>::const_iterator i = _children.begin(); i != _children.end(); ++i) {

						bool const was_selected = selected (*i);

						if (selecting && !was_selected) {
							add_to_selection (*i);
						}
						
						if (!selecting && !done) {
							if (selected (*i)) {
								selecting = true;
							} else if (*i == child) {
								selecting = true;
								add_to_selection (child);
							}
						} else if (selecting) {
							if (was_selected || *i == child) {
								selecting = false;
								done = true;
							}
						}
					}

				} else {
						
					if ((ev->state & Gdk::CONTROL_MASK) == 0) {
						clear_selection ();
					}
					
					if (child) {
						add_to_selection (child);
					}

				}
				
				SelectionChanged (); /* EMIT SIGNAL */
				
			} else {
				/* XXX THIS NEEDS GENERALIZING FOR OS X */
				if (ev->button == 1 && (ev->state & Gdk::CONTROL_MASK)) {
					if (child && selected (child)) {
						remove_from_selection (child);
						SelectionChanged (); /* EMIT SIGNAL */
					}
				}
			}
		}


		return ButtonPress (ev, child); /* EMIT SIGNAL */
	}
	
	bool button_release (GdkEventButton* ev, T* child)
	{
		if (_expecting_unwanted_button_event == true && child == 0) {
			_expecting_unwanted_button_event = false;
			return true;
		}

		if (child) {
			_expecting_unwanted_button_event = true;
		}

		return ButtonRelease (ev, child); /* EMIT SIGNAL */
	}

	void clear_selection ()
	{
		for (typename std::list<T*>::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			(*i)->action_widget().set_state (Gtk::STATE_NORMAL);
		}
		_selection.clear ();
	}
	
	void add_to_selection (T* child)
	{
		child->action_widget().set_state (Gtk::STATE_SELECTED);
		_selection.push_back (child);
	}
		
	
	void remove_from_selection (T* child)
	{
		typename std::list<T*>::iterator x = find (_selection.begin(), _selection.end(), child);
		if (x != _selection.end()) {
			child->action_widget().set_state (Gtk::STATE_NORMAL);
			_selection.erase (x);
		}
	}
		
	T* child_from_widget (Gtk::Widget const * w) const
	{
		typename std::list<T*>::const_iterator i = _children.begin();
		while (i != _children.end() && &(*i)->widget() != w) {
			++i;
		}
		
		if (i == _children.end()) {
			return 0;
		}

		return *i;
	}
	
	Gtk::VBox _internal_vbox;
	std::list<Gtk::TargetEntry> _targets;
	std::list<T*> _children;
	std::list<T*> _selection;
	Gtk::Window* _drag_icon;
	bool _expecting_unwanted_button_event;

	static DnDVBox* _drag_source;
};

template <class T>
DnDVBox<T>* DnDVBox<T>::_drag_source = 0;
	
}
