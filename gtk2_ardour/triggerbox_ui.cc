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

#include "pbd/i18n.h"
#include "pbd/compose.h"
#include "pbd/convert.h"

#include "ardour/region.h"
#include "ardour/triggerbox.h"

#include "canvas/polygon.h"
#include "canvas/text.h"

#include "gtkmm2ext/utils.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "triggerbox_ui.h"
#include "public_editor.h"
#include "ui_config.h"
#include "utils.h"

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

	poly_margin = 2. * scale;
	poly_size = height - (poly_margin * 2.);

	ArdourCanvas::Rect r (0, 0, width, height);
	set (r);
	set_outline_all ();
	set_fill_color (Gtkmm2ext::random_color());
	set_outline_color (Gtkmm2ext::random_color());
	name = string_compose ("trigger %1", _trigger.index());

	play_button = new ArdourCanvas::Rectangle (this);
	play_button->set (ArdourCanvas::Rect (0., 0., poly_size + (poly_margin * 2.), height));
	play_button->set_outline (false);
	play_button->set_fill_color (0x000000ff);
	play_button->name = string_compose ("playbutton %1", _trigger.index());

	play_shape = new ArdourCanvas::Polygon (play_button);
	play_shape->set_fill_color (Gtkmm2ext::random_color());
	play_shape->set_outline (false);
	play_shape->name = string_compose ("playshape %1", _trigger.index());

	name_text = new Text (this);
	name_text->set_font_description (UIConfiguration::instance().get_NormalFont());
	name_text->set_color (Gtkmm2ext::contrasting_text_color (fill_color()));
	name_text->set_position (Duple (play_button->get().width() + (2. * scale), poly_margin));

	_trigger.PropertyChanged.connect (trigger_prop_connection, MISSING_INVALIDATOR, boost::bind (&TriggerEntry::prop_change, this, _1), gui_context());

	PropertyChange changed;
	changed.add (ARDOUR::Properties::name);
	changed.add (ARDOUR::Properties::running);

	prop_change (changed);
}


TriggerEntry::~TriggerEntry ()
{
}

void
TriggerEntry::draw_play_button ()
{
	Points p;

	if (_trigger.region()) {
		/* stopped: triangle */
		p.push_back (Duple (poly_margin, poly_margin));
		p.push_back (Duple (poly_margin, poly_size));
		p.push_back (Duple (poly_size, poly_size / 2.));
	} else {
		/* stopped: square */
		p.push_back (Duple (poly_margin, poly_margin));
		p.push_back (Duple (poly_margin, poly_size));
		p.push_back (Duple (poly_size, poly_size));
		p.push_back (Duple (poly_size, poly_margin));
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
			name_text->set (short_version (_trigger.region()->name(), 20));
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
		draw_play_button ();
	}
}



/* ---------------------------- */

TriggerBoxUI::TriggerBoxUI (ArdourCanvas::Item* parent, TriggerBox& tb)
	: Box (parent, Box::Vertical)
	, _triggerbox (tb)
	, file_chooser (0)
	, _context_menu (0)
{
	set_homogenous (true);
	set_spacing (16);
	set_padding (16);
	set_fill (false);

	build ();
}

TriggerBoxUI::~TriggerBoxUI ()
{
}

void
TriggerBoxUI::build ()
{
	Trigger* t;
	size_t n = 0;

	// clear_items (true);

	_slots.clear ();

	while (true) {
		t = _triggerbox.trigger (n);
		if (!t) {
			break;
		}
		TriggerEntry* te = new TriggerEntry (canvas(), *t);
		te->set_pack_options (PackOptions (PackFill|PackExpand));
		add (te);

		_slots.push_back (te);

		te->play_button->Event.connect (sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::bang), n));
		te->name_text->Event.connect (sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::text_event), n));
		te->Event.connect (sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::event), n));

		++n;
	}
}

bool
TriggerBoxUI::text_event (GdkEvent *ev, size_t n)
{
	return false;
}

bool
TriggerBoxUI::event (GdkEvent* ev, size_t n)
{
	switch (ev->type) {
	case GDK_2BUTTON_PRESS:
		choose_sample (n);
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
TriggerBoxUI::bang (GdkEvent *ev, size_t n)
{
	if (!_triggerbox.trigger (n)->region()) {
		/* this is a stop button */
		switch (ev->type) {
		case GDK_BUTTON_PRESS:
			if (ev->button.button == 1) {
				_triggerbox.stop_all ();
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
TriggerBoxUI::context_menu (size_t n)
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

	fitems.push_back (CheckMenuElem (_("Stop"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::Stop)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::Stop) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (CheckMenuElem (_("Again"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::Again)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::Again) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (CheckMenuElem (_("Queued"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::QueuedTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::QueuedTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (CheckMenuElem (_("Next"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::NextTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::NextTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (CheckMenuElem (_("Previous"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::PrevTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::PrevTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (CheckMenuElem (_("First"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::FirstTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::FirstTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (CheckMenuElem (_("Last"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::LastTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::LastTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (CheckMenuElem (_("Any"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::AnyTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::AnyTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}
	fitems.push_back (CheckMenuElem (_("Other"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_follow_action), n, Trigger::OtherTrigger)));
	if (_triggerbox.trigger (n)->follow_action(0) == Trigger::OtherTrigger) {
		dynamic_cast<Gtk::CheckMenuItem*> (&fitems.back ())->set_active (true);
	}

	Menu* launch_menu = manage (new Menu);
	MenuList& litems = launch_menu->items();

	litems.push_back (CheckMenuElem (_("One Shot"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_launch_style), n, Trigger::OneShot)));
	if (_triggerbox.trigger (n)->launch_style() == Trigger::OneShot) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}
	litems.push_back (CheckMenuElem (_("Gate"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_launch_style), n, Trigger::Gate)));
	if (_triggerbox.trigger (n)->launch_style() == Trigger::Gate) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}
	litems.push_back (CheckMenuElem (_("Toggle"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_launch_style), n, Trigger::Toggle)));
	if (_triggerbox.trigger (n)->launch_style() == Trigger::Toggle) {
		dynamic_cast<Gtk::CheckMenuItem*> (&litems.back ())->set_active (true);
	}
	litems.push_back (CheckMenuElem (_("Repeat"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_launch_style), n, Trigger::Repeat)));
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
		qitems.push_back (CheckMenuElem (_("Main Grid"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
		/* can't mark this active because the current trigger quant setting may just a specific setting below */
		/* XXX HOW TO GET THIS TO FOLLOW GRID CHANGES (which are GUI only) */
	}

	b = BBT_Offset (1, 0, 0);
	qitems.push_back (CheckMenuElem (_("Bars"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}

	b = BBT_Offset (0, 4, 0);
	qitems.push_back (CheckMenuElem (_("Whole"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 2, 0);
	qitems.push_back (CheckMenuElem (_("Half"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 1, 0);
	qitems.push_back (CheckMenuElem (_("Quarters"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat/2);
	qitems.push_back (CheckMenuElem (_("Eighths"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}
	b = BBT_Offset (0, 0, ticks_per_beat/4);
	qitems.push_back (CheckMenuElem (_("Sixteenths"), sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::set_quantization), n, b)));
	if (_triggerbox.trigger (n)->quantization() == b) {
		dynamic_cast<Gtk::CheckMenuItem*> (&qitems.back ())->set_active (true);
	}

	items.push_back (MenuElem (_("Follow Action..."), *follow_menu));
	items.push_back (MenuElem (_("Launch Style..."), *launch_menu));
	items.push_back (MenuElem (_("Quantization..."), *quant_menu));

	_context_menu->popup (1, gtk_get_current_event_time());
}

void
TriggerBoxUI::set_follow_action (size_t n, Trigger::FollowAction fa)
{
	_triggerbox.trigger (n)->set_follow_action (fa, 0);
}

void
TriggerBoxUI::set_launch_style (size_t n, Trigger::LaunchStyle ls)
{
	_triggerbox.trigger (n)->set_launch_style (ls);
}

void
TriggerBoxUI::set_quantization (size_t n, Temporal::BBT_Offset const & q)
{
	_triggerbox.trigger (n)->set_quantization (q);
}

void
TriggerBoxUI::choose_sample (size_t n)
{
	if (!file_chooser) {
		file_chooser = new Gtk::FileChooserDialog (_("Select sample"), Gtk::FILE_CHOOSER_ACTION_OPEN);
		file_chooser->add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
		file_chooser->add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
	}

	file_chooser_connection.disconnect ();
	file_chooser_connection = file_chooser->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::sample_chosen), n));

	file_chooser->present ();
}

void
TriggerBoxUI::sample_chosen (int response, size_t n)
{
	file_chooser->hide ();

	switch (response) {
	case Gtk::RESPONSE_OK:
		break;
	default:
		return;
	}

	std::string path = file_chooser->get_filename ();

	_triggerbox.set_from_path (n, path);
	// _triggerbox.trigger (n)->set_length (timecnt_t (Temporal::Beats (4, 0)));
}

/* ------------ */

TriggerBoxWidget::TriggerBoxWidget (TriggerBox& tb)
{
	ui = new TriggerBoxUI (root(), tb);
}

void
TriggerBoxWidget::size_request (double& w, double& h) const
{
	ui->size_request (w, h);
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
