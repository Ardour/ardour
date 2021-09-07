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
#include "trigger_ui.h"
#include "public_editor.h"
#include "ui_config.h"
#include "utils.h"

using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace Gtkmm2ext;
using namespace PBD;

TriggerUI::TriggerUI (Item* parent, Trigger& t)
	: Box (parent, Box::Vertical)
	, trigger (t)
{
	follow_label = new Box (canvas(), Horizontal);
	follow_label->set_fill_color (UIConfiguration::instance().color (X_("theme:bg")));
	follow_label->set_outline_color (UIConfiguration::instance().color (X_("neutral:foreground")));

	follow_text = new Text (canvas());
	follow_text->set (X_("Follow Action"));
	follow_text->set_color (Gtkmm2ext::contrasting_text_color (follow_label->fill_color()));

	follow_label->add (follow_text);
	add (follow_label);
}

TriggerUI::~TriggerUI ()
{
}

/* ------------ */

TriggerWidget::TriggerWidget (Trigger& t)
{
	ui = new TriggerUI (root(), t);
	set_background_color (UIConfiguration::instance().color (X_("theme:bg")));
}

void
TriggerWidget::size_request (double& w, double& h) const
{
	ui->size_request (w, h);
}

/* ------------ */

TriggerWindow::TriggerWindow (Trigger& t)
{
	TriggerWidget* tw = manage (new TriggerWidget (t));
	set_title (_("Trigger XXXX"));

	double w;
	double h;

	tw->size_request (w, h);

	set_default_size (w, h);
	add (*tw);
	tw->show ();
}

bool
TriggerWindow::on_key_press_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance()->main_window());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}

bool
TriggerWindow::on_key_release_event (GdkEventKey* ev)
{
	Gtk::Window& main_window (ARDOUR_UI::instance()->main_window());
	return ARDOUR_UI_UTILS::relay_key_press (ev, &main_window);
}
