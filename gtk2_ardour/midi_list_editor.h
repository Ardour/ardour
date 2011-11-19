/*
    Copyright (C) 2009 Paul Davis

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_gtk2_midi_list_editor_h_
#define __ardour_gtk2_midi_list_editor_h_

#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>

#include "evoral/types.hpp"

#include "ardour/session_handle.h"

#include "ardour_window.h"

namespace Evoral {
	template<typename Time> class Note;
};

namespace ARDOUR {
	class MidiRegion;
	class MidiModel;
	class Session;
};

class MidiListEditor : public ArdourWindow
{
  public:
	typedef Evoral::Note<Evoral::MusicalTime> NoteType;

	MidiListEditor(ARDOUR::Session*, boost::shared_ptr<ARDOUR::MidiRegion>);
	~MidiListEditor();

  private:
	struct MidiListModelColumns : public Gtk::TreeModel::ColumnRecord {
		MidiListModelColumns() {
			add (channel);
			add (note);
			add (note_name);
			add (velocity);
			add (start);
			add (length);
			add (end);
			add (_note);
		};
		Gtk::TreeModelColumn<uint8_t> channel;
		Gtk::TreeModelColumn<uint8_t> note;
		Gtk::TreeModelColumn<std::string> note_name;
		Gtk::TreeModelColumn<uint8_t> velocity;
		Gtk::TreeModelColumn<std::string> start;
		Gtk::TreeModelColumn<std::string> length;
		Gtk::TreeModelColumn<std::string> end;
		Gtk::TreeModelColumn<boost::shared_ptr<NoteType> > _note;
	};

	MidiListModelColumns         columns;
	Glib::RefPtr<Gtk::ListStore> model;
	Gtk::TreeView                view;
	Gtk::ScrolledWindow          scroller;
	std::string               _current_edit;

	boost::shared_ptr<ARDOUR::MidiRegion> region;

	void edited (const std::string&, const std::string&);
	void editing_started (Gtk::CellEditable*, const std::string& path, int);
	void editing_canceled ();

	void redisplay_model ();

	bool key_press (GdkEventKey* ev);
	bool key_release (GdkEventKey* ev);

	void delete_selected_note ();
};

#endif /* __ardour_gtk2_midi_list_editor_h_ */
