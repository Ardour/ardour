/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <gtkmm/filechooserdialog.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/stock.h>

#include "pbd/compose.h"
#include "pbd/convert.h"

#include "ardour/region.h"
#include "ardour/triggerbox.h"

#include "canvas/polygon.h"
#include "canvas/text.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/utils.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "triggerbox_ui.h"
#include "trigger_ui.h"
#include "public_editor.h"
#include "region_view.h"
#include "selection.h"
#include "timers.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace Gtkmm2ext;
using namespace PBD;

TriggerEntry::TriggerEntry (Canvas* canvas, ARDOUR::Trigger& t)
	: ArdourCanvas::Rectangle (canvas)
	, _trigger (t)
{
	const double scale = UIConfiguration::instance().get_ui_scale();
	const double width = 150. * scale;
	const double height = 20. * scale;

	name = string_compose ("trigger %1", _trigger.index());

	Event.connect (sigc::mem_fun (*this, &TriggerEntry::event_handler));

	poly_margin = 2. * scale;
	poly_size = height - (poly_margin * 2.);

	ArdourCanvas::Rect r (0, 0, width, height);
	set (r);
	set_outline_all ();

	active_bar = new ArdourCanvas::Rectangle (this);
	active_bar->set_outline (false);

	owner_color_changed ();

	play_button = new ArdourCanvas::Rectangle (this);
	play_button->set (ArdourCanvas::Rect (poly_margin/2., poly_margin/2., poly_size + ((poly_margin/2.) * 2.), height - ((poly_margin/2.) * 2.)));
	play_button->set_outline (false);
	play_button->set_fill_color (outline_color());
	play_button->name = string_compose ("playbutton %1", _trigger.index());
	play_button->hide ();

	play_shape = new ArdourCanvas::Polygon (play_button);
	play_shape->set_fill_color (outline_color());
	play_shape->set_outline (false);
	play_shape->name = string_compose ("playshape %1", _trigger.index());
	play_shape->hide ();

	name_text = new Text (this);
	name_text->set_font_description (UIConfiguration::instance().get_NormalFont());
	name_text->set_height_based_on_allocation (true);
	name_text->set_color (Gtkmm2ext::contrasting_text_color (fill_color()));
	name_text->set_position (Duple (play_button->get().width() + (2. * scale), poly_margin));
	name_text->set_ignore_events (true);

	_trigger.PropertyChanged.connect (trigger_prop_connection, MISSING_INVALIDATOR, boost::bind (&TriggerEntry::prop_change, this, _1), gui_context());
	dynamic_cast<Stripable*> (_trigger.box().owner())->presentation_info().Change.connect (owner_prop_connection, MISSING_INVALIDATOR, boost::bind (&TriggerEntry::owner_prop_change, this, _1), gui_context());

	PropertyChange changed;
	changed.add (ARDOUR::Properties::name);
	changed.add (ARDOUR::Properties::running);

	prop_change (changed);
}


TriggerEntry::~TriggerEntry ()
{
}

void
TriggerEntry::owner_prop_change (PropertyChange const & pc)
{
	if (pc.contains (Properties::color)) {
		owner_color_changed ();
	}
}

void
TriggerEntry::owner_color_changed ()
{
	set_fill_color (dynamic_cast<Stripable*> (_trigger.box().owner())->presentation_info().color());
	set_outline_color (HSV (fill_color()).opposite().color());
	active_bar->set_fill_color (HSV (fill_color()).darker(0.3).color ());
}

bool
TriggerEntry::event_handler (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
		if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
			play_button->show ();
			play_shape->show ();
		}
		redraw ();
		break;
	case GDK_LEAVE_NOTIFY:
		if (ev->crossing.detail != GDK_NOTIFY_INFERIOR) {
			play_button->hide ();
			play_shape->hide ();
		}
		redraw ();
		break;
	default:
		break;
	}

	return false;
}

void
TriggerEntry::maybe_update ()
{
	double nbw;

	if (!_trigger.active()) {
		nbw = 0;
	} else {
		nbw = _trigger.position_as_fraction () * (_allocation.width() - _allocation.height());
	}

	if (nbw) {
		const double scale = UIConfiguration::instance().get_ui_scale();
		ArdourCanvas::Rect r (get());

		active_bar->set (ArdourCanvas::Rect (r.height() * scale,
		                                     (r.y0 + 1) * scale,
		                                     (r.height() + nbw - 1) * scale,
		                                     (r.y1 - 1) * scale));
		active_bar->show ();
	} else {
		active_bar->hide ();
	}
}

void
TriggerEntry::_size_allocate (ArdourCanvas::Rect const & alloc)
{
	const double scale = UIConfiguration::instance().get_ui_scale();

	Rectangle::_size_allocate (alloc);

	const Distance height = get().height();

	poly_size = height - (poly_margin * 2.);

	shape_play_button ();
	play_button->set (ArdourCanvas::Rect (poly_margin/2., poly_margin/2., poly_size + ((poly_margin/2.) * 2.), height - ((poly_margin/2.) * 2.)));

	const Distance lhs = play_button->get().width() + (2. * poly_margin * scale);
	ArdourCanvas::Rect text_alloc (lhs, poly_margin, alloc.width() - lhs - (poly_margin * scale), alloc.height() - (2. * poly_margin));
	name_text->size_allocate (text_alloc);

	name_text->set_position (Duple (lhs, poly_margin));
}

void
TriggerEntry::shape_play_button ()
{
	Points p;

	if (!_trigger.region()) {
		/* no region, so must be a stop button, drawn as a square */
		p.push_back (Duple (poly_margin, poly_margin));
		p.push_back (Duple (poly_margin, poly_size));
		p.push_back (Duple (poly_size, poly_size));
		p.push_back (Duple (poly_size, poly_margin));
	} else {
		/* region exists; draw triangle to show that we can trigger */
		p.push_back (Duple (poly_margin, poly_margin));
		p.push_back (Duple (poly_margin, poly_size));
		p.push_back (Duple (poly_size, poly_size / 2.));
	}

	play_shape->set (p);

	if (_trigger.active()) {
		play_button->set_fill (true);
	} else {
		play_button->set_fill (false);
	}
}

void
TriggerEntry::prop_change (PropertyChange const & change)
{
	bool need_pb = false;

	if (change.contains (ARDOUR::Properties::name)) {
		if (_trigger.region()) {
			name_text->set (short_version (_trigger.name(), 16));
		} else {
			/* we need some spaces to have something to click on */
			name_text->set ("");
		}

		need_pb = true;
	}

	if (change.contains (ARDOUR::Properties::running)) {
		need_pb = true;
	}

	if (need_pb) {
		shape_play_button ();
	}
}



/* ---------------------------- */

TriggerBoxUI::TriggerBoxUI (ArdourCanvas::Item* parent, TriggerBox& tb)
	: Table (parent)
	, _triggerbox (tb)
	, file_chooser (0)
	, _context_menu (0)
{
	set_homogenous (true);
	set_row_spacing (4);
	set_fill_color (UIConfiguration::instance().color (X_("theme:bg")));
	set_fill (true);

	build ();
}

TriggerBoxUI::~TriggerBoxUI ()
{
	update_connection.disconnect ();
}

void
TriggerBoxUI::build ()
{
	Trigger* t;
	uint64_t n = 0;
	uint32_t row = 0;

	// clear_items (true);

	_slots.clear ();

	while (true) {
		t = _triggerbox.trigger (n);
		if (!t) {
			break;
		}
		TriggerEntry* te = new TriggerEntry (canvas(), *t);
		attach (te, 0, row++, PackExpand, PackExpand);

		_slots.push_back (te);

		te->play_button->Event.connect (sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::bang), n));
		te->name_text->Event.connect (sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::text_event), n));
		te->Event.connect (sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::event), n));

		++n;
	}
}

bool
TriggerBoxUI::text_event (GdkEvent *ev, uint64_t n)
{
	return false;
}

bool
TriggerBoxUI::event (GdkEvent* ev, uint64_t n)
{
	switch (ev->type) {
	case GDK_2BUTTON_PRESS:
		edit_trigger (n);
		return true;
	case GDK_BUTTON_RELEASE:
		switch (ev->button.button) {
		case 3:
			context_menu (n);
			return true;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return false;
}


bool
TriggerBoxUI::bang (GdkEvent *ev, uint64_t n)
{
	if (!_triggerbox.trigger (n)->region()) {
		/* this is a stop button */
		switch (ev->type) {
		case GDK_BUTTON_PRESS:
			if (ev->button.button == 1) {
				_triggerbox.request_stop_all ();
				return true;
			}
		default:
			break;
		}

		return false;
	}

	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		switch (ev->button.button) {
		case 1:
			_slots[n]->trigger().bang ();
			return true;
		default:
			break;
		}
		break;
	case GDK_BUTTON_RELEASE:
		switch (ev->button.button) {
		case 1:
			if (_slots[n]->trigger().launch_style() == Trigger::Gate) {
				_slots[n]->trigger().unbang ();
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return false;
}

void
TriggerBoxUI::context_menu (uint64_t n)
{
	using namespace Gtk;
	using namespace Gtk::Menu_Helpers;
	using namespace Temporal;

	delete _context_menu;

	_context_menu = new Menu;
	MenuList& items = _context_menu->items();
	_context_menu->set_name ("ArdourContextMenu");

	Menu* follow_menu = manage (new Menu);
	MenuList& fitems = follow_menu->items();

	RadioMenuItem::Group fagroup;
	RadioMenuItem::Group lagroup;
	RadioMenuItem::Group qgroup;

	fitems.push_back (RadioMenuElem (fagroup, _("Stop"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::Stop)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::Stop) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (RadioMenuElem (fagroup, _("Again"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::Again)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::Again) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (RadioMenuElem (fagroup, _("Queued"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::QueuedTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::QueuedTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (RadioMenuElem (fagroup, _("Next"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::NextTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::NextTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (RadioMenuElem (fagroup, _("Previous"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::PrevTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::PrevTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (RadioMenuElem (fagroup, _("First"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::FirstTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::FirstTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (RadioMenuElem (fagroup, _("Last"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::LastTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::LastTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (RadioMenuElem (fagroup, _("Any"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::AnyTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::AnyTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (RadioMenuElem (fagroup, _("Other"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::OtherTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::OtherTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}

	Menu* launch_menu = manage (new Menu);
	MenuList& litems = launch_menu->items();

	litems.push_back (RadioMenuElem (lagroup, _("One Shot"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_launch_style), n, Trigger::OneShot)));
	if (_triggerbox.trigger (n)->launch_style() == Trigger::OneShot) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}
	litems.push_back (RadioMenuElem (lagroup, _("Gate"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_launch_style), n, Trigger::Gate)));
	if (_triggerbox.trigger (n)->launch_style() == Trigger::Gate) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}
	litems.push_back (RadioMenuElem (lagroup, _("Toggle"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_launch_style), n, Trigger::Toggle)));
	if (_triggerbox.trigger (n)->launch_style() == Trigger::Toggle) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}
	litems.push_back (RadioMenuElem (lagroup, _("Repeat"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_launch_style), n, Trigger::Repeat)));
	if (_triggerbox.trigger (n)->launch_style() == Trigger::Repeat) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}


	Menu* quant_menu = manage (new Menu);
	MenuList& qitems = quant_menu->items();
	bool success;

	Beats grid_beats (PublicEditor::instance().get_grid_type_as_beats (success, timepos_t (0)));
	BBT_Offset b;

	if (success) {
		b = BBT_Offset (0, grid_beats.get_beats(), grid_beats.get_ticks());
		qitems.push_back (RadioMenuElem (fagroup, _("Main Grid"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
		/* can't mark this active because the current trigger quant setting may just a specific setting below */
		/* XXX HOW TO GET THIS TO FOLLOW GRID CHANGES (which are GUI only) */
	}

	b = BBT_Offset (1, 0, 0);
	qitems.push_back (RadioMenuElem (qgroup, _("Bars"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}

	b = BBT_Offset (0, 4, 0);
	qitems.push_back (RadioMenuElem (qgroup, _("Whole"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 2, 0);
	qitems.push_back (RadioMenuElem (qgroup, _("Half"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 1, 0);
	qitems.push_back (RadioMenuElem (qgroup, _("Quarters"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat/2);
	qitems.push_back (RadioMenuElem (qgroup, _("Eighths"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat/4);
	qitems.push_back (RadioMenuElem (qgroup, _("Sixteenths"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat/8);
	qitems.push_back (RadioMenuElem (qgroup, _("Thirty-Seconds"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat/16);
	qitems.push_back (RadioMenuElem (qgroup, _("Sixty-Fourths"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}

	Menu* load_menu = manage (new Menu);
	MenuList& loitems (load_menu->items());

	loitems.push_back (MenuElem (_("from file"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::choose_sample), n)));
	loitems.push_back (MenuElem (_("from selection"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_from_selection), n)));

	items.push_back (MenuElem (_("Load..."), *load_menu));
	items.push_back (MenuElem (_("Edit..."), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::edit_trigger), n)));
	items.push_back (MenuElem (_("Follow Action..."), *follow_menu));
	items.push_back (MenuElem (_("Launch Style..."), *launch_menu));
	items.push_back (MenuElem (_("Quantization..."), *quant_menu));

	_context_menu->popup (1, gtk_get_current_event_time());
}

void
TriggerBoxUI::edit_trigger (uint64_t n)
{
	Trigger* trigger = _triggerbox.trigger (n);
	TriggerWindow* tw = static_cast<TriggerWindow*> (trigger->ui());

	if (!tw) {
		tw = new TriggerWindow (*_triggerbox.trigger (n));
		trigger->set_ui (tw);
	}

	tw->present ();
}

void
TriggerBoxUI::set_follow_action (uint64_t n, Trigger::FollowAction fa)
{
	_triggerbox.trigger (n)->set_follow_action (fa, 0);
}

void
TriggerBoxUI::set_launch_style (uint64_t n, Trigger::LaunchStyle ls)
{
	_triggerbox.trigger (n)->set_launch_style (ls);
}

void
TriggerBoxUI::set_quantization (uint64_t n, Temporal::BBT_Offset const & q)
{
	_triggerbox.trigger (n)->set_quantization (q);
}

void
TriggerBoxUI::choose_sample (uint64_t n)
{
	if (!file_chooser) {
		file_chooser = new Gtk::FileChooserDialog (_("Select sample"), Gtk::FILE_CHOOSER_ACTION_OPEN);
		file_chooser->add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		file_chooser->add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
		file_chooser->set_select_multiple (true);
	}

	file_chooser_connection.disconnect ();
	file_chooser_connection = file_chooser->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::sample_chosen), n));

	file_chooser->present ();
}

void
TriggerBoxUI::sample_chosen (int response, uint64_t n)
{
	file_chooser->hide ();

	switch (response) {
	case Gtk::RESPONSE_OK:
		break;
	default:
		return;
	}

	std::list<std::string> paths = file_chooser->get_filenames ();

	for (std::list<std::string>::iterator s = paths.begin(); s != paths.end(); ++s) {
		/* this will do nothing if n is too large */
		_triggerbox.set_from_path (n, *s);
		++n;
	}
}

void
TriggerBoxUI::set_from_selection (uint64_t n)
{
	Selection& selection (PublicEditor::instance().get_selection());
	RegionSelection rselection (selection.regions);

	if (rselection.empty()) {
		/* XXX possible message about no selection ? */
		return;
	}

	for (RegionSelection::iterator r = rselection.begin(); r != rselection.end(); ++r) {
		_triggerbox.set_from_selection (n, (*r)->region());
		++n;
	}
}

void
TriggerBoxUI::start_updating ()
{
	update_connection = Timers::rapid_connect (sigc::mem_fun (*this, &TriggerBoxUI::rapid_update));
}

void
TriggerBoxUI::stop_updating ()
{
	update_connection.disconnect ();
}

void
TriggerBoxUI::rapid_update ()
{
	for (Slots::iterator s = _slots.begin(); s != _slots.end(); ++s) {
		(*s)->maybe_update ();
	}
}


/* ------------ */

TriggerBoxWidget::TriggerBoxWidget (TriggerBox& tb)
{
	ui = new TriggerBoxUI (root(), tb);
	set_background_color (UIConfiguration::instance().color (X_("theme:bg")));
}

void
TriggerBoxWidget::size_request (double& w, double& h) const
{
	ui->size_request (w, h);
}

void
TriggerBoxWidget::on_map ()
{
	GtkCanvas::on_map ();
	ui->start_updating ();
}

void
TriggerBoxWidget::on_unmap ()
{
	GtkCanvas::on_unmap ();
	ui->stop_updating ();
}


/* ------------ */

TriggerBoxWindow::TriggerBoxWindow (TriggerBox& tb)
{
	TriggerBoxWidget* tbw = manage (new TriggerBoxWidget (tb));
	set_title (_("TriggerBox for XXXX"));

	double w;
	double h;

	tbw->size_request (w, h);

	set_default_size (w, h);
	add (*tbw);
	tbw->show ();
}

bool
TriggerBoxWindow::on_key_press_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance()->main_window());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}

bool
TriggerBoxWindow::on_key_release_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance()->main_window());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}
