/*
    Copyright (C) 2008 Paul Davis 

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

#ifndef __gtkmm2ext_scroomer_h__ 
#define __gtkmm2ext_scroomer_h__

#include <gtkmm/drawingarea.h>
#include <gtkmm/adjustment.h>
#include <gdkmm.h>

namespace Gtkmm2ext {

class Scroomer : public Gtk::DrawingArea
{
public:
	enum Component {
		TopBase = 0,
		Handle1 = 1,
		Slider = 2,
		Handle2 = 3,
		BottomBase = 4,
		Total = 5,
		None = 6
	};

	Scroomer(Gtk::Adjustment& adjustment);
	~Scroomer();

	bool on_motion_notify_event (GdkEventMotion*);
	bool on_button_press_event (GdkEventButton*);
	bool on_button_release_event (GdkEventButton*);
	bool on_scroll_event (GdkEventScroll*);
	virtual void on_size_allocate (Gtk::Allocation&);

	void set_comp_rect(GdkRectangle&, Component) const;

	Component point_in(double point) const;

	void set_min_page_size(double page_size);
	int get_handle_size() { return handle_size; }
	
	inline int position_of(Component comp) { return position[comp]; }

	sigc::signal0<void> DragStarting;
	sigc::signal0<void> DragFinishing;

	sigc::signal0<void> DoubleClicked;

protected:
	Gtk::Adjustment& adj;

private:
	struct UpdateRect {
		GdkRectangle rect;
		Component first_comp;
	};

	void update();
	void adjustment_changed ();

	int position[6];
	int old_pos[6];
	int handle_size;
	double min_page_size;
	GdkWindow* grab_window;
	Component grab_comp;
	double grab_y;
	double unzoomed_val;
	double unzoomed_page;
	bool pinch;
};

} // namespace

#endif /* __gtkmm2ext_scroomer_h__ */
