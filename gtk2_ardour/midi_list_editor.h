/*
 * Copyright (C) 2009-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_gtk2_midi_list_editor_h_
#define __ardour_gtk2_midi_list_editor_h_

#include <gtkmm/treeview.h>
#include <gtkmm/table.h>
#include <gtkmm/box.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>

#include "ardour/session_handle.h"

#include "ardour_window.h"

namespace Evoral {
	template<typename Time> class Note;
};

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class MidiTrack;
	class Session;
};

class MidiListEditor : public ArdourWindow
{
public:
	typedef Evoral::Note<Temporal::Beats> NoteType;

	MidiListEditor(ARDOUR::Session*, boost::shared_ptr<ARDOUR::MidiRegion>,
	               boost::shared_ptr<ARDOUR::MidiTrack>);
	~MidiListEditor();

private:
	struct MidiListModelColumns : public Gtk::TreeModel::ColumnRecord
	{
		MidiListModelColumns() {
			add (channel);
			add (note);
			add (note_name);
			add (velocity);
			add (start);
			add (length);
			add (_note);
		};
		Gtk::TreeModelColumn<uint8_t>     channel;
		Gtk::TreeModelColumn<uint8_t>     note;
		Gtk::TreeModelColumn<std::string> note_name;
		Gtk::TreeModelColumn<uint8_t>     velocity;
		Gtk::TreeModelColumn<std::string> start;
		Gtk::TreeModelColumn<std::string> length;
		Gtk::TreeModelColumn<boost::shared_ptr<NoteType> > _note;
	};

	struct NoteLengthColumns : public Gtk::TreeModel::ColumnRecord
	{
		NoteLengthColumns() {
			add (ticks);
			add (name);
		}
		Gtk::TreeModelColumn<int>         ticks;
		Gtk::TreeModelColumn<std::string> name;
	};

	MidiListModelColumns         columns;
	Glib::RefPtr<Gtk::ListStore> model;
	NoteLengthColumns            note_length_columns;
	Glib::RefPtr<Gtk::ListStore> note_length_model;
	Gtk::TreeView                view;
	Gtk::ScrolledWindow          scroller;
	Gtk::TreeModel::Path         edit_path;
	int                          edit_column;
	Gtk::CellRendererText*       editing_renderer;
	Gtk::CellEditable*           editing_editable;
	Gtk::Table                   buttons;
	Gtk::VBox                    vbox;
	Gtk::ToggleButton            sound_notes_button;

	boost::shared_ptr<ARDOUR::MidiRegion> region;
	boost::shared_ptr<ARDOUR::MidiTrack>  track;

	/** connection used to connect to model's ContentChanged signal */
	PBD::ScopedConnectionList content_connections;

	void edited (const std::string&, const std::string&);
	void editing_started (Gtk::CellEditable*, const std::string& path, int);
	void editing_canceled ();
	void stop_editing (bool cancelled = false);

	void redisplay_model ();

	bool key_press (GdkEventKey* ev);
	bool key_release (GdkEventKey* ev);
	bool scroll_event (GdkEventScroll*);

	void delete_selected_note ();
	void selection_changed ();
};

#endif /* __ardour_gtk2_midi_list_editor_h_ */
