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

#include <sstream>

#include "pbd/strsplit.h"

#include "widgets/tooltips.h"

#include "gtkmm2ext/actions.h"

#include "editing_context.h"
#include "chord_editor.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

ChordEditor::ChordEditor (EditingContext& ec, ChordProvider& cp, int cs)
	: chord_provider (cp)
	, _chord_size (cs)
	, button_sizing (Gtk::SizeGroup::create (Gtk::SIZE_GROUP_HORIZONTAL))
	, canonical_label (_("Canonical name"))
	, short_label (_("Preferred short name"))
	, other_label (_("Other names"))
{
	using namespace Gtk;
	using namespace Menu_Helpers;
	using namespace ArdourWidgets;

	/* XXX 12TET alert */

	for (int n = 1; n < 12; ++n) {
		char buf[64];
		snprintf (buf, sizeof (buf), "%d", n);
		ArdourButton* i_button (manage (new ArdourButton (buf, ArdourButton::default_elements, true)));
		interval_buttons.push_back (i_button);
		upper_interval_packer.pack_start (*i_button, false, false);
		button_sizing->add_widget (*i_button);
	}

	for (int n = 13; n < 24; ++n) {
		char buf[64];
		snprintf (buf, sizeof (buf), "%d", n);
		ArdourButton* i_button (manage (new ArdourButton (buf, ArdourButton::default_elements, true)));
		interval_buttons.push_back (i_button);
		lower_interval_packer.pack_start (*i_button, false, false);
		button_sizing->add_widget (*i_button);
	}

	upper_interval_packer.show_all ();
	lower_interval_packer.show_all ();

	ArdourWidgets::set_tooltip (canonical_entry, _("Long, maximally descriptive, canonical name for this chord"));
	ArdourWidgets::set_tooltip (short_entry, _("Your preferred short name for this chord"));
	ArdourWidgets::set_tooltip (other_entry, _("Comma-separated list of other names for this chord (optional)"));

	canonical_box.set_spacing (12);
	canonical_box.pack_start (canonical_label, false, true);
	canonical_box.pack_start (canonical_entry, true, true);

	other_box.set_spacing (12);
	other_box.pack_start (other_label, false, true);
	other_box.pack_start (other_entry, true, true);

	short_box.set_spacing (12);
	short_box.pack_start (short_label, false, true);
	short_box.pack_start (short_entry, true, true);

	Label* l (manage (new Label (_("Chord contains the following intervals from root"))));

	set_border_width (12);
	set_spacing (12);

	pack_start (*l, true, true);
	pack_start (upper_interval_packer, true, true);
	pack_start (lower_interval_packer, true, true);
	pack_start (canonical_box, false, false);
	pack_start (short_box, false, false);
	pack_start (other_box, false, false);
}

ChordEditor::~ChordEditor ()
{
}

void
ChordEditor::set_protected (bool yn)
{
	for (auto & b : interval_buttons) {
		b->set_sensitive (!yn);
	}
	canonical_entry.set_editable (!yn);
}

bool
ChordEditor::is_protected() const
{
	return !canonical_entry.get_editable ();
}

ChordProvider::ChordInfo
ChordEditor::get_chord () const
{
	ChordProvider::Intervals intervals;
	intervals.push_back (0);

	int n = 1;

	for (auto & b : interval_buttons) {
		if (b->get_active()) {
			intervals.push_back (n);
		}
		++n;
	}

	std::vector<std::string> others;
	std::string otxt = other_entry.get_text();
	split (otxt, others, ',');

	return ChordProvider::ChordInfo (intervals, ChordProvider::hash_intervals (intervals), canonical_entry.get_text(), short_entry.get_text(), others);
}

void
ChordEditor::show_chord (std::vector<int> const & intervals)
{
	for (auto & b : interval_buttons) {
		b->set_active (false);
	}

	for (auto i : intervals) {
		interval_buttons[i]->set_active (true);
	}

	auto other_names (chord_provider.other_names (intervals));
	std::string canon (chord_provider.canonical_name (intervals));
	std::string short_name (chord_provider.short_name (intervals));

	canonical_entry.set_text (canon);
	short_entry.set_text (short_name);

	std::string otxt;
	if (!other_names.empty()) {
		std::stringstream ss;
		for (auto const & o : other_names) {
			ss << o << ',';
		}
		otxt = ss.str();
		otxt.pop_back(); /* remove final comma */
	}

	other_entry.set_text (otxt);
}
