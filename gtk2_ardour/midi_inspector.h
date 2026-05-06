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

#pragma once

#include "ytkmm/box.h"
#include "ytkmm/expander.h"

class ChordBox;
class EditingContext;
class RegionEditor;
class QuantizeWidget;

namespace ARDOUR {
	class Session;
	class MidiRegion;
}

class MidiInspector : public Gtk::VBox
{
  public:
	MidiInspector (EditingContext&);

	void set_region (ARDOUR::Session* s, std::shared_ptr<ARDOUR::MidiRegion> mr);

	ChordBox* chord_box;
	QuantizeWidget* quantize_widget;
	RegionEditor* region_editor;

  private:
	Gtk::Expander chord_expander;
	Gtk::Expander quantize_expander;
	Gtk::Expander region_expander;

	void on_size_request (Gtk::Requisition*);
};
