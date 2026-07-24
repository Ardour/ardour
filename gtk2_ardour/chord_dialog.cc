/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#include <ytkmm/stock.h>

#include "pbd/natsort.h"

#include "widgets/tooltips.h"

#include "gtkmm2ext/actions.h"

#include "chord_dialog.h"
#include "chord_editor.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

ChordDialog::ChordDialog (EditingContext& ec, ChordProvider& cp, int chord_size)
	: ArdourDialog (_("Chord Editor"), true)
	, editor (manage (new ChordEditor (ec, cp, chord_size)))
	, chord_list (1)
	, add_chord_button (_("Add a new chord"))
{
	using namespace Gtk;
	std::vector<char const *> names;

	chord_list.set_column_title (0, _("Select a chord below"));

	for (auto const & ci : ChordProvider::chord_info) {
		if ((chord_size < 0 && int (ci.intervals.size()) >= -chord_size) || ci.intervals.size() == size_t (chord_size)) {
			names.push_back (ci.canonical_name.c_str());
		}
	}

	sort (names.begin(), names.end(), [](const char* a, const char* b) {
		return PBD::naturally_less(a, b);
	});

	for (auto & n : names) {
		chord_list.append_text (n);
	}

	chord_list.set_size_request (-1, 250);
	chord_list.get_selection()->signal_changed().connect (sigc::mem_fun (*this, &ChordDialog::chord_selected));

	scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	scroller.add (chord_list);

	add_chord_button.signal_clicked.connect (sigc::mem_fun (*this, &ChordDialog::add_chord));
	add_chord_button.show ();

	get_vbox()->pack_start (scroller, true, true);
	get_vbox()->pack_start (add_chord_button, false, false);
	get_vbox()->pack_start (*editor, false, false);

	scroller.show_all ();
	editor->hide ();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OK, RESPONSE_OK);
}

void
ChordDialog::add_chord ()
{
	editor->set_protected (false);
	editor->show_all ();
	add_chord_button.hide ();
	scroller.hide ();
}

bool
ChordDialog::is_protected() const
{
	return editor->is_protected();
}

void
ChordDialog::chord_selected ()
{
	auto sel = chord_list.get_selected ();

	if (sel.empty()) {
		return;
	}

	std::string name = chord_list.get_text (sel.front());

	for (auto const & ci : ChordProvider::chord_info) {
		if (ci.canonical_name == name) {
			editor->set_protected (true);
			editor->show_all ();
			editor->show_chord (ci.intervals);
			break;
		}
	}
}

ARDOUR::ChordProvider::ChordInfo
ChordDialog::get_chord () const
{
	return editor->get_chord ();
}
