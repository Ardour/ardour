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
	Rect r (0, 0, 25, 12);
	set (r);
	set_outline_all ();
}

TriggerEntry::~TriggerEntry ()
{
}

void
TriggerEntry::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	/* convert expose area back to item coordinate space */

	setup_outline_context (context);
	rounded_rectangle (context, x0(), y0(), x1(), y1());
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
	set_spacing (6);
	set_padding (6);

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

	clear_items (true);

	while (true) {
		t = _triggerbox.trigger (n);
		if (!t) {
			break;
		}

		(void) new TriggerEntry (this, *t);
	}
}

/* ------------ */

TriggerBoxWidget::TriggerBoxWidget (TriggerBox& tb)
{
	ui = new TriggerBoxUI (root(), tb);
}
