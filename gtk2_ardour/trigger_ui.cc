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

#include "canvas/constrained_item.h"
#include "canvas/polygon.h"
#include "canvas/text.h"
#include "canvas/widget.h"

#include "widgets/ardour_button.h"

#include "gtkmm2ext/utils.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "trigger_ui.h"
#include "public_editor.h"
#include "ui_config.h"
#include "utils.h"

using namespace ARDOUR;
using namespace ArdourCanvas;
using namespace ArdourWidgets;
using namespace Gtkmm2ext;
using namespace PBD;

static std::vector<std::string> follow_strings;
static std::string longest_follow;

TriggerUI::TriggerUI (Item* parent, Trigger& t)
	: ConstraintPacker (parent)
	, trigger (t)
{
	if (follow_strings.empty()) {
		follow_strings.push_back (follow_action_to_string (Trigger::Stop));
		follow_strings.push_back (follow_action_to_string (Trigger::Again));
		follow_strings.push_back (follow_action_to_string (Trigger::QueuedTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::NextTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::PrevTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::FirstTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::LastTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::AnyTrigger));
		follow_strings.push_back (follow_action_to_string (Trigger::OtherTrigger));

		for (std::vector<std::string>::const_iterator i = follow_strings.begin(); i != follow_strings.end(); ++i) {
			if (i->length() > longest_follow.length()) {
				longest_follow = *i;
			}
		}
	}

	name = "TriggerUI-CP";

	_follow_action_button = new ArdourButton ();
	_follow_action_button->set_text (_("Follow Action"));

	follow_action_button = new ArdourCanvas::Widget (canvas(), *_follow_action_button);
	follow_action_button->name = "FollowAction";

	_follow_left = new ArdourDropdown;
	_follow_left->append_text_item (_("None"));
	_follow_left->append_text_item (_("Repeat"));
	_follow_left->append_text_item (_("Next"));
	_follow_left->append_text_item (_("Previous"));
	_follow_left->set_sizing_text (longest_follow);

	follow_left = new Widget (canvas(), *_follow_left);
	follow_left->name = "FollowLeft";

	_follow_right = new ArdourDropdown;
	_follow_right->append_text_item (_("None"));
	_follow_right->append_text_item (_("Repeat"));
	_follow_right->append_text_item (_("Next"));
	_follow_right->append_text_item (_("Previous"));
	_follow_right->set_sizing_text (longest_follow);

	follow_right = new Widget (canvas(), *_follow_right);
	follow_right->name = "FollowRight";

	ConstrainedItem* cfa = add_constrained (follow_action_button);
	ConstrainedItem* cfl = add_constrained (follow_left);
	ConstrainedItem* cfr = add_constrained (follow_right);

	/* sizing */

	const double scale = UIConfiguration::instance().get_ui_scale();
	const Distance spacing = 12. * scale;

	constrain (this->width == cfa->width() + (2. * spacing));
	constrain (cfa->top() == spacing);
	constrain (cfa->left() == spacing);
	constrain (cfa->height() == 26); /* XXX fix me */

	cfl->below (*cfa, spacing);
	cfl->same_size_as (*cfr);
	cfl->left_aligned_with (*cfa);
	cfl->same_height_as (*cfa);
	cfl->top_aligned_with (*cfr);

	cfr->below (*cfa, spacing);
	cfr->right_aligned_with (*cfa);
	cfr->right_of (*cfl, spacing);

	trigger_changed ();
}

TriggerUI::~TriggerUI ()
{
}

std::string
TriggerUI::follow_action_to_string (Trigger::FollowAction fa)
{
	switch (fa) {
	case Trigger::Stop:
		return _("Stop");
	case Trigger::Again:
		return _("Again");
	case Trigger::QueuedTrigger:
		return _("Queued");
	case Trigger::NextTrigger:
		return _("Next");
	case Trigger::PrevTrigger:
		return _("Prev");
	case Trigger::FirstTrigger:
		return _("First");
	case Trigger::LastTrigger:
		return _("Last");
	case Trigger::AnyTrigger:
		return _("Any");
	case Trigger::OtherTrigger:
		return _("Other");
	}
	/*NOTREACHED*/
	return std::string();
}

void
TriggerUI::trigger_changed ()
{
	_follow_right->set_text (follow_action_to_string (trigger.follow_action (0)));
	_follow_left->set_text (follow_action_to_string (trigger.follow_action (1)));
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
	set_title (string_compose (_("Trigger: %1"), t.name()));

	double w;
	double h;

	tw->show ();
	tw->size_request (w, h);
	set_default_size (w, h);
	add (*tw);
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
