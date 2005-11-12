/*
    Copyright (C) 2004 Paul Davis 

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

#include <cmath>

#include <sigc++/bind.h>

#include <gtkmm/frame.h>
#include <gtkmm/image.h>
#include <gtkmm/scrolledwindow.h>

#include <ardour/automation_event.h>
#include <ardour/curve.h>
#include <ardour/crossfade.h>
#include <ardour/session.h>
#include <ardour/auditioner.h>
#include <ardour/audioplaylist.h>
#include <ardour/playlist_templates.h>

#include <gtkmm2ext/gtk_ui.h>

#include "ardour_ui.h"
#include "crossfade_edit.h"
#include "rgb_macros.h"
#include "canvas-simplerect.h"
#include "canvas-waveview.h"
#include "keyboard.h"
#include "utils.h"
#include "gui_thread.h"

using namespace std;
using namespace ARDOUR;
using namespace Gtk;
using namespace sigc;
using namespace Editing;

#include "i18n.h"

const int32_t CrossfadeEditor::Point::size = 7;
const double CrossfadeEditor::canvas_border = 10;
CrossfadeEditor::Presets* CrossfadeEditor::fade_in_presets = 0;
CrossfadeEditor::Presets* CrossfadeEditor::fade_out_presets = 0;

#include "crossfade_xpms.h"

CrossfadeEditor::Half::Half ()
	: line (0), 
	  normative_curve (0.0, 1.0, 1.0, true),
	  gain_curve (0.0, 2.0, 1.0, true)
{
}

CrossfadeEditor::CrossfadeEditor (Session& s, Crossfade& xf, double my, double mxy)
	: ArdourDialog (_("crossfade editor")),
	  cancel_button (_("Cancel")),
	  ok_button (_("OK")),
	  xfade (xf),
	  session (s),
	  clear_button (_("Clear")),
	  revert_button (_("Reset")),
	  audition_both_button (_("Fade")),
	  audition_left_dry_button (_("Out (dry)")),
	  audition_left_button (_("Out")),
	  audition_right_dry_button (_("In (dry)")),
	  audition_right_button (_("In")),

	  preroll_button (_("With Pre-roll")),
	  postroll_button (_("With Post-roll")),
	  
	  miny (my),
	  maxy (mxy),

	  fade_in_table (3, 3),
	  fade_out_table (3, 3),

	  select_in_button (_("Fade In")),
	  select_out_button (_("Fade Out"))
{
	set_wmclass ("ardour_automationedit", "Ardour");
	set_name ("CrossfadeEditWindow");
	set_title (_("ardour: x-fade edit"));
	set_position (Gtk::WIN_POS_MOUSE);

	add (vpacker);
	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|Gdk::POINTER_MOTION_MASK);

	RadioButtonGroup sel_but_group = select_in_button.get_group();
	select_out_button.set_group (sel_but_group);
	select_out_button.set_mode (false);
	select_in_button.set_mode (false);

	if (fade_in_presets == 0) {
		build_presets ();
	}

	point_grabbed = false;
	toplevel = 0;

	_canvas = gnome_canvas_new_aa ();

	canvas = Glib::wrap (_canvas);
	canvas->signal_size_allocate().connect (mem_fun(*this, &CrossfadeEditor::canvas_allocation));
	canvas->set_size_request (425, 200);

	toplevel = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS(_canvas)),
					gnome_canvas_simplerect_get_type(),
					"x1", 0.0,
					"y1", 0.0,
					"x2", 10.0,
					"y2", 10.0,
					"fill", (gboolean) TRUE,
					"fill_color_rgba", (guint32) color_map[cCrossfadeEditorBase],
					"outline_pixels", 0,
					NULL);

	gtk_signal_connect (GTK_OBJECT(toplevel),
			    "event",
			    (GtkSignalFunc) _canvas_event,
			    this);

	fade[Out].line = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS(_canvas)),
					      gnome_canvas_line_get_type (),
					      "width_pixels", (guint) 1,
					      "fill_color_rgba", color_map[cCrossfadeEditorLine],
					      NULL);

	fade[Out].shading = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS(_canvas)),
						 gnome_canvas_polygon_get_type(),
						 "fill_color_rgba", color_map[cCrossfadeEditorLineShading],
						 NULL);
	
	fade[In].line = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS(_canvas)),
					     gnome_canvas_line_get_type (),
					     "width_pixels", (guint) 1,
					     "fill_color_rgba", color_map[cCrossfadeEditorLine],
					     NULL);
	
	fade[In].shading = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS(_canvas)),
						gnome_canvas_polygon_get_type(),
						"fill_color_rgba", color_map[cCrossfadeEditorLineShading],
						NULL);

	gtk_signal_connect (GTK_OBJECT(fade[In].shading),
			    "event",
			    (GtkSignalFunc) _canvas_event,
			    this);


	gtk_signal_connect (GTK_OBJECT(fade[Out].shading),
			    "event",
			    (GtkSignalFunc) _canvas_event,
			    this);

	gtk_signal_connect (GTK_OBJECT(fade[In].line),
			    "event",
			    (GtkSignalFunc) _curve_event,
			    this);

	gtk_signal_connect (GTK_OBJECT(fade[Out].line),
			    "event",
			    (GtkSignalFunc) _curve_event,
			    this);


	select_in_button.set_name (X_("CrossfadeEditCurveButton"));
	select_out_button.set_name (X_("CrossfadeEditCurveButton"));

	select_in_button.signal_clicked().connect (bind (mem_fun (*this, &CrossfadeEditor::curve_select_clicked), In));
	select_out_button.signal_clicked().connect (bind (mem_fun (*this, &CrossfadeEditor::curve_select_clicked), Out));

	HBox* acbox = manage (new HBox);
	
	audition_box.set_border_width (7);
	audition_box.set_spacing (5);
	audition_box.set_homogeneous (false);
	audition_box.pack_start (audition_left_dry_button, false, false);
	audition_box.pack_start (audition_left_button, false, false);
	audition_box.pack_start (audition_both_button, false, false);
	audition_box.pack_start (audition_right_button, false, false);
	audition_box.pack_start (audition_right_dry_button, false, false);

	Frame* audition_frame = manage (new Frame (_("Audition")));
	
	audition_frame->set_name (X_("CrossfadeEditFrame"));
	audition_frame->add (audition_box);

	acbox->pack_start (*audition_frame, true, false);

	Frame* canvas_frame = manage (new Frame);
	canvas_frame->add (*canvas);
	canvas_frame->set_shadow_type (Gtk::SHADOW_IN);

	fade_in_table.attach (select_in_button, 0, 2, 0, 1, Gtk::FILL|Gtk::EXPAND);
	fade_out_table.attach (select_out_button, 0, 2, 0, 1, Gtk::FILL|Gtk::EXPAND);

	Image *pxmap;
	Button* pbutton;
	int row;
	int col;

	row = 1;
	col = 0;

	for (list<Preset*>::iterator i = fade_in_presets->begin(); i != fade_in_presets->end(); ++i) {

	        pxmap = manage (new Image (Gdk::Pixbuf::create_from_xpm_data((*i)->xpm)));
		pbutton = manage (new Button);
		pbutton->add (*pxmap);
		pbutton->set_name ("CrossfadeEditButton");
		pbutton->signal_clicked().connect (bind (mem_fun(*this, &CrossfadeEditor::apply_preset), *i));
		fade_in_table.attach (*pbutton, col, col+1, row, row+1);
		fade_in_buttons.push_back (pbutton);

		col++;

		if (col == 2) {
			col = 0;
			row++;
		}
	}

	row = 1;
	col = 0;

	for (list<Preset*>::iterator i = fade_out_presets->begin(); i != fade_out_presets->end(); ++i) {

	        pxmap = manage (new Image (Gdk::Pixbuf::create_from_xpm_data((*i)->xpm)));
		pbutton = manage (new Button);
		pbutton->add (*pxmap);
		pbutton->set_name ("CrossfadeEditButton");
		pbutton->signal_clicked().connect (bind (mem_fun(*this, &CrossfadeEditor::apply_preset), *i));
		fade_out_table.attach (*pbutton, col, col+1, row, row+1);
		fade_out_buttons.push_back (pbutton);

		col++;

		if (col == 2) {
			col = 0;
			row++;
		}
	}

	clear_button.set_name ("CrossfadeEditButton");
	revert_button.set_name ("CrossfadeEditButton");
	ok_button.set_name ("CrossfadeEditButton");
	cancel_button.set_name ("CrossfadeEditButton");
	preroll_button.set_name ("CrossfadeEditButton");
	postroll_button.set_name ("CrossfadeEditButton");
	audition_both_button.set_name ("CrossfadeEditAuditionButton");
	audition_left_dry_button.set_name ("CrossfadeEditAuditionButton");
	audition_left_button.set_name ("CrossfadeEditAuditionButton");
	audition_right_dry_button.set_name ("CrossfadeEditAuditionButton");
	audition_right_button.set_name ("CrossfadeEditAuditionButton");

	clear_button.signal_clicked().connect (mem_fun(*this, &CrossfadeEditor::clear));
	revert_button.signal_clicked().connect (mem_fun(*this, &CrossfadeEditor::reset));
	audition_both_button.signal_toggled().connect (mem_fun(*this, &CrossfadeEditor::audition_toggled));
	audition_right_button.signal_toggled().connect (mem_fun(*this, &CrossfadeEditor::audition_right_toggled));
	audition_right_dry_button.signal_toggled().connect (mem_fun(*this, &CrossfadeEditor::audition_right_dry_toggled));
	audition_left_button.signal_toggled().connect (mem_fun(*this, &CrossfadeEditor::audition_left_toggled));
	audition_left_dry_button.signal_toggled().connect (mem_fun(*this, &CrossfadeEditor::audition_left_dry_toggled));

	action_box.set_border_width (7);
	action_box.set_spacing (5);
	action_box.set_homogeneous (false);
	action_box.pack_end (cancel_button, false, false);
	action_box.pack_end (ok_button, false, false);
	action_box.pack_end (revert_button, false, false);
	action_box.pack_end (clear_button, false, false);

	Frame* edit_frame = manage (new Frame (_("Edit")));
	edit_frame->set_name (X_("CrossfadeEditFrame"));
	edit_frame->add (action_box);

	Gtk::HBox* action_center_box = manage (new HBox);
	action_center_box->pack_start (*edit_frame, true, false);

	roll_box.pack_start (preroll_button, false, false);
	roll_box.pack_start (postroll_button, false, false);

	Gtk::HBox* rcenter_box = manage (new HBox);
	rcenter_box->pack_start (roll_box, true, false);

	VBox* vpacker2 = manage (new (VBox));

	vpacker2->set_border_width (12);
	vpacker2->set_spacing (7);
	vpacker2->pack_start (*acbox, false, false);
	vpacker2->pack_start (*rcenter_box, false, false);
	vpacker2->pack_start (*action_center_box, false, false);

	curve_button_box.set_spacing (7);
	curve_button_box.pack_start (fade_out_table, false, false, 12);
	curve_button_box.pack_start (*vpacker2, false, false, 12);
	curve_button_box.pack_start (fade_in_table, false, false, 12);
	
	vpacker.set_border_width (12);
	vpacker.set_spacing (5);
	vpacker.pack_start (*canvas_frame, true, true);
	vpacker.pack_start (curve_button_box, false, false);

	/* button to allow hackers to check the actual curve values */

//	Button* foobut = manage (new Button ("dump"));
//	foobut-.signal_clicked().connect (mem_fun(*this, &CrossfadeEditor::dump));
//	vpacker.pack_start (*foobut, false, false);

	current = In;
	set (xfade.fade_in(), In);

	current = Out;
	set (xfade.fade_out(), Out);

	curve_select_clicked (In);

	xfade.StateChanged.connect (mem_fun(*this, &CrossfadeEditor::xfade_changed));

	session.AuditionActive.connect (mem_fun(*this, &CrossfadeEditor::audition_state_changed));
}

CrossfadeEditor::~CrossfadeEditor()
{
	/* most objects will be destroyed when the toplevel window is. */

	for (list<Point*>::iterator i = fade[In].points.begin(); i != fade[In].points.end(); ++i) {
		delete *i;
	}

	for (list<Point*>::iterator i = fade[Out].points.begin(); i != fade[Out].points.end(); ++i) {
		delete *i;
	}
}

void
CrossfadeEditor::dump ()
{
	for (AutomationList::iterator i = fade[Out].normative_curve.begin(); i != fade[Out].normative_curve.end(); ++i) {
		cerr << (*i)->when << ' ' << (*i)->value << endl;
	}
}

void
CrossfadeEditor::audition_state_changed (bool yn)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &CrossfadeEditor::audition_state_changed), yn));

	if (!yn) {
		audition_both_button.set_active (false);
		audition_left_button.set_active (false);
		audition_right_button.set_active (false);
		audition_left_dry_button.set_active (false);
		audition_right_dry_button.set_active (false);
	}
}

void
CrossfadeEditor::set (const ARDOUR::Curve& curve, WhichFade which)
{
	double firstx, endx;
	ARDOUR::Curve::const_iterator the_end;

	for (list<Point*>::iterator i = fade[which].points.begin(); i != fade[which].points.end(); ++i) {
			delete *i;
	}
	
	fade[which].points.clear ();
	fade[which].gain_curve.clear ();
	fade[which].normative_curve.clear ();

	if (curve.empty()) {
		goto out;
	}
	
	the_end = curve.const_end();
	--the_end;
	
	firstx = (*curve.const_begin())->when;
	endx = (*the_end)->when;

	for (ARDOUR::Curve::const_iterator i = curve.const_begin(); i != curve.const_end(); ++i) {
		
		double xfract = ((*i)->when - firstx) / (endx - firstx);
		double yfract = ((*i)->value - miny) / (maxy - miny);
		
		Point* p = make_point ();

		p->move_to (x_coordinate (xfract), y_coordinate (yfract),
			    xfract, yfract);
		
		fade[which].points.push_back (p);
	}

	/* no need to sort because curve is already time-ordered */

  out:
	
	swap (which, current);
	redraw ();
	swap (which, current);
}

gint		     
CrossfadeEditor::_canvas_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data)
{
	CrossfadeEditor* ed = static_cast<CrossfadeEditor*> (data);
	return ed->canvas_event (item, event);
}

gint
CrossfadeEditor::canvas_event (GnomeCanvasItem* item, GdkEvent* event)
{
	switch (event->type) {
	case GDK_BUTTON_PRESS:
		add_control_point ((event->button.x - canvas_border)/effective_width(),
				   1.0 - ((event->button.y - canvas_border)/effective_height()));
		return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

CrossfadeEditor::Point::~Point()
{
	gtk_object_destroy (GTK_OBJECT(box));
}

CrossfadeEditor::Point*
CrossfadeEditor::make_point ()
{
	Point* p = new Point;

	p->box = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS(_canvas)),
				      gnome_canvas_simplerect_get_type(),
				      "fill", (gboolean) TRUE,
				      "fill_color_rgba", color_map[cCrossfadeEditorPointFill],
				      "outline_color_rgba", color_map[cCrossfadeEditorPointOutline],
				      "outline_pixels", (gint) 1,
				      NULL);

	p->curve = fade[current].line;

	gtk_object_set_data (GTK_OBJECT(p->box), "point", p);
	gtk_signal_connect (GTK_OBJECT(p->box), "event", 
			    (GtkSignalFunc) _point_event, 
			    this);
	
	return p;
}

void
CrossfadeEditor::add_control_point (double x, double y)
{
	PointSorter cmp;

	/* enforce end point x location */
	
	if (fade[current].points.empty()) {
		x = 0.0;
	} else if (fade[current].points.size() == 1) {
		x = 1.0;
	} 

	Point* p = make_point ();

	p->move_to (x_coordinate (x), y_coordinate (y), x, y);

	fade[current].points.push_back (p);
	fade[current].points.sort (cmp);

	redraw ();
}

void
CrossfadeEditor::Point::move_to (double nx, double ny, double xfract, double yfract)
{
	const double half_size = rint(size/2.0);
	double x1 = nx - half_size;
	double x2 = nx + half_size;

	gnome_canvas_item_set (box,
			     "x1", x1,
			     "x2", x2,
			     "y1", ny - half_size,
			     "y2", ny + half_size,
			     NULL);
	x = xfract;
	y = yfract;
}

void
CrossfadeEditor::canvas_allocation (Gtk::Allocation& alloc)
{
	if (toplevel) {
		gnome_canvas_item_set (toplevel,
				     "x1", 0.0,
				     "y1", 0.0,
				     "x2", (double) _canvas->allocation.width + canvas_border,
				     "y2", (double) _canvas->allocation.height + canvas_border,
				     NULL);
	}

	gnome_canvas_set_scroll_region (GNOME_CANVAS(_canvas), 0.0, 0.0,
				      _canvas->allocation.width,
				      _canvas->allocation.height);

	Point* end = make_point ();
	PointSorter cmp;

	if (fade[In].points.size() > 1) {
		Point* old_end = fade[In].points.back();
		fade[In].points.pop_back ();
		end->move_to (x_coordinate (old_end->x),
			      y_coordinate (old_end->y),
			      old_end->x, old_end->y);
		delete old_end;
	} else {
		double x = 1.0;
		double y = 0.5;
		end->move_to (x_coordinate (x), y_coordinate (y), x, y);

	}

	fade[In].points.push_back (end);
	fade[In].points.sort (cmp);

	for (list<Point*>::iterator i = fade[In].points.begin(); i != fade[In].points.end(); ++i) {
		(*i)->move_to (x_coordinate((*i)->x), y_coordinate((*i)->y),
			       (*i)->x, (*i)->y);
	}
	
	end = make_point ();
	
	if (fade[Out].points.size() > 1) {
		Point* old_end = fade[Out].points.back();
		fade[Out].points.pop_back ();
		end->move_to (x_coordinate (old_end->x),
			      y_coordinate (old_end->y),
			      old_end->x, old_end->y);
		delete old_end;
	} else {
		double x = 1.0;
		double y = 0.5;
		end->move_to (x_coordinate (x), y_coordinate (y), x, y);

	}

	fade[Out].points.push_back (end);
	fade[Out].points.sort (cmp);

	for (list<Point*>::iterator i = fade[Out].points.begin(); i != fade[Out].points.end(); ++i) {
		(*i)->move_to (x_coordinate ((*i)->x),
			       y_coordinate ((*i)->y),
			       (*i)->x, (*i)->y);
	}
	
	WhichFade old_current = current;
	current = In;
	redraw ();
	current = Out;
	redraw ();
	current = old_current;

	double spu = xfade.length() / (double) effective_width();

	if (fade[In].waves.empty()) {
		make_waves (xfade.in(), In);
	}

	if (fade[Out].waves.empty()) {
		make_waves (xfade.out(), Out);
	}

	double ht;
	vector<GnomeCanvasItem*>::iterator i;
	uint32_t n;

	ht = _canvas->allocation.height / xfade.in().n_channels();

	for (n = 0, i = fade[In].waves.begin(); i != fade[In].waves.end(); ++i, ++n) {
		double yoff;

		yoff = n * ht;

		gnome_canvas_item_set ((*i),
				     "y", yoff,
				     "height", ht,
				     "samples_per_unit", spu,
				     NULL);
	}

	ht = _canvas->allocation.height / xfade.out().n_channels();

	for (n = 0, i = fade[Out].waves.begin(); i != fade[Out].waves.end(); ++i, ++n) {
		double yoff;

		yoff = n * ht;

		gnome_canvas_item_set ((*i),
				     "y", yoff,
				     "height", ht,
				     "samples_per_unit", spu,
				     NULL);
	}

}

gint
CrossfadeEditor::_point_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data)
{
	CrossfadeEditor* ed = static_cast<CrossfadeEditor*> (data);
	return ed->point_event (item, event);
}

gint
CrossfadeEditor::point_event (GnomeCanvasItem* item, GdkEvent* event)
{
	Point* point = static_cast<Point*> (gtk_object_get_data (GTK_OBJECT (item), "point"));

	if (point->curve != fade[current].line) {
		return FALSE;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		point_grabbed = true;
		break;
	case GDK_BUTTON_RELEASE:
		point_grabbed = false;

		if (Keyboard::is_delete_event (&event->button)) {
			fade[current].points.remove (point);
			delete point;
		}

		redraw ();
		break;

	case GDK_MOTION_NOTIFY:
		if (point_grabbed) {
			double new_x, new_y;

			/* can't drag first or last points horizontally */

			if (point == fade[current].points.front() || point == fade[current].points.back()) {
				new_x = point->x;
			} else {
				new_x = (event->motion.x - canvas_border)/effective_width();
			}

			new_y = 1.0 - ((event->motion.y - canvas_border)/effective_height());
			point->move_to (x_coordinate (new_x), y_coordinate (new_y), 
					new_x, new_y);
			redraw ();
		}
		break;
	default:
		break;
	}
	return TRUE;
}

gint
CrossfadeEditor::_curve_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data)
{
	CrossfadeEditor* ed = static_cast<CrossfadeEditor*> (data);
	return ed->curve_event (item, event);
}

gint
CrossfadeEditor::curve_event (GnomeCanvasItem* item, GdkEvent* event)
{
	/* treat it like a toplevel event */

	return canvas_event (item, event);
}

void
CrossfadeEditor::xfade_changed (Change ignored)
{
	set (xfade.fade_in(), In);
	set (xfade.fade_out(), Out);
}

void
CrossfadeEditor::redraw ()
{
	if (_canvas->allocation.width < 2) {
		return;
	}

	jack_nframes_t len = xfade.length ();

	fade[current].normative_curve.clear ();
	fade[current].gain_curve.clear ();

	for (list<Point*>::iterator i = fade[current].points.begin(); i != fade[current].points.end(); ++i) {
		fade[current].normative_curve.add ((*i)->x, (*i)->y);
		fade[current].gain_curve.add (((*i)->x * len), (*i)->y);
	}

	size_t npoints = (size_t) effective_width();
	float vec[npoints];

	fade[current].normative_curve.get_vector (0, 1.0, vec, npoints);
	
	GnomeCanvasPoints* pts = get_canvas_points ("xfade edit1", npoints);
	GnomeCanvasPoints* spts = get_canvas_points ("xfade edit2", npoints + 3);

	/* the shade coordinates *MUST* be in anti-clockwise order.
	 */

	if (current == In) {

		/* lower left */

		spts->coords[0] = canvas_border;
		spts->coords[1] = effective_height() + canvas_border;

		/* lower right */

		spts->coords[2] = effective_width() + canvas_border;
		spts->coords[3] = effective_height() + canvas_border;

		/* upper right */

		spts->coords[4] = effective_width() + canvas_border;
		spts->coords[5] = canvas_border;

		
	} else {

		/*  upper left */
		
		spts->coords[0] = canvas_border;
		spts->coords[1] = canvas_border;

		/* lower left */

		spts->coords[2] = canvas_border;
		spts->coords[3] = effective_height() + canvas_border;

		/* lower right */

		spts->coords[4] = effective_width() + canvas_border;
		spts->coords[5] = effective_height() + canvas_border;

	}

	size_t last_spt = ((npoints + 3) * 2) - 2;

	for (size_t i = 0; i < npoints; ++i) {

		double y = vec[i];
		
		pts->coords[i*2] = canvas_border + i;
		pts->coords[(i*2)+1] = y_coordinate (y);

		spts->coords[last_spt - (i*2)] = canvas_border + i;
		spts->coords[last_spt - (i*2) + 1] = pts->coords[(i*2)+1];
	}

	gnome_canvas_item_set (fade[current].line, "points", pts, NULL);
	gnome_canvas_item_set (fade[current].shading, "points", spts, NULL);

	gnome_canvas_points_unref (pts);
	gnome_canvas_points_unref (spts);

	for (vector<GnomeCanvasItem*>::iterator i = fade[current].waves.begin(); i != fade[current].waves.end(); ++i) {
		gnome_canvas_item_set ((*i), "gain_src", &fade[current].gain_curve, NULL);
	}
}

void
CrossfadeEditor::apply_preset (Preset *preset)
{
	for (list<Point*>::iterator i = fade[current].points.begin(); i != fade[current].points.end(); ++i) {
		delete *i;
	}

	fade[current].points.clear ();

	for (Preset::iterator i = preset->begin(); i != preset->end(); ++i) {
		Point* p = make_point ();
		p->move_to (x_coordinate ((*i).x), y_coordinate ((*i).y),
			    (*i).x, (*i).y);
		fade[current].points.push_back (p);
	}

	redraw ();
}

void
CrossfadeEditor::apply ()
{
	_apply_to (&xfade);
}

void
CrossfadeEditor::_apply_to (Crossfade* xf)
{
	ARDOUR::Curve& in (xf->fade_in());
	ARDOUR::Curve& out (xf->fade_out());

	/* IN */


	ARDOUR::Curve::const_iterator the_end = in.const_end();
	--the_end;

	double firstx = (*in.begin())->when;
	double endx = (*the_end)->when;
	double miny = in.get_min_y ();
	double maxy = in.get_max_y ();

	in.freeze ();
	in.clear ();

	for (list<Point*>::iterator i = fade[In].points.begin(); i != fade[In].points.end(); ++i) {

		double when = firstx + ((*i)->x * (endx - firstx));
		double value = (*i)->y; // miny + ((*i)->y * (maxy - miny));
		in.add (when, value);
	}

	/* OUT */

	the_end = out.const_end();
	--the_end;

	firstx = (*out.begin())->when;
	endx = (*the_end)->when;
	miny = out.get_min_y ();
	maxy = out.get_max_y ();

	out.freeze ();
	out.clear ();

	for (list<Point*>::iterator i = fade[Out].points.begin(); i != fade[Out].points.end(); ++i) {

		double when = firstx + ((*i)->x * (endx - firstx));
		double value = (*i)->y; // miny + ((*i)->y * (maxy - miny));
		out.add (when, value);
	}

	in.thaw ();
	out.thaw ();
}

void
CrossfadeEditor::setup (Crossfade* xfade)
{
	_apply_to (xfade);
	xfade->set_active (true);
	xfade->fade_in().solve ();
	xfade->fade_out().solve ();
}

void
CrossfadeEditor::clear ()
{
	for (list<Point*>::iterator i = fade[current].points.begin(); i != fade[current].points.end(); ++i) {
		delete *i;
	}

	fade[current].points.clear ();

	redraw ();
}

void
CrossfadeEditor::reset ()
{
	set (xfade.fade_in(),  In);
	set (xfade.fade_out(), Out);
}

void
CrossfadeEditor::build_presets ()
{
	Preset* p;

	fade_in_presets = new Presets;
	fade_out_presets = new Presets;

	/* FADE OUT */

	p = new Preset (hiin_xpm);
	p->push_back (PresetPoint (0, 0));
	p->push_back (PresetPoint (0.0207373, 0.197222));
	p->push_back (PresetPoint (0.0645161, 0.525));
	p->push_back (PresetPoint (0.152074, 0.802778));
	p->push_back (PresetPoint (0.276498, 0.919444));
	p->push_back (PresetPoint (0.481567, 0.980556));
	p->push_back (PresetPoint (0.767281, 1));
	p->push_back (PresetPoint (1, 1));
	fade_in_presets->push_back (p);
	
	p = new Preset (loin_xpm);
	p->push_back (PresetPoint (0, 0));
	p->push_back (PresetPoint (0.389401, 0.0333333));
	p->push_back (PresetPoint (0.629032, 0.0861111));
	p->push_back (PresetPoint (0.829493, 0.233333));
	p->push_back (PresetPoint (0.9447, 0.483333));
	p->push_back (PresetPoint (0.976959, 0.697222));
	p->push_back (PresetPoint (1, 1));
	fade_in_presets->push_back (p);

	p = new Preset (regin_xpm);
	p->push_back (PresetPoint (0, 0));
	p->push_back (PresetPoint (0.0737327, 0.308333));
	p->push_back (PresetPoint (0.246544, 0.658333));
	p->push_back (PresetPoint (0.470046, 0.886111));
	p->push_back (PresetPoint (0.652074, 0.972222));
	p->push_back (PresetPoint (0.771889, 0.988889));
	p->push_back (PresetPoint (1, 1));
	fade_in_presets->push_back (p);

	p = new Preset (regin2_xpm);
	p->push_back (PresetPoint (0, 0));
	p->push_back (PresetPoint (0.304147, 0.0694444));
	p->push_back (PresetPoint (0.529954, 0.152778));
	p->push_back (PresetPoint (0.725806, 0.333333));
	p->push_back (PresetPoint (0.847926, 0.558333));
	p->push_back (PresetPoint (0.919355, 0.730556));
	p->push_back (PresetPoint (1, 1));
	fade_in_presets->push_back (p);

	p = new Preset (linin_xpm);
	p->push_back (PresetPoint (0, 0));
	p->push_back (PresetPoint (1, 1));
	fade_in_presets->push_back (p);

	/* FADE OUT */

	p = new Preset (hiout_xpm);
	p->push_back (PresetPoint (0, 1));
	p->push_back (PresetPoint (0.305556, 1));
	p->push_back (PresetPoint (0.548611, 0.991736));
	p->push_back (PresetPoint (0.759259, 0.931129));
	p->push_back (PresetPoint (0.918981, 0.68595));
	p->push_back (PresetPoint (0.976852, 0.22865));
	p->push_back (PresetPoint (1, 0));
	fade_out_presets->push_back (p);
	
	p = new Preset (regout_xpm);
	p->push_back (PresetPoint (0, 1));
	p->push_back (PresetPoint (0.228111, 0.988889));
	p->push_back (PresetPoint (0.347926, 0.972222));
	p->push_back (PresetPoint (0.529954, 0.886111));
	p->push_back (PresetPoint (0.753456, 0.658333));
	p->push_back (PresetPoint (0.9262673, 0.308333));
	p->push_back (PresetPoint (1, 0));
	fade_out_presets->push_back (p);

	p = new Preset (loout_xpm);
	p->push_back (PresetPoint (0, 1));
	p->push_back (PresetPoint (0.023041, 0.697222));
	p->push_back (PresetPoint (0.0553,   0.483333));
	p->push_back (PresetPoint (0.170507, 0.233333));
	p->push_back (PresetPoint (0.370968, 0.0861111));
	p->push_back (PresetPoint (0.610599, 0.0333333));
	p->push_back (PresetPoint (1, 0));
	fade_out_presets->push_back (p);

	p = new Preset (regout2_xpm);
	p->push_back (PresetPoint (0, 1));
	p->push_back (PresetPoint (0.080645, 0.730556));
	p->push_back (PresetPoint (0.277778, 0.289256));
	p->push_back (PresetPoint (0.470046, 0.152778));
	p->push_back (PresetPoint (0.695853, 0.0694444));
	p->push_back (PresetPoint (1, 0));
	fade_out_presets->push_back (p);

	p = new Preset (linout_xpm);
	p->push_back (PresetPoint (0, 1));
	p->push_back (PresetPoint (1, 0));
	fade_out_presets->push_back (p);
}

void
CrossfadeEditor::curve_select_clicked (WhichFade wf)
{
	current = wf;

	if (wf == In) {

		for (vector<GnomeCanvasItem*>::iterator i = fade[In].waves.begin(); i != fade[In].waves.end(); ++i) {
			gnome_canvas_item_set ((*i), "wave_color", color_map[cSelectedCrossfadeEditorWave], NULL);
		}

		for (vector<GnomeCanvasItem*>::iterator i = fade[Out].waves.begin(); i != fade[Out].waves.end(); ++i) {
			gnome_canvas_item_set ((*i), "wave_color", color_map[cCrossfadeEditorWave], NULL);
		}

		gnome_canvas_item_set (fade[In].line, "fill_color_rgba", color_map[cSelectedCrossfadeEditorLine], NULL);
		gnome_canvas_item_set (fade[Out].line, "fill_color_rgba", color_map[cCrossfadeEditorLine], NULL);
		gnome_canvas_item_hide (fade[Out].shading);
		gnome_canvas_item_show (fade[In].shading);

		for (list<Point*>::iterator i = fade[Out].points.begin(); i != fade[Out].points.end(); ++i) {
			gnome_canvas_item_hide ((*i)->box);
		}

		for (list<Point*>::iterator i = fade[In].points.begin(); i != fade[In].points.end(); ++i) {
			gnome_canvas_item_show ((*i)->box);
		}

		for (vector<Button*>::iterator i = fade_out_buttons.begin(); i != fade_out_buttons.end(); ++i) {
			(*i)->set_sensitive (false);
		}

		for (vector<Button*>::iterator i = fade_in_buttons.begin(); i != fade_in_buttons.end(); ++i) {
			(*i)->set_sensitive (true);
		}

	} else {

		for (vector<GnomeCanvasItem*>::iterator i = fade[In].waves.begin(); i != fade[In].waves.end(); ++i) {
			gnome_canvas_item_set ((*i), "wave_color", color_map[cCrossfadeEditorWave], NULL);
		}

		for (vector<GnomeCanvasItem*>::iterator i = fade[Out].waves.begin(); i != fade[Out].waves.end(); ++i) {
			gnome_canvas_item_set ((*i), "wave_color", color_map[cSelectedCrossfadeEditorWave], NULL);
		}

		gnome_canvas_item_set (fade[Out].line, "fill_color_rgba", color_map[cSelectedCrossfadeEditorLine], NULL);
		gnome_canvas_item_set (fade[In].line, "fill_color_rgba", color_map[cCrossfadeEditorLine], NULL);
		gnome_canvas_item_hide (fade[In].shading);
		gnome_canvas_item_show (fade[Out].shading);

		for (list<Point*>::iterator i = fade[In].points.begin(); i != fade[In].points.end(); ++i) {
			gnome_canvas_item_hide ((*i)->box);
		}
		
		for (list<Point*>::iterator i = fade[Out].points.begin(); i != fade[Out].points.end(); ++i) {
			gnome_canvas_item_show ((*i)->box);
		}

		for (vector<Button*>::iterator i = fade_out_buttons.begin(); i != fade_out_buttons.end(); ++i) {
			(*i)->set_sensitive (true);
		}

		for (vector<Button*>::iterator i = fade_in_buttons.begin(); i != fade_in_buttons.end(); ++i) {
			(*i)->set_sensitive (false);
		}

	}
}

double 
CrossfadeEditor::x_coordinate (double& xfract) const
{
	xfract = min (1.0, xfract);
	xfract = max (0.0, xfract);
    
	return canvas_border + (xfract * effective_width());
}

double
CrossfadeEditor::y_coordinate (double& yfract) const
{
	yfract = min (1.0, yfract);
	yfract = max (0.0, yfract);

	return (_canvas->allocation.height - (canvas_border)) - (yfract * effective_height());
}

void
CrossfadeEditor::make_waves (AudioRegion& region, WhichFade which)
{
	gdouble ht;
	uint32_t nchans = region.n_channels();
	guint32 color;
	double spu;

	if (which == In) {
		color = color_map[cSelectedCrossfadeEditorWave];
	} else {
		color = color_map[cCrossfadeEditorWave];
	}

	ht = _canvas->allocation.height / (double) nchans;
	spu = xfade.length() / (double) effective_width();

	for (uint32_t n = 0; n < nchans; ++n) {
		
		gdouble yoff = n * ht;
		
		if (region.source(n).peaks_ready (bind (mem_fun(*this, &CrossfadeEditor::peaks_ready), &region, which))) {
			
			GnomeCanvasItem *wave = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS(_canvas)),
								   gnome_canvas_waveview_get_type (),
								   "data_src", (gpointer) &region,
								   "cache_updater", (gboolean) TRUE,
								   "cache", gnome_canvas_waveview_cache_new (),
								   "channel", (guint32) n,
								   "length_function", (gpointer) region_length_from_c,
								   "sourcefile_length_function", (gpointer) sourcefile_length_from_c,
								   "peak_function", (gpointer) region_read_peaks_from_c,
								   "gain_function", (gpointer) curve_get_vector_from_c,
								   "gain_src", &fade[which].gain_curve,
								   "x", (double) canvas_border,
								   "y", yoff,
								   "height", ht,
								   "samples_per_unit", spu,
								   "amplitude_above_axis", 2.0,
								   "wave_color", color,
								   NULL);
			
			gnome_canvas_item_lower_to_bottom (wave);
			fade[which].waves.push_back (wave);
		}
	}

	gnome_canvas_item_lower_to_bottom (toplevel);
}

void
CrossfadeEditor::peaks_ready (AudioRegion* r, WhichFade which)
{
	/* this should never be called, because the peak files for an xfade
	   will be ready by the time we want them. but our API forces us
	   to provide this, so ..
	*/

	make_waves (*r, which);
}

void
CrossfadeEditor::audition_both ()
{
	AudioPlaylist& pl (session.the_auditioner().prepare_playlist());
	jack_nframes_t preroll;
	jack_nframes_t postroll;
	jack_nframes_t length;
	jack_nframes_t left_start_offset;
	jack_nframes_t right_length;
	jack_nframes_t left_length;

	if (preroll_button.get_active()) {
		preroll = ARDOUR_UI::instance()->preroll_clock.current_duration ();
	} else {
		preroll = 0;
	}

	if (postroll_button.get_active()) {
		postroll = ARDOUR_UI::instance()->postroll_clock.current_duration ();
	} else {
		postroll = 0;
	}

 	if ((left_start_offset = xfade.out().length() - xfade.length()) >= preroll) {
  		left_start_offset -= preroll;
  	} 

	length = 0;

 	if ((left_length = xfade.length()) < xfade.out().length() - left_start_offset) {
  		length += postroll;
  	}

	right_length = xfade.length();

	if (xfade.in().length() - right_length < postroll) {
		right_length += postroll;
	}

	AudioRegion* left = new AudioRegion (xfade.out(), left_start_offset, left_length, "xfade out", 
					     0, Region::DefaultFlags, false);
	AudioRegion* right = new AudioRegion (xfade.in(), 0, right_length, "xfade in", 
					      0, Region::DefaultFlags, false);
	
	pl.add_region (*left, 0);
	pl.add_region (*right, 1+preroll);

	/* there is only one ... */

	pl.foreach_crossfade (this, &CrossfadeEditor::setup);

	session.audition_playlist ();
}

void
CrossfadeEditor::audition_left_dry ()
{
	AudioRegion* left = new AudioRegion (xfade.out(), xfade.out().length() - xfade.length(), xfade.length(), "xfade left", 
					     0, Region::DefaultFlags, false);
	
	session.audition_region (*left);
}

void
CrossfadeEditor::audition_left ()
{
	AudioPlaylist& pl (session.the_auditioner().prepare_playlist());

	AudioRegion* left = new AudioRegion (xfade.out(), xfade.out().length() - xfade.length(), xfade.length(), "xfade left", 
					     0, Region::DefaultFlags, false);
	AudioRegion* right = new AudioRegion (xfade.in(), 0, xfade.length(), "xfade in", 
					      0, Region::DefaultFlags, false);

	pl.add_region (*left, 0);
	pl.add_region (*right, 1);

	right->set_muted (true);

	/* there is only one ... */

	pl.foreach_crossfade (this, &CrossfadeEditor::setup);

	session.audition_playlist ();

	/* memory leak for regions */
}

void
CrossfadeEditor::audition_right_dry ()
{
	AudioRegion* right = new AudioRegion (xfade.in(), 0, xfade.length(), "xfade in", 
					      0, Region::DefaultFlags, false);
	session.audition_region (*right);
}

void
CrossfadeEditor::audition_right ()
{
	AudioPlaylist& pl (session.the_auditioner().prepare_playlist());

	AudioRegion* left = new AudioRegion (xfade.out(), xfade.out().length() - xfade.length(), xfade.length(), "xfade out", 
					     0, Region::DefaultFlags, false);
	AudioRegion* right = new AudioRegion (xfade.out(), 0, xfade.length(), "xfade out", 
					      0, Region::DefaultFlags, false);

	pl.add_region (*left, 0);
	pl.add_region (*right, 1);
	
	left->set_muted (true);

	/* there is only one ... */

	pl.foreach_crossfade (this, &CrossfadeEditor::setup);

	session.audition_playlist ();
}
	
void
CrossfadeEditor::cancel_audition ()
{
	session.cancel_audition ();
}

void
CrossfadeEditor::audition_toggled ()
{
	bool x;

	if ((x = audition_both_button.get_active ()) != session.is_auditioning()) {

		if (x) {
			audition_both ();
		} else {
			cancel_audition ();
		}
	}
}

void
CrossfadeEditor::audition_right_toggled ()
{
	bool x;
	
	if ((x = audition_right_button.get_active ()) != session.is_auditioning()) {

		if (x) {
			audition_right ();
		} else {
			cancel_audition ();
		}
	}
}

void
CrossfadeEditor::audition_right_dry_toggled ()
{
	bool x;

	if ((x = audition_right_dry_button.get_active ()) != session.is_auditioning()) {

		if (x) {
			audition_right_dry ();
		} else {
			cancel_audition ();
		}
	}
}

void
CrossfadeEditor::audition_left_toggled ()
{
	bool x;

	if ((x = audition_left_button.get_active ()) != session.is_auditioning()) {

		if (x) {
			audition_left ();
		} else {
			cancel_audition ();
		}
	}
}

void
CrossfadeEditor::audition_left_dry_toggled ()
{
	bool x;

	if ((x = audition_left_dry_button.get_active ()) != session.is_auditioning()) {
		
		if (x) {
			audition_left_dry ();
		} else {
			cancel_audition ();
		}
	}
}
