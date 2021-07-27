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
#include "ardour/triggerbox.h"

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
}

TriggerEntry::~TriggerEntry ()
{
}

void
TriggerEntry::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	/* convert expose area back to item coordinate space */

	Rect self (item_to_window (get()));

	setup_outline_context (context);
	rounded_rectangle (context, self.x0, self.y0, self.width(), self.height());
	context->stroke_preserve ();
	setup_fill_context (context);
	context->fill ();
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

	while (true) {
		t = _triggerbox.trigger (n);
		if (!t) {
			break;
		}
		std::cerr << "NEW TE for trigger " << n << std::endl;
		(void) new TriggerEntry (this, *t);
		++n;
	}
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
