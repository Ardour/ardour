/*
    Copyright (C) 2012 Paul Davis
    Author: Robin Gareus

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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <map>
#include <sigc++/bind.h>

#include <gtkmm/accelmap.h>

#include <glibmm/threads.h>

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/window_title.h>

#include "ardour/debug.h"
#include "ardour/midi_track.h"
#include "ardour/route_group.h"
#include "ardour/session.h"

#include "ardour/audio_track.h"
#include "ardour/midi_track.h"

#include "meterbridge.h"

#include "keyboard.h"
#include "monitor_section.h"
#include "public_editor.h"
#include "ardour_ui.h"
#include "utils.h"
#include "route_sorter.h"
#include "actions.h"
#include "gui_thread.h"
#include "global_signals.h"
#include "meter_patterns.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;
using namespace ArdourMeter;

using PBD::atoi;

Meterbridge* Meterbridge::_instance = 0;

Meterbridge*
Meterbridge::instance ()
{
	if (!_instance) {
		_instance  = new Meterbridge;
	}

	return _instance;
}

/* copy from gtk2_ardour/mixer_ui.cc -- TODO consolidate
 * used by Meterbridge::set_session() below
 */
struct SignalOrderRouteSorter {
	bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
		if (a->is_master() || a->is_monitor()) {
			/* "a" is a special route (master, monitor, etc), and comes
			 * last in the mixer ordering
			 */
			return false;
		} else if (b->is_master() || b->is_monitor()) {
			/* everything comes before b */
			return true;
		}
		return a->order_key () < b->order_key ();
	}
};

Meterbridge::Meterbridge ()
	: Window (Gtk::WINDOW_TOPLEVEL)
	, VisibilityTracker (*((Gtk::Window*) this))
	, _visible (false)
	, _show_busses (false)
	, metrics_left (1, MeterPeak)
	, metrics_right (2, MeterPeak)
	, cur_max_width (-1)
{
	set_name ("Meter Bridge");

	m_width = default_width;
	m_height = default_height;
	m_root_x = 1;
	m_root_y = 1;

	update_title ();

	set_wmclass (X_("ardour_mixer"), PROGRAM_NAME);

	Gdk::Geometry geom;
	geom.max_width = 1<<16;
	geom.max_height = max_height;
	geom.height_inc = 16;
	geom.width_inc = 1;
	set_geometry_hints(*((Gtk::Window*) this), geom, Gdk::HINT_MAX_SIZE | Gdk::HINT_RESIZE_INC);

	set_keep_above (true);
	set_border_width (0);

	metrics_vpacker_left.pack_start (metrics_left, true, true);
	metrics_vpacker_left.pack_start (metrics_spacer_left, false, false);
	metrics_spacer_left.set_size_request(-1, 0);
	metrics_spacer_left.set_spacing(0);

	metrics_vpacker_right.pack_start (metrics_right, true, true);
	metrics_vpacker_right.pack_start (metrics_spacer_right, false, false);
	metrics_spacer_right.set_size_request(-1, 0);
	metrics_spacer_right.set_spacing(0);

	signal_delete_event().connect (sigc::mem_fun (*this, &Meterbridge::hide_window));
	signal_configure_event().connect (sigc::mem_fun (*ARDOUR_UI::instance(), &ARDOUR_UI::configure_handler));
	Route::SyncOrderKeys.connect (*this, invalidator (*this), boost::bind (&Meterbridge::sync_order_keys, this), gui_context());
	MeterStrip::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&Meterbridge::remove_strip, this, _1), gui_context());
	MeterStrip::MetricChanged.connect (*this, invalidator (*this), boost::bind(&Meterbridge::resync_order, this), gui_context());
	MeterStrip::ConfigurationChanged.connect (*this, invalidator (*this), boost::bind(&Meterbridge::queue_resize, this), gui_context());

	/* work around ScrolledWindowViewport alignment mess Part one */
	Gtk::HBox * yspc = manage (new Gtk::HBox());
	yspc->set_size_request(-1, 1);
	Gtk::VBox * xspc = manage (new Gtk::VBox());
	xspc->pack_start(meterarea, true, true);
	xspc->pack_start(*yspc, false, false);
	yspc->show();
	xspc->show();

	meterarea.set_spacing(0);
	scroller.set_shadow_type(Gtk::SHADOW_NONE);
	scroller.set_border_width(0);
	scroller.add (*xspc);
	scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_NEVER);

	global_hpacker.pack_start (metrics_vpacker_left, false, false);
	global_hpacker.pack_start (scroller, true, true);
	global_hpacker.pack_start (metrics_vpacker_right, false, false);

	global_vpacker.pack_start (global_hpacker, true, true);
	add (global_vpacker);

	metrics_left.show();
	metrics_right.show();

	metrics_vpacker_left.show();
	metrics_spacer_left.show();
	metrics_vpacker_right.show();
	metrics_spacer_right.show();

	meterarea.show();
	global_vpacker.show();
	global_hpacker.show();
	scroller.show();

	/* the return of the ScrolledWindowViewport mess:
	 * remove shadow from scrollWindow's viewport
	 * see http://www.mail-archive.com/gtkmm-list@gnome.org/msg03509.html
	 */
	Gtk::Viewport* viewport = (Gtk::Viewport*) scroller.get_child();
	viewport->set_shadow_type(Gtk::SHADOW_NONE);
	viewport->set_border_width(0);

	UI::instance()->theme_changed.connect (sigc::mem_fun(*this, &Meterbridge::on_theme_changed));
	ColorsChanged.connect (sigc::mem_fun (*this, &Meterbridge::on_theme_changed));
	DPIReset.connect (sigc::mem_fun (*this, &Meterbridge::on_theme_changed));
}

Meterbridge::~Meterbridge ()
{
	while (_metrics.size() > 0) {
		delete (_metrics.back());
		_metrics.pop_back();
	}
}

void
Meterbridge::show_window ()
{
	present();
	if (!_visible) {
		set_window_pos_and_size ();
	}
	_visible = true;
}

/* code duplicated from gtk2_ardour/mixer_ui.cc  Mixer_UI::update_title() */
void
Meterbridge::update_title ()
{
	if (_session) {
		string n;

		if (_session->snap_name() != _session->name()) {
			n = _session->snap_name ();
		} else {
			n = _session->name ();
		}

		if (_session->dirty ()) {
			n = "*" + n;
		}

		WindowTitle title (n);
		title += S_("Window|Meterbridge");
		title += Glib::get_application_name ();
		set_title (title.get_string());

	} else {

		WindowTitle title (S_("Window|Meterbridge"));
		title += Glib::get_application_name ();
		set_title (title.get_string());
	}
}

void
Meterbridge::set_window_pos_and_size ()
{
	resize (m_width, m_height);
	if (m_root_x >= 0 && m_root_y >= 0) {
		move (m_root_x, m_root_y);
	}
}

void
Meterbridge::get_window_pos_and_size ()
{
	get_position(m_root_x, m_root_y);
	get_size(m_width, m_height);
}

bool
Meterbridge::hide_window (GdkEventAny *ev)
{
	if (!_visible) return 0;
	get_window_pos_and_size();
	_visible = false;
	return just_hide_it(ev, static_cast<Gtk::Window *>(this));
}

bool
Meterbridge::on_key_press_event (GdkEventKey* ev)
{
	if (gtk_window_propagate_key_event (GTK_WINDOW(gobj()), ev)) {
		return true;
	}
	return forward_key_press (ev);
}

bool
Meterbridge::on_key_release_event (GdkEventKey* ev)
{
	if (gtk_window_propagate_key_event (GTK_WINDOW(gobj()), ev)) {
		return true;
	}
	/* don't forward releases */
	return true;
}

bool
Meterbridge::on_scroll_event (GdkEventScroll* ev)
{
	switch (ev->direction) {
	case GDK_SCROLL_LEFT:
		scroll_left ();
		return true;
	case GDK_SCROLL_UP:
		if (ev->state & Keyboard::TertiaryModifier) {
			scroll_left ();
			return true;
		}
		return false;

	case GDK_SCROLL_RIGHT:
		scroll_right ();
		return true;

	case GDK_SCROLL_DOWN:
		if (ev->state & Keyboard::TertiaryModifier) {
			scroll_right ();
			return true;
		}
		return false;
	}

	return false;
}

void
Meterbridge::scroll_left ()
{
	if (!scroller.get_hscrollbar()) return;
	Adjustment* adj = scroller.get_hscrollbar()->get_adjustment();
	/* stupid GTK: can't rely on clamping across versions */
	scroller.get_hscrollbar()->set_value (max (adj->get_lower(), adj->get_value() - adj->get_step_increment()));
}

void
Meterbridge::scroll_right ()
{
	if (!scroller.get_hscrollbar()) return;
	Adjustment* adj = scroller.get_hscrollbar()->get_adjustment();
	/* stupid GTK: can't rely on clamping across versions */
	scroller.get_hscrollbar()->set_value (min (adj->get_upper(), adj->get_value() + adj->get_step_increment()));
}

void
Meterbridge::on_size_request (Gtk::Requisition* r)
{
	meter_clear_pattern_cache(3);
	Gtk::Window::on_size_request(r);

	Gdk::Geometry geom;
	Gtk::Requisition mr = meterarea.size_request();

	geom.max_width = mr.width + metrics_left.get_width() + metrics_right.get_width();
	geom.max_height = max_height;

#ifndef GTKOSX
	/* on OSX this leads to a constant live-loop: show/hide scrollbar
	 * on Linux, the window is resized IFF the scrollbar was not visible
	 */
	const Gtk::Scrollbar * hsc = scroller.get_hscrollbar();
	Glib::RefPtr<Gdk::Screen> screen = get_screen ();
	Gdk::Rectangle monitor_rect;
	screen->get_monitor_geometry (0, monitor_rect);
	const int scr_w = monitor_rect.get_width() - 44;

	if (cur_max_width < geom.max_width
			&& cur_max_width < scr_w
			&& !(scroller.get_hscrollbar_visible() && hsc)) {
		int h = r->height;
		*r = Gtk::Requisition();
		r->width = geom.max_width;
		r->height = h;
	}
#endif

	if (cur_max_width != geom.max_width) {
		cur_max_width = geom.max_width;
		geom.height_inc = 16;
		geom.width_inc = 1;
		set_geometry_hints(*((Gtk::Window*) this), geom, Gdk::HINT_MAX_SIZE | Gdk::HINT_RESIZE_INC);
	}
}

void
Meterbridge::on_size_allocate (Gtk::Allocation& a)
{
	const Gtk::Scrollbar * hsc = scroller.get_hscrollbar();

	if (scroller.get_hscrollbar_visible() && hsc) {
		if (!scroll_connection.connected()) {
			scroll_connection = scroller.get_hscrollbar()->get_adjustment()->signal_value_changed().connect(sigc::mem_fun (*this, &Meterbridge::on_scroll));
			scroller.get_hscrollbar()->get_adjustment()->signal_changed().connect(sigc::mem_fun (*this, &Meterbridge::on_scroll));
		}
		gint scrollbar_spacing;
		gtk_widget_style_get (GTK_WIDGET (scroller.gobj()),
				"scrollbar-spacing", &scrollbar_spacing, NULL);
		const int h = hsc->get_height() + scrollbar_spacing + 1;
		metrics_spacer_left.set_size_request(-1, h);
		metrics_spacer_right.set_size_request(-1, h);
	} else {
		metrics_spacer_left.set_size_request(-1, 0);
		metrics_spacer_right.set_size_request(-1, 0);
	}
	Gtk::Window::on_size_allocate(a);
}

void
Meterbridge::on_scroll()
{
	if (!scroller.get_hscrollbar()) return;

	Adjustment* adj = scroller.get_hscrollbar()->get_adjustment();
	int leftend = adj->get_value();
	int rightend = scroller.get_width() + leftend;

	int mm_left = _mm_left;
	int mm_right = _mm_right;
	ARDOUR::MeterType mt_left = _mt_left;
	ARDOUR::MeterType mt_right = _mt_right;

	for (unsigned int i = 0; i < _metrics.size(); ++i) {
		int sx, dx, dy;
		int mm = _metrics[i]->get_metric_mode();
		sx = (mm & 2) ? _metrics[i]->get_width() : 0;

		_metrics[i]->translate_coordinates(meterarea, sx, 0, dx, dy);

		if (dx < leftend && !(mm&2)) {
			mm_left = mm;
			mt_left = _metrics[i]->meter_type();
		}
		if (dx > rightend && (mm&2)) {
			mm_right = mm;
			mt_right = _metrics[i]->meter_type();
			break;
		}
	}
	metrics_left.set_metric_mode(mm_left, mt_left);
	metrics_right.set_metric_mode(mm_right, mt_right);
}

void
Meterbridge::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		return;
	}

	metrics_left.set_session(s);
	metrics_right.set_session(s);

	XMLNode* node = _session->instant_xml(X_("Meterbridge"));
	if (node) {
		set_state (*node);
	}

	update_title ();
	_show_busses = _session->config.get_show_busses_on_meterbridge();
	_show_master = _session->config.get_show_master_on_meterbridge();
	_show_midi = _session->config.get_show_midi_on_meterbridge();

	SignalOrderRouteSorter sorter;
	boost::shared_ptr<RouteList> routes = _session->get_routes();

	RouteList copy(*routes);
	copy.sort(sorter);
	add_strips(copy);

	_session->RouteAdded.connect (_session_connections, invalidator (*this), boost::bind (&Meterbridge::add_strips, this, _1), gui_context());
	_session->DirtyChanged.connect (_session_connections, invalidator (*this), boost::bind (&Meterbridge::update_title, this), gui_context());
	_session->StateSaved.connect (_session_connections, invalidator (*this), boost::bind (&Meterbridge::update_title, this), gui_context());
	_session->config.ParameterChanged.connect (*this, invalidator (*this), ui_bind (&Meterbridge::parameter_changed, this, _1), gui_context());
	Config->ParameterChanged.connect (*this, invalidator (*this), ui_bind (&Meterbridge::parameter_changed, this, _1), gui_context());

	if (_visible) {
		show_window();
		ActionManager::check_toggleaction ("<Actions>/Common/toggle-meterbridge");
	}
	start_updating ();
}

void
Meterbridge::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &Meterbridge::session_going_away);

	for (list<MeterBridgeStrip>::iterator i = strips.begin(); i != strips.end(); ++i) {
		delete ((*i).s);
	}

	strips.clear ();
	stop_updating ();

	SessionHandlePtr::session_going_away ();

	_session = 0;
	update_title ();
}

int
Meterbridge::set_state (const XMLNode& node)
{
	const XMLProperty* prop;
	XMLNode* geometry;

	m_width = default_width;
	m_height = default_height;
	m_root_x = 1;
	m_root_y = 1;

	if ((geometry = find_named_node (node, "geometry")) != 0) {

		XMLProperty* prop;

		if ((prop = geometry->property("x_size")) == 0) {
			prop = geometry->property ("x-size");
		}
		if (prop) {
			m_width = atoi(prop->value());
		}
		if ((prop = geometry->property("y_size")) == 0) {
			prop = geometry->property ("y-size");
		}
		if (prop) {
			m_height = atoi(prop->value());
		}

		if ((prop = geometry->property ("x_pos")) == 0) {
			prop = geometry->property ("x-pos");
		}
		if (prop) {
			m_root_x = atoi (prop->value());

		}
		if ((prop = geometry->property ("y_pos")) == 0) {
			prop = geometry->property ("y-pos");
		}
		if (prop) {
			m_root_y = atoi (prop->value());
		}
	}

	set_window_pos_and_size ();

	if ((prop = node.property ("show-meterbridge"))) {
		if (string_is_affirmative (prop->value())) {
		       _visible = true;
		}
	}

	return 0;
}

XMLNode&
Meterbridge::get_state (void)
{
	char buf[32];
	XMLNode* node = new XMLNode ("Meterbridge");

	if (is_realized() && _visible) {
		get_window_pos_and_size ();
	}

	XMLNode* geometry = new XMLNode ("geometry");
	snprintf(buf, sizeof(buf), "%d", m_width);
	geometry->add_property(X_("x_size"), string(buf));
	snprintf(buf, sizeof(buf), "%d", m_height);
	geometry->add_property(X_("y_size"), string(buf));
	snprintf(buf, sizeof(buf), "%d", m_root_x);
	geometry->add_property(X_("x_pos"), string(buf));
	snprintf(buf, sizeof(buf), "%d", m_root_y);
	geometry->add_property(X_("y_pos"), string(buf));
	node->add_child_nocopy (*geometry);

	node->add_property ("show-meterbridge", _visible ? "yes" : "no");
	return *node;
}


gint
Meterbridge::start_updating ()
{
	fast_screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect (sigc::mem_fun(*this, &Meterbridge::fast_update_strips));
	return 0;
}

gint
Meterbridge::stop_updating ()
{
	fast_screen_update_connection.disconnect();
	return 0;
}

void
Meterbridge::fast_update_strips ()
{
	if (!is_mapped () || !_session) {
		return;
	}
	for (list<MeterBridgeStrip>::iterator i = strips.begin(); i != strips.end(); ++i) {
		if (!(*i).visible) continue;
		(*i).s->fast_update ();
	}
}

void
Meterbridge::add_strips (RouteList& routes)
{
	MeterStrip* strip;
	for (RouteList::iterator x = routes.begin(); x != routes.end(); ++x) {
		boost::shared_ptr<Route> route = (*x);
		if (route->is_auditioner()) {
			continue;
		}
		if (route->is_monitor()) {
			continue;
		}

		strip = new MeterStrip (_session, route);
		strips.push_back (MeterBridgeStrip(strip));
		route->active_changed.connect (*this, invalidator (*this), boost::bind (&Meterbridge::resync_order, this), gui_context ());

		meterarea.pack_start (*strip, false, false);
		strip->show();
	}

	resync_order();
}

void
Meterbridge::remove_strip (MeterStrip* strip)
{
	if (_session && _session->deletion_in_progress()) {
		return;
	}

	list<MeterBridgeStrip>::iterator i;
	for (list<MeterBridgeStrip>::iterator i = strips.begin(); i != strips.end(); ++i) {
		if ( (*i).s == strip) {
			strips.erase (i);
			break;
		}
	}

	resync_order();
}

void
Meterbridge::sync_order_keys ()
{
	Glib::Threads::Mutex::Lock lm (_resync_mutex);

	MeterOrderRouteSorter sorter;
	strips.sort(sorter);

	int pos = 0;
	int vis = 0;
	MeterStrip * last = 0;

	unsigned int metrics = 0;
	MeterType lmt = MeterPeak;
	bool have_midi = false;
	metrics_left.set_metric_mode(1, lmt);

	for (list<MeterBridgeStrip>::iterator i = strips.begin(); i != strips.end(); ++i) {

		if (! (*i).s->route()->active()) {
			(*i).s->hide();
			(*i).visible = false;
		}
		else if ((*i).s->route()->is_master()) {
			if (_show_master) {
				(*i).s->show();
				(*i).visible = true;
				vis++;
			} else {
				(*i).s->hide();
				(*i).visible = false;
			}
		}
		else if (boost::dynamic_pointer_cast<AudioTrack>((*i).s->route()) == 0
				&& boost::dynamic_pointer_cast<MidiTrack>((*i).s->route()) == 0
				) {
			/* non-master bus */
			if (_show_busses) {
				(*i).s->show();
				(*i).visible = true;
				vis++;
			} else {
				(*i).s->hide();
				(*i).visible = false;
			}
		}
		else if (boost::dynamic_pointer_cast<MidiTrack>((*i).s->route())) {
			if (_show_midi) {
				(*i).s->show();
				(*i).visible = true;
				vis++;
			} else {
				(*i).s->hide();
				(*i).visible = false;
			}
		}
		else {
			(*i).s->show();
			(*i).visible = true;
				vis++;
		}

		(*i).s->set_tick_bar(0);

		MeterType nmt = (*i).s->meter_type();
		if (nmt == MeterKrms) nmt = MeterPeak; // identical metrics
		if (vis == 1) {
			(*i).s->set_tick_bar(1);
		}

		if ((*i).visible && nmt != lmt && vis == 1) {
			lmt = nmt;
			metrics_left.set_metric_mode(1, lmt);
		} else if ((*i).visible && nmt != lmt) {

			if (last) {
				last->set_tick_bar(last->get_tick_bar() | 2);
			}
			(*i).s->set_tick_bar((*i).s->get_tick_bar() | 1);

			if (_metrics.size() <= metrics) {
				_metrics.push_back(new MeterStrip(have_midi ? 2 : 3, lmt));
				meterarea.pack_start (*_metrics[metrics], false, false);
				_metrics[metrics]->set_session(_session);
				_metrics[metrics]->show();
			} else {
				_metrics[metrics]->set_metric_mode(have_midi ? 2 : 3, lmt);
			}
			meterarea.reorder_child(*_metrics[metrics], pos++);
			metrics++;

			lmt = nmt;

			if (_metrics.size() <= metrics) {
				_metrics.push_back(new MeterStrip(1, lmt));
				meterarea.pack_start (*_metrics[metrics], false, false);
				_metrics[metrics]->set_session(_session);
				_metrics[metrics]->show();
			} else {
				_metrics[metrics]->set_metric_mode(1, lmt);
			}
			meterarea.reorder_child(*_metrics[metrics], pos++);
			metrics++;
			have_midi = false;
		}

		if ((*i).visible && (*i).s->has_midi()) {
			have_midi = true;
		}

		meterarea.reorder_child(*((*i).s), pos++);
		if ((*i).visible) {
			last = (*i).s;
		}
	}

	if (last) {
		last->set_tick_bar(last->get_tick_bar() | 2);
	}

	metrics_right.set_metric_mode(have_midi ? 2 : 3, lmt);

	while (_metrics.size() > metrics) {
		meterarea.remove(*_metrics.back());
		delete (_metrics.back());
		_metrics.pop_back();
	}

	_mm_left = metrics_left.get_metric_mode();
	_mt_left = metrics_left.meter_type();
	_mm_right = metrics_right.get_metric_mode();
	_mt_right = metrics_right.meter_type();

	on_scroll();
	queue_resize();
}

void
Meterbridge::resync_order()
{
	sync_order_keys();
}

void
Meterbridge::parameter_changed (std::string const & p)
{
	if (p == "show-busses-on-meterbridge") {
		_show_busses = _session->config.get_show_busses_on_meterbridge();
		resync_order();
	}
	else if (p == "show-master-on-meterbridge") {
		_show_master = _session->config.get_show_master_on_meterbridge();
		resync_order();
	}
	else if (p == "show-midi-on-meterbridge") {
		_show_midi = _session->config.get_show_midi_on_meterbridge();
		resync_order();
	}
	else if (p == "meter-line-up-level") {
		meter_clear_pattern_cache();
	}
	else if (p == "show-rec-on-meterbridge") {
		scroller.queue_resize();
	}
	else if (p == "show-mute-on-meterbridge") {
		scroller.queue_resize();
	}
	else if (p == "show-solo-on-meterbridge") {
		scroller.queue_resize();
	}
	else if (p == "show-name-on-meterbridge") {
		scroller.queue_resize();
	}
	else if (p == "meterbridge-label-height") {
		scroller.queue_resize();
	}
}

void
Meterbridge::on_theme_changed ()
{
	meter_clear_pattern_cache();
}
