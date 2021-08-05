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
#include <gtkmm/stock.h>

#include "pbd/i18n.h"
#include "pbd/compose.h"
#include "pbd/convert.h"

#include "ardour/region.h"
#include "ardour/triggerbox.h"

#include "canvas/polygon.h"
#include "canvas/text.h"

#include "gtkmm2ext/utils.h"

#include "triggerbox_ui.h"
#include "ui_config.h"

using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace Gtkmm2ext;
using namespace PBD;

TriggerEntry::TriggerEntry (Canvas* canvas, ARDOUR::Trigger& t)
	: Rectangle (canvas)
	, _trigger (t)
{
	const double scale = UIConfiguration::instance().get_ui_scale();
	const double width = 150. * scale;
	const double height = 20. * scale;

	Rect r (0, 0, width, height);
	set (r);
	set_outline_all ();
	set_fill_color (Gtkmm2ext::random_color());
	set_outline_color (Gtkmm2ext::random_color());
	name = string_compose ("trigger %1", _trigger.index());

	play_button = new Polygon (this);

	Points p;
	const double triangle_size = height - (8. * scale);
	p.push_back (Duple (0., 0.));
	p.push_back (Duple (0., triangle_size));
	p.push_back (Duple (triangle_size, triangle_size / 2.));

	play_button->set (p);
	play_button->set_fill_color (Gtkmm2ext::random_color());
	play_button->set_outline (false);

	play_button->set_position (Duple (10. * scale, 4. * scale));

	name_text = new Text (this);
	name_text->set_font_description (UIConfiguration::instance().get_NormalFont());
	if (_trigger.region()) {
		name_text->set (short_version (_trigger.region()->name(), 20));
	} else {
		/* we need some spaces to have something to click on */
		name_text->set (X_("     "));
	}
	name_text->set_color (Gtkmm2ext::contrasting_text_color (fill_color()));
	name_text->set_position (Duple (50, 4. * scale));
}

TriggerEntry::~TriggerEntry ()
{
}

/* ---------------------------- */

TriggerBoxUI::TriggerBoxUI (ArdourCanvas::Item* parent, TriggerBox& tb)
	: Box (parent, Box::Vertical)
	, _triggerbox (tb)
	, file_chooser (0)
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

		++n;
	}
}

bool
TriggerBoxUI::text_event (GdkEvent *ev, size_t n)
{
	switch (ev->type) {
	case GDK_2BUTTON_PRESS:
		/* double click */
		choose_sample (n);
		return true;
		break;
	case GDK_BUTTON_PRESS:
		break;
	default:
		break;
	}

	return false;
}

bool
TriggerBoxUI::bang (GdkEvent *ev, size_t n)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		if (ev->button.button == 1) {
			_triggerbox.queue_trigger (&_slots[n]->trigger());
			return true;
		}
		break;
	default:
		break;
	}
	return false;
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
