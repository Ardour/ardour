/*
 * Copyright (C) 2022 Ben Loftis <ben@harrisonconsoles.com>
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

#include <gtkmm/stock.h>
#include <gtkmm/table.h>

#include "gtkmm2ext/utils.h"

#include "widgets/ardour_button.h"

#include "ardour/triggerbox.h"

#include "trigger_jump_dialog.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR;
using namespace PBD;
using namespace ArdourWidgets;

/**
 *    EditNoteDialog constructor.
 *
 *    @param n Notes to edit.
 */

TriggerJumpDialog::TriggerJumpDialog (bool right)
	: ArdourDialog ("")
	, _right_fa(right)
{
//	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
//	add_button (Gtk::Stock::APPLY, Gtk::RESPONSE_ACCEPT);
	set_default_response (Gtk::RESPONSE_ACCEPT);
}

void
TriggerJumpDialog::on_trigger_set ()
{
	_table.set_border_width (4);
	_table.set_spacings (4);

	int r = 0;

	for (int i = 0; i < default_triggers_per_box; i++) {  //someday this might change dynamically
		ArdourButton* b = manage (new ArdourButton (ArdourButton::led_default_elements));

		b->signal_clicked.connect(sigc::bind(sigc::mem_fun(*this, &TriggerJumpDialog::button_clicked), i));

		Gtk::Label* l = manage(new Gtk::Label (cue_marker_name(i), ALIGN_RIGHT));
		_table.attach (*l, 0, 1, r,r+1, Gtk::FILL, Gtk::SHRINK);
		_table.attach (*b, 1, 2, r,r+1, Gtk::FILL, Gtk::SHRINK);

		_buttonlist.push_back(b);

		++r;
	}

	get_vbox()->pack_start (_table);

	PropertyChange pc;
	pc.add (Properties::name);
	pc.add (Properties::follow_action0);
	on_trigger_changed(pc);
}

void
TriggerJumpDialog::button_clicked (int b)
{
	FollowAction jump_fa = _right_fa ? trigger()->follow_action1() : trigger()->follow_action0();

	jump_fa.type = FollowAction::JumpTrigger;  //should already be the case if we are in this dialog, but let's take no chances
	jump_fa.targets.flip(b);

	if (_right_fa) {
		trigger()->set_follow_action1(jump_fa);
	} else {
		trigger()->set_follow_action0(jump_fa);
	}
}

void
TriggerJumpDialog::on_trigger_changed (PropertyChange const& what)
{
	set_title(string_compose(_("Jump Target for: %1"), trigger()->name()));
	
	TriggerBox &box = trigger()->box();

	FollowAction jump_fa = _right_fa ? trigger()->follow_action1() : trigger()->follow_action0();

	//update button display state
	ButtonList::const_iterator b = _buttonlist.begin ();
	for (int i = 0; i < default_triggers_per_box; i++) {

		if (b==_buttonlist.end()) {
			break;
		}

		(*b)->set_text(box.trigger(i)->name());
		(*b)->set_active_state(jump_fa.targets.test(i) ? Gtkmm2ext::ExplicitActive : Gtkmm2ext::Off);

		++b;
	}
	
}

void
TriggerJumpDialog::done (int r)
{
	if (r != RESPONSE_ACCEPT) {
		return;
	}
}
