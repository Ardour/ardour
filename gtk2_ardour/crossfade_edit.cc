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

*/

#include <cmath>

#include <sigc++/bind.h>

#include <gtkmm/frame.h>
#include <gtkmm/image.h>
#include <gtkmm/scrolledwindow.h>

#include <libgnomecanvasmm/line.h>

#include <ardour/automation_event.h>
#include <ardour/curve.h>
#include <ardour/crossfade.h>
#include <ardour/session.h>
#include <ardour/auditioner.h>
#include <ardour/audioplaylist.h>
#include <ardour/audiosource.h>
#include <ardour/playlist_templates.h>
#include <ardour/region_factory.h>

#include <gtkmm2ext/gtk_ui.h>

#include "ardour_ui.h"
#include "crossfade_edit.h"
#include "rgb_macros.h"
#include "keyboard.h"
#include "utils.h"
#include "gui_thread.h"
#include "canvas_impl.h"
#include "simplerect.h"
#include "waveview.h"
#include "color.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace sigc;
using namespace Editing;

#include "i18n.h"

const int32_t CrossfadeEditor::Point::size = 7;
const double CrossfadeEditor::canvas_border = 10;
CrossfadeEditor::Presets* CrossfadeEditor::fade_in_presets = 0;
CrossfadeEditor::Presets* CrossfadeEditor::fade_out_presets = 0;

CrossfadeEditor::Half::Half ()
	: line (0), 
	  normative_curve (ParamID(GainAutomation), 0.0, 1.0, 1.0), // FIXME: GainAutomation?
	  gain_curve (ParamID(GainAutomation), 0.0, 2.0, 1.0)
{
}

CrossfadeEditor::CrossfadeEditor (Session& s, boost::shared_ptr<Crossfade> xf, double my, double mxy)
	: ArdourDialog (_("ardour: x-fade edit")),
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
	set_wmclass (X_("ardour_automationedit"), "Ardour");
	set_name ("CrossfadeEditWindow");
	set_position (Gtk::WIN_POS_MOUSE);

	add_events (Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK|Gdk::POINTER_MOTION_MASK);

	RadioButtonGroup sel_but_group = select_in_button.get_group();
	select_out_button.set_group (sel_but_group);
	select_out_button.set_mode (false);
	select_in_button.set_mode (false);

	get_action_area()->set_layout(BUTTONBOX_SPREAD);
	get_action_area()->pack_start(clear_button);
	get_action_area()->pack_start(revert_button);
	cancel_button = add_button ("Cancel", RESPONSE_CANCEL);
	ok_button = add_button ("OK", RESPONSE_ACCEPT);

	if (fade_in_presets == 0) {
		build_presets ();
	}

	point_grabbed = false;
	toplevel = 0;

	canvas = new ArdourCanvas::CanvasAA ();
	canvas->signal_size_allocate().connect (mem_fun(*this, &CrossfadeEditor::canvas_allocation));
	canvas->set_size_request (425, 200);
	
	toplevel = new ArdourCanvas::SimpleRect (*(canvas->root()));
	toplevel->property_x1() =  0.0;
	toplevel->property_y1() =  0.0;
	toplevel->property_x2() =  10.0;
	toplevel->property_y2() =  10.0;
	toplevel->property_fill() =  true;
	toplevel->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_CrossfadeEditorBase.get();
	toplevel->property_outline_pixels() =  0;
	toplevel->signal_event().connect (mem_fun (*this, &CrossfadeEditor::canvas_event));
	
	fade[Out].line = new ArdourCanvas::Line (*(canvas->root()));
	fade[Out].line->property_width_pixels() = 1;
	fade[Out].line->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_CrossfadeEditorLine.get();
		
	fade[Out].shading = new ArdourCanvas::Polygon (*(canvas->root()));
	fade[Out].shading->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_CrossfadeEditorLineShading.get();

	fade[In].line = new ArdourCanvas::Line (*(canvas->root()));
	fade[In].line->property_width_pixels() = 1;
	fade[In].line->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_CrossfadeEditorLine.get();
		
	fade[In].shading = new ArdourCanvas::Polygon (*(canvas->root()));
	fade[In].shading->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_CrossfadeEditorLineShading.get();
	
	fade[In].shading->signal_event().connect (mem_fun (*this, &CrossfadeEditor::canvas_event));
	fade[In].line->signal_event().connect (mem_fun (*this, &CrossfadeEditor::curve_event));
	fade[Out].shading->signal_event().connect (mem_fun (*this, &CrossfadeEditor::canvas_event));
	fade[Out].line->signal_event().connect (mem_fun (*this, &CrossfadeEditor::curve_event));

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

		pxmap = manage (new Image (::get_icon ((*i)->image_name)));
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

		pxmap = manage (new Image (::get_icon ((*i)->image_name)));
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
	ok_button->set_name ("CrossfadeEditButton");
	cancel_button->set_name ("CrossfadeEditButton");
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

	roll_box.pack_start (preroll_button, false, false);
	roll_box.pack_start (postroll_button, false, false);

	Gtk::HBox* rcenter_box = manage (new HBox);
	rcenter_box->pack_start (roll_box, true, false);

	VBox* vpacker2 = manage (new (VBox));

	vpacker2->set_border_width (12);
	vpacker2->set_spacing (7);
	vpacker2->pack_start (*acbox, false, false);
	vpacker2->pack_start (*rcenter_box, false, false);

	curve_button_box.set_spacing (7);
	curve_button_box.pack_start (fade_out_table, false, false, 12);
	curve_button_box.pack_start (*vpacker2, false, false, 12);
	curve_button_box.pack_start (fade_in_table, false, false, 12);
	
	get_vbox()->pack_start (*canvas_frame, true, true);
	get_vbox()->pack_start (curve_button_box, false, false);

	/* button to allow hackers to check the actual curve values */

//	Button* foobut = manage (new Button ("dump"));
//	foobut-.signal_clicked().connect (mem_fun(*this, &CrossfadeEditor::dump));
//	vpacker.pack_start (*foobut, false, false);

	current = In;
	set (xfade->fade_in(), In);

	current = Out;
	set (xfade->fade_out(), Out);

	curve_select_clicked (In);

	xfade->StateChanged.connect (mem_fun(*this, &CrossfadeEditor::xfade_changed));

	session.AuditionActive.connect (mem_fun(*this, &CrossfadeEditor::audition_state_changed));
	show_all_children();
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
CrossfadeEditor::set (const ARDOUR::AutomationList& curve, WhichFade which)
{
	double firstx, endx;
	ARDOUR::AutomationList::const_iterator the_end;

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

	for (ARDOUR::AutomationList::const_iterator i = curve.const_begin(); i != curve.const_end(); ++i) {
		
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

bool
CrossfadeEditor::curve_event (GdkEvent* event)
{
	/* treat it like a toplevel event */

	return canvas_event (event);
}

bool
CrossfadeEditor::point_event (GdkEvent* event, Point* point)
{

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

bool
CrossfadeEditor::canvas_event (GdkEvent* event)
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
	delete box;
}

CrossfadeEditor::Point*
CrossfadeEditor::make_point ()
{
	Point* p = new Point;

	p->box = new ArdourCanvas::SimpleRect (*(canvas->root()));
	p->box->property_fill() = true;
	p->box->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_CrossfadeEditorPointFill.get();
	p->box->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_CrossfadeEditorPointOutline.get();
	p->box->property_outline_pixels() = 1;

	p->curve = fade[current].line;

	p->box->signal_event().connect (bind (mem_fun (*this, &CrossfadeEditor::point_event), p));
	
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

	box->property_x1() = x1;
	box->property_x2() = x2;

	box->property_y1() = ny - half_size;
	box->property_y2() = ny + half_size;

	x = xfract;
	y = yfract;
}

void
CrossfadeEditor::canvas_allocation (Gtk::Allocation& alloc)
{
	if (toplevel) {
		toplevel->property_x1() = 0.0;
		toplevel->property_y1() = 0.0;
		toplevel->property_x2() = (double) canvas->get_allocation().get_width() + canvas_border;
		toplevel->property_y2() = (double) canvas->get_allocation().get_height() + canvas_border;
	}
	
	canvas->set_scroll_region (0.0, 0.0, 
				   canvas->get_allocation().get_width(), 
				   canvas->get_allocation().get_height());

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

	double spu = xfade->length() / (double) effective_width();

	if (fade[In].waves.empty()) {
		make_waves (xfade->in(), In);
	}

	if (fade[Out].waves.empty()) {
		make_waves (xfade->out(), Out);
	}

	double ht;
	vector<ArdourCanvas::WaveView*>::iterator i;
	uint32_t n;

	ht = canvas->get_allocation().get_height() / xfade->in()->n_channels();

	for (n = 0, i = fade[In].waves.begin(); i != fade[In].waves.end(); ++i, ++n) {
		double yoff;

		yoff = n * ht;

		(*i)->property_y() = yoff;
		(*i)->property_height() = ht;
		(*i)->property_samples_per_unit() = spu;
	}

	ht = canvas->get_allocation().get_height() / xfade->out()->n_channels();

	for (n = 0, i = fade[Out].waves.begin(); i != fade[Out].waves.end(); ++i, ++n) {
		double yoff;

		yoff = n * ht;

		(*i)->property_y() = yoff;
		(*i)->property_height() = ht;
		(*i)->property_samples_per_unit() = spu;
	}

}


void
CrossfadeEditor::xfade_changed (Change ignored)
{
	set (xfade->fade_in(), In);
	set (xfade->fade_out(), Out);
}

void
CrossfadeEditor::redraw ()
{
	if (canvas->get_allocation().get_width() < 2) {
		return;
	}

	nframes_t len = xfade->length ();

	fade[current].normative_curve.clear ();
	fade[current].gain_curve.clear ();

	for (list<Point*>::iterator i = fade[current].points.begin(); i != fade[current].points.end(); ++i) {
		fade[current].normative_curve.add ((*i)->x, (*i)->y);
		fade[current].gain_curve.add (((*i)->x * len), (*i)->y);
	}

	size_t npoints = (size_t) effective_width();
	float vec[npoints];

	fade[current].normative_curve.curve().get_vector (0, 1.0, vec, npoints);
	
	ArdourCanvas::Points pts;
	ArdourCanvas::Points spts;

	while (pts.size() < npoints) {
		pts.push_back (Gnome::Art::Point (0,0));
	}

	while (spts.size() < npoints + 3) {
		spts.push_back (Gnome::Art::Point (0,0));
	}

	/* the shade coordinates *MUST* be in anti-clockwise order.
	 */

	if (current == In) {

		/* lower left */

		spts[0].set_x (canvas_border);
		spts[0].set_y (effective_height() + canvas_border);

		/* lower right */

		spts[1].set_x (effective_width() + canvas_border);
		spts[1].set_y (effective_height() + canvas_border);

		/* upper right */

		spts[2].set_x (effective_width() + canvas_border);
		spts[2].set_y (canvas_border);

		
	} else {

		/*  upper left */
		
		spts[0].set_x (canvas_border);
		spts[0].set_y (canvas_border);

		/* lower left */

		spts[1].set_x (canvas_border);
		spts[1].set_y (effective_height() + canvas_border);

		/* lower right */

		spts[2].set_x (effective_width() + canvas_border);
		spts[2].set_y (effective_height() + canvas_border);

	}

	size_t last_spt = (npoints + 3) - 1;

	for (size_t i = 0; i < npoints; ++i) {

		double y = vec[i];
		
		pts[i].set_x (canvas_border + i);
		pts[i].set_y  (y_coordinate (y));

		spts[last_spt - i].set_x (canvas_border + i);
		spts[last_spt - i].set_y (pts[i].get_y());
	}

	fade[current].line->property_points() = pts;
	fade[current].shading->property_points() = spts;

	for (vector<ArdourCanvas::WaveView*>::iterator i = fade[current].waves.begin(); i != fade[current].waves.end(); ++i) {
		(*i)->property_gain_src() = &fade[current].gain_curve;
	}
}

void
CrossfadeEditor::apply_preset (Preset *preset)
{
  
	WhichFade wf =  find(fade_in_presets->begin(), fade_in_presets->end(), preset) != fade_in_presets->end() ? In : Out;
	
	if (current != wf) {
	  
	      	if (wf == In) {
			select_in_button.clicked();
		} else {
			select_out_button.clicked();
		}
		
		curve_select_clicked (wf);
	}  
	
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
	_apply_to (xfade);
}

void
CrossfadeEditor::_apply_to (boost::shared_ptr<Crossfade> xf)
{
	ARDOUR::AutomationList& in (xf->fade_in());
	ARDOUR::AutomationList& out (xf->fade_out());

	/* IN */


	ARDOUR::AutomationList::const_iterator the_end = in.const_end();
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
CrossfadeEditor::setup (boost::shared_ptr<Crossfade> xfade)
{
	_apply_to (xfade);
	xfade->set_active (true);
	xfade->fade_in().curve().solve ();
	xfade->fade_out().curve().solve ();
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
	set (xfade->fade_in(),  In);
	set (xfade->fade_out(), Out);

        curve_select_clicked (current);
}

void
CrossfadeEditor::build_presets ()
{
	Preset* p;

	fade_in_presets = new Presets;
	fade_out_presets = new Presets;

	/* FADE OUT */
	// p = new Preset ("hiin.xpm");
	p = new Preset ("crossfade_in_fast-cut");
	p->push_back (PresetPoint (0, 0));
	p->push_back (PresetPoint (0.0207373, 0.197222));
	p->push_back (PresetPoint (0.0645161, 0.525));
	p->push_back (PresetPoint (0.152074, 0.802778));
	p->push_back (PresetPoint (0.276498, 0.919444));
	p->push_back (PresetPoint (0.481567, 0.980556));
	p->push_back (PresetPoint (0.767281, 1));
	p->push_back (PresetPoint (1, 1));
	fade_in_presets->push_back (p);
	
	// p = new Preset ("loin.xpm");
	p = new Preset ("crossfade_in_transition");
	p->push_back (PresetPoint (0, 0));
	p->push_back (PresetPoint (0.389401, 0.0333333));
	p->push_back (PresetPoint (0.629032, 0.0861111));
	p->push_back (PresetPoint (0.829493, 0.233333));
	p->push_back (PresetPoint (0.9447, 0.483333));
	p->push_back (PresetPoint (0.976959, 0.697222));
	p->push_back (PresetPoint (1, 1));
	fade_in_presets->push_back (p);

	// p = new Preset ("regin.xpm");
	p = new Preset ("crossfade_in_constant");
	p->push_back (PresetPoint (0, 0));
	p->push_back (PresetPoint (0.0737327, 0.308333));
	p->push_back (PresetPoint (0.246544, 0.658333));
	p->push_back (PresetPoint (0.470046, 0.886111));
	p->push_back (PresetPoint (0.652074, 0.972222));
	p->push_back (PresetPoint (0.771889, 0.988889));
	p->push_back (PresetPoint (1, 1));
	fade_in_presets->push_back (p);

	// p = new Preset ("regin2.xpm");
	p = new Preset ("crossfade_in_slow-cut");
	p->push_back (PresetPoint (0, 0));
	p->push_back (PresetPoint (0.304147, 0.0694444));
	p->push_back (PresetPoint (0.529954, 0.152778));
	p->push_back (PresetPoint (0.725806, 0.333333));
	p->push_back (PresetPoint (0.847926, 0.558333));
	p->push_back (PresetPoint (0.919355, 0.730556));
	p->push_back (PresetPoint (1, 1));
	fade_in_presets->push_back (p);

	// p = new Preset ("linin.xpm");
	p = new Preset ("crossfade_in_dipped");
	p->push_back (PresetPoint (0, 0));
	p->push_back (PresetPoint (1, 1));
	fade_in_presets->push_back (p);

	p = new Preset ("crossfade_in_default");
	p->push_back (PresetPoint (0, 0));
	p->push_back (PresetPoint (0.1, 0.01));
	p->push_back (PresetPoint (0.2, 0.03));
	p->push_back (PresetPoint (0.8, 0.97));
	p->push_back (PresetPoint (0.9, 0.99));
	p->push_back (PresetPoint (1, 1));
	fade_in_presets->push_back (p);
	
	/* FADE OUT */

	// p = new Preset ("hiout.xpm");
	p = new Preset ("crossfade_out_fast-cut");
	p->push_back (PresetPoint (0, 1));
	p->push_back (PresetPoint (0.305556, 1));
	p->push_back (PresetPoint (0.548611, 0.991736));
	p->push_back (PresetPoint (0.759259, 0.931129));
	p->push_back (PresetPoint (0.918981, 0.68595));
	p->push_back (PresetPoint (0.976852, 0.22865));
	p->push_back (PresetPoint (1, 0));
	fade_out_presets->push_back (p);
	
	// p = new Preset ("regout.xpm");
	p = new Preset ("crossfade_out_constant");
	p->push_back (PresetPoint (0, 1));
	p->push_back (PresetPoint (0.228111, 0.988889));
	p->push_back (PresetPoint (0.347926, 0.972222));
	p->push_back (PresetPoint (0.529954, 0.886111));
	p->push_back (PresetPoint (0.753456, 0.658333));
	p->push_back (PresetPoint (0.9262673, 0.308333));
	p->push_back (PresetPoint (1, 0));
	fade_out_presets->push_back (p);

	// p = new Preset ("loout.xpm");
	p = new Preset ("crossfade_out_transition");
	p->push_back (PresetPoint (0, 1));
	p->push_back (PresetPoint (0.023041, 0.697222));
	p->push_back (PresetPoint (0.0553,   0.483333));
	p->push_back (PresetPoint (0.170507, 0.233333));
	p->push_back (PresetPoint (0.370968, 0.0861111));
	p->push_back (PresetPoint (0.610599, 0.0333333));
	p->push_back (PresetPoint (1, 0));
	fade_out_presets->push_back (p);

	// p = new Preset ("regout2.xpm");
	p = new Preset ("crossfade_out_slow-fade");
	p->push_back (PresetPoint (0, 1));
	p->push_back (PresetPoint (0.080645, 0.730556));
	p->push_back (PresetPoint (0.277778, 0.289256));
	p->push_back (PresetPoint (0.470046, 0.152778));
	p->push_back (PresetPoint (0.695853, 0.0694444));
	p->push_back (PresetPoint (1, 0));
	fade_out_presets->push_back (p);

	// p = new Preset ("linout.xpm");
	p = new Preset ("crossfade_out_dipped");
	p->push_back (PresetPoint (0, 1));
	p->push_back (PresetPoint (1, 0));
	fade_out_presets->push_back (p);
	
	p = new Preset ("crossfade_out_default");
	p->push_back (PresetPoint (0, 1));
	p->push_back (PresetPoint (0.1, 0.99));
	p->push_back (PresetPoint (0.2, 0.97));
	p->push_back (PresetPoint (0.8, 0.03));
	p->push_back (PresetPoint (0.9, 0.01));
	p->push_back (PresetPoint (1, 0));
	fade_out_presets->push_back (p);
}

void
CrossfadeEditor::curve_select_clicked (WhichFade wf)
{
	current = wf;
	
	if (wf == In) {
		
		for (vector<ArdourCanvas::WaveView*>::iterator i = fade[In].waves.begin(); i != fade[In].waves.end(); ++i) {
			(*i)->property_wave_color() = ARDOUR_UI::config()->canvasvar_SelectedCrossfadeEditorWave.get();
		}

		for (vector<ArdourCanvas::WaveView*>::iterator i = fade[Out].waves.begin(); i != fade[Out].waves.end(); ++i) {
			(*i)->property_wave_color() = ARDOUR_UI::config()->canvasvar_CrossfadeEditorWave.get();
		}

		fade[In].line->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_SelectedCrossfadeEditorLine.get();
		fade[Out].line->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_CrossfadeEditorLine.get();
		fade[Out].shading->hide();
		fade[In].shading->show();

		for (list<Point*>::iterator i = fade[Out].points.begin(); i != fade[Out].points.end(); ++i) {
			(*i)->box->hide();
		}

		for (list<Point*>::iterator i = fade[In].points.begin(); i != fade[In].points.end(); ++i) {
			(*i)->box->show ();
		}

	} else {

		for (vector<ArdourCanvas::WaveView*>::iterator i = fade[In].waves.begin(); i != fade[In].waves.end(); ++i) {
			(*i)->property_wave_color() = ARDOUR_UI::config()->canvasvar_CrossfadeEditorWave.get();
		}

		for (vector<ArdourCanvas::WaveView*>::iterator i = fade[Out].waves.begin(); i != fade[Out].waves.end(); ++i) {
			(*i)->property_wave_color() = ARDOUR_UI::config()->canvasvar_SelectedCrossfadeEditorWave.get();
		}

		fade[Out].line->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_SelectedCrossfadeEditorLine.get();
		fade[In].line->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_CrossfadeEditorLine.get();
		fade[In].shading->hide();
		fade[Out].shading->show();

		for (list<Point*>::iterator i = fade[In].points.begin(); i != fade[In].points.end(); ++i) {
			(*i)->box->hide();
		}
		
		for (list<Point*>::iterator i = fade[Out].points.begin(); i != fade[Out].points.end(); ++i) {
			(*i)->box->show();
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

	return (canvas->get_allocation().get_height() - (canvas_border)) - (yfract * effective_height());
}

void
CrossfadeEditor::make_waves (boost::shared_ptr<AudioRegion> region, WhichFade which)
{
	gdouble ht;
	uint32_t nchans = region->n_channels();
	guint32 color;
	double spu;

	if (which == In) {
		color = ARDOUR_UI::config()->canvasvar_SelectedCrossfadeEditorWave.get();
	} else {
		color = ARDOUR_UI::config()->canvasvar_CrossfadeEditorWave.get();
	}

	ht = canvas->get_allocation().get_height() / (double) nchans;
	spu = xfade->length() / (double) effective_width();

	for (uint32_t n = 0; n < nchans; ++n) {
		
		gdouble yoff = n * ht;
		
		if (region->audio_source(n)->peaks_ready (bind (mem_fun(*this, &CrossfadeEditor::peaks_ready), region, which), peaks_ready_connection)) {
			WaveView* waveview = new WaveView (*(canvas->root()));

			waveview->property_data_src() = region.get();
			waveview->property_cache_updater() =  true;
			waveview->property_cache() = WaveView::create_cache();
			waveview->property_channel() = n;
			waveview->property_length_function() = (void*) region_length_from_c;
			waveview->property_sourcefile_length_function() = (void*) sourcefile_length_from_c;
			waveview->property_peak_function() = (void*) region_read_peaks_from_c;
			waveview->property_gain_function() = (void*) curve_get_vector_from_c;
			waveview->property_gain_src() = &fade[which].gain_curve;
			waveview->property_x() = canvas_border;
			waveview->property_y() = yoff;
			waveview->property_height() = ht;
			waveview->property_samples_per_unit() = spu;
			waveview->property_amplitude_above_axis() = 2.0;
			waveview->property_wave_color() = color;
			
			waveview->lower_to_bottom();
			fade[which].waves.push_back (waveview);
		}
	}

	toplevel->lower_to_bottom();
}

void
CrossfadeEditor::peaks_ready (boost::shared_ptr<AudioRegion> r, WhichFade which)
{
	/* this should never be called, because the peak files for an xfade
	   will be ready by the time we want them. but our API forces us
	   to provide this, so ..
	*/
	peaks_ready_connection.disconnect ();
	make_waves (r, which);
}

void
CrossfadeEditor::audition_both ()
{
	AudioPlaylist& pl (session.the_auditioner()->prepare_playlist());
	nframes_t preroll;
	nframes_t postroll;
	nframes_t length;
	nframes_t left_start_offset;
	nframes_t right_length;
	nframes_t left_length;

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

 	if ((left_start_offset = xfade->out()->length() - xfade->length()) >= preroll) {
  		left_start_offset -= preroll;
  	} 

	length = 0;

 	if ((left_length = xfade->length()) < xfade->out()->length() - left_start_offset) {
  		length += postroll;
  	}

	right_length = xfade->length();

	if (xfade->in()->length() - right_length < postroll) {
		right_length += postroll;
	}

	boost::shared_ptr<AudioRegion> left (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (xfade->out(), left_start_offset, left_length, "xfade out", 
													      0, Region::DefaultFlags, false)));
	boost::shared_ptr<AudioRegion> right (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (xfade->in(), 0, right_length, "xfade in", 
													       0, Region::DefaultFlags, false)));
	
	pl.add_region (left, 0);
	pl.add_region (right, 1+preroll);

	/* there is only one ... */

	pl.foreach_crossfade (this, &CrossfadeEditor::setup);

	session.audition_playlist ();
}

void
CrossfadeEditor::audition_left_dry ()
{
	boost::shared_ptr<AudioRegion> left (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (xfade->out(), xfade->out()->length() - xfade->length(), xfade->length(), "xfade left", 
													      0, Region::DefaultFlags, false)));
	
	session.audition_region (left);
}

void
CrossfadeEditor::audition_left ()
{
	AudioPlaylist& pl (session.the_auditioner()->prepare_playlist());

	boost::shared_ptr<AudioRegion> left (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (xfade->out(), xfade->out()->length() - xfade->length(), xfade->length(), "xfade left", 
													      0, Region::DefaultFlags, false)));
	boost::shared_ptr<AudioRegion> right (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (xfade->in(), 0, xfade->length(), "xfade in", 
													       0, Region::DefaultFlags, false)));

	pl.add_region (left, 0);
	pl.add_region (right, 1);

	right->set_muted (true);

	/* there is only one ... */

	pl.foreach_crossfade (this, &CrossfadeEditor::setup);

	session.audition_playlist ();

	/* memory leak for regions */
}

void
CrossfadeEditor::audition_right_dry ()
{
	boost::shared_ptr<AudioRegion> right (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (xfade->in(), 0, xfade->length(), "xfade in", 
													       0, Region::DefaultFlags, false)));
	session.audition_region (right);
}

void
CrossfadeEditor::audition_right ()
{
	AudioPlaylist& pl (session.the_auditioner()->prepare_playlist());

	boost::shared_ptr<AudioRegion> left (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (xfade->out(), xfade->out()->length() - xfade->length(), xfade->length(), "xfade out", 
													      0, Region::DefaultFlags, false)));
					     boost::shared_ptr<AudioRegion> right (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (xfade->out(), 0, xfade->length(), "xfade out", 
													       0, Region::DefaultFlags, false)));

	pl.add_region (left, 0);
	pl.add_region (right, 1);
	
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
