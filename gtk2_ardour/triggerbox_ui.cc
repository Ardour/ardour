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

#include "pbd/i18n.h"
#include "pbd/compose.h"

#include "ardour/triggerbox.h"

#include "canvas/polygon.h"
#include "canvas/text.h"

#include "gtkmm2ext/utils.h"

#include "triggerbox_ui.h"

using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace Gtkmm2ext;

TriggerEntry::TriggerEntry (Item* parent, ARDOUR::Trigger& t)
	: Rectangle (parent)
	, _trigger (t)
{
	Rect r (0, 0, 150, 20);
	set (r);
	set_outline_all ();
	set_fill_color (Gtkmm2ext::random_color());
	set_outline_color (Gtkmm2ext::random_color());
	name = string_compose ("trigger %1", _trigger.index());

	play_button = new Polygon (this);

	Points p;
	p.push_back (Duple (0, 0));
	p.push_back (Duple (0, 10));
	p.push_back (Duple (10, 5));

	play_button->set (p);
	play_button->set_fill_color (Gtkmm2ext::random_color());
	play_button->set_outline (false);

	play_button->set_position (Duple (10, 2));

	name_text = new Text (this);
	name_text->set_font_description (Pango::FontDescription ("Sans 10"));
	name_text->set ("Bang Crash");
	name_text->set_color (Gtkmm2ext::random_color());
	name_text->set_position (Duple (50, name_text->height() / 2));
}

TriggerEntry::~TriggerEntry ()
{
}

/* ---------------------------- */

TriggerBoxUI::TriggerBoxUI (ArdourCanvas::Item* parent, TriggerBox& tb)
	: Box (parent, Box::Vertical)
	, _triggerbox (tb)
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
		std::cerr << "NEW TE for trigger " << n << std::endl;
		TriggerEntry* te = new TriggerEntry (this, *t);

		_slots.push_back (te);

		te->play_button->Event.connect (sigc::bind (sigc::mem_fun (*this, &TriggerBoxUI::bang), n));

		++n;
	}
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

/* ------------ */

TriggerBoxWidget::TriggerBoxWidget (TriggerBox& tb)
{
	ui = new TriggerBoxUI (root(), tb);
}

/* ------------ */

TriggerBoxWindow::TriggerBoxWindow (TriggerBox& tb)
{
	TriggerBoxWidget* tbw = manage (new TriggerBoxWidget (tb));
	set_title (_("TriggerBox for XXXX"));
	set_default_size (100, 100);
	add (*tbw);
	tbw->show ();
}
