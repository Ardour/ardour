/*
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
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

#ifndef __gtk_ardour_region_edit_h__
#define __gtk_ardour_region_edit_h__

#include <map>

#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/button.h>
#include <gtkmm/arrow.h>
#include <gtkmm/frame.h>
#include <gtkmm/table.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/separator.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/listviewtext.h>
#include <gtkmm/scrolledwindow.h>

#include "gtkmm2ext/dndtreeview.h"
#include "gtkmm2ext/dndvbox.h"

#include "pbd/signals.h"

#include "audio_clock.h"
#include "ardour_dialog.h"
#include "plugin_interest.h"
#include "region_editor.h"

namespace ARDOUR {
	class Region;
	class Session;
	class RegionFxPlugin;
}

class RegionView;
class ClockGroup;

class RegionEditor : public ArdourDialog
{
public:
	RegionEditor (ARDOUR::Session*, RegionView*);
	virtual ~RegionEditor ();

protected:
	virtual void region_changed (const PBD::PropertyChange&);
	virtual void region_fx_changed ();

	Gtk::Table _table;
	int _table_row;

private:
	class RegionFxEntry : public Gtkmm2ext::DnDVBoxChild, public sigc::trackable
	{
	public:
		RegionFxEntry (std::shared_ptr<ARDOUR::RegionFxPlugin>);

		Gtk::EventBox& action_widget () { return _fx_btn; }
		Gtk::Widget& widget () { return _box; }
		std::string drag_text () const { return name (); }
		bool is_selectable() const { return true; }
		bool can_copy_state (Gtkmm2ext::DnDVBoxChild*) const;
		void set_visual_state (Gtkmm2ext::VisualState, bool);
		bool drag_data_get (Glib::RefPtr<Gdk::DragContext> const, Gtk::SelectionData &);
		std::shared_ptr<ARDOUR::RegionFxPlugin> region_fx_plugin () const { return _rfx; }

	private:
		std::string name () const;

		Gtk::VBox                               _box;
		ArdourWidgets::ArdourButton             _fx_btn;
		std::shared_ptr<ARDOUR::RegionFxPlugin> _rfx;
		ARDOUR::PluginPresetPtr                 _plugin_preset_pointer;
	};

	class RegionFxBox : public Gtk::VBox, public PluginInterestedObject //, public ARDOUR::SessionHandlePtr
	{
	public:
		RegionFxBox (std::shared_ptr<ARDOUR::Region>);
		void redisplay_plugins ();

	private:
		void add_fx_to_display (std::weak_ptr<ARDOUR::RegionFxPlugin>);
		void show_plugin_gui (std::weak_ptr<ARDOUR::RegionFxPlugin>, bool custom_ui = true);
		void queue_delete_region_fx (std::weak_ptr<ARDOUR::RegionFxPlugin>);
		bool idle_delete_region_fx (std::weak_ptr<ARDOUR::RegionFxPlugin>);
		void notify_plugin_load_fail (uint32_t cnt = 1);
		bool on_key_press (GdkEventKey*);

		/* PluginInterestedObject */
		bool use_plugins (SelectedPlugins const&);

		/* DNDVbox signal handlers */
		bool fxe_button_press_event (GdkEventButton*, RegionFxEntry*);
		bool fxe_button_release_event (GdkEventButton*, RegionFxEntry*);

		void reordered ();
		void plugin_drop (Gtk::SelectionData const&, RegionFxEntry*, Glib::RefPtr<Gdk::DragContext> const&);
		void object_drop (Gtkmm2ext::DnDVBox<RegionFxEntry>*, RegionFxEntry*, Glib::RefPtr<Gdk::DragContext> const&);
		void delete_dragged_plugins (std::list<std::shared_ptr<ARDOUR::RegionFxPlugin>> const&);

		std::shared_ptr<ARDOUR::RegionFxPlugin> find_drop_position (RegionFxEntry*);

		std::shared_ptr<ARDOUR::Region>   _region;
		Gtkmm2ext::DnDVBox<RegionFxEntry> _display;
		Gtk::ScrolledWindow               _scroller;
		Gtk::EventBox                     _base;
		bool                              _no_redisplay;
		int                               _placement;
	};

	std::shared_ptr<ARDOUR::Region> _region;

	void connect_editor_events ();

	Gtk::Label name_label;
	Gtk::Entry name_entry;
	Gtk::ToggleButton audition_button;

	Gtk::Label position_label;
	Gtk::Label end_label;
	Gtk::Label length_label;
	Gtk::Label sync_relative_label;
	Gtk::Label sync_absolute_label;
	Gtk::Label start_label;
	Gtk::Label region_fx_label;

	ClockGroup* _clock_group;

	AudioClock position_clock;
	AudioClock end_clock;
	AudioClock length_clock;
	AudioClock sync_offset_relative_clock; ///< sync offset relative to the start of the region
	AudioClock sync_offset_absolute_clock; ///< sync offset relative to the start of the timeline
	AudioClock start_clock;

	RegionFxBox _region_fx_box;

	PBD::ScopedConnection state_connection;
	PBD::ScopedConnection audition_connection;
	PBD::ScopedConnection region_connection;

	void bounds_changed (const PBD::PropertyChange&);
	void name_changed ();

	void audition_state_changed (bool);

	void activation ();

	void name_entry_changed ();
	void position_clock_changed ();
	void end_clock_changed ();
	void length_clock_changed ();
	void sync_offset_absolute_clock_changed ();
	void sync_offset_relative_clock_changed ();

	void audition_button_toggled ();

	gint bpressed (GdkEventButton* ev, Gtk::SpinButton* but, void (RegionEditor::*pmf)());
	gint breleased (GdkEventButton* ev, Gtk::SpinButton* but, void (RegionEditor::*pmf)());

	bool on_delete_event (GdkEventAny *);
	void handle_response (int);

	bool spin_arrow_grab;

	Gtk::Label _sources_label;
	Gtk::ListViewText _sources;

	void set_clock_mode_from_primary ();
};

#endif /* __gtk_ardour_region_edit_h__ */
