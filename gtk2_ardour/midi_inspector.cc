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

#include <ardour/midi_region.h>

#include "chord_box.h"
#include "midi_inspector.h"
#include "ui_config.h"
#include "region_editor.h"
#include "quantize_dialog.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

MidiInspector::MidiInspector (EditingContext& ec)
	: region_editor (nullptr)
	, chord_expander (_("Chord Editing"))
	, quantize_expander (_("Quantize"))
	, region_expander (_("Region Properties"))
{
	chord_box = manage (new ChordBox (ec));
	chord_expander.add (*chord_box);

	quantize_widget = manage (new QuantizeWidget (ec));
	quantize_expander.add (*quantize_widget);

	pack_start (chord_expander, false, false);
	pack_start (quantize_expander, false, false);
	pack_start (region_expander, false, false);

	set_border_width (12);
	UIConfiguration::instance().DPIReset.connect ([this]() { queue_resize(); });
}

void
MidiInspector::set_region (Session* s, std::shared_ptr<MidiRegion> mr)
{
	if (mr) {
		region_editor = manage (new RegionEditor (s, mr));
		region_expander.add (*region_editor);
	} else if (region_editor) {
		region_expander.remove (); /* will delete */
		region_editor = nullptr;
	}
}

void
MidiInspector::on_size_request (Gtk::Requisition* req)
{
	Gtk::Requisition max;
	max.width = -1;
	max.height = -1;
	Gtk::Requisition sub;
	sub.width = -1;
	sub.height = -1;

	chord_box->size_request (sub);
	if (sub.width > 0 && sub.width > max.width) {
		max.width = sub.width;
	}

	if (sub.height > 0 && sub.height > max.height) {
		max.height = sub.height;
	}

	quantize_widget->size_request (sub);
	if (sub.width > 0 && sub.width > max.width) {
		max.width = sub.width;
	}

	if (sub.height > 0 && sub.height > max.height) {
		max.height = sub.height;
	}

	VBox::on_size_request (req);
	req->width = std::max (max.width, req->width);
	/* no need to adjust height */
}

