#ifndef __gtk2_ardour_region_layering_order_editor_h__
#define __gtk2_ardour_region_layering_order_editor_h__

#include <gtkmm/dialog.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treeview.h>
#include <gtkmm/scrolledwindow.h>

#include "ardour/region.h"
#include "ardour/playlist.h"

#include "ardour_window.h"
#include "audio_clock.h"

class PublicEditor;

namespace ARDOUR {
	class Session;
}

class RegionLayeringOrderEditor : public ArdourWindow
{
  public:
	RegionLayeringOrderEditor (PublicEditor&);
	virtual ~RegionLayeringOrderEditor ();

	void set_context(const std::string& name, ARDOUR::Session* s, const boost::shared_ptr<ARDOUR::Playlist>  & pl, ARDOUR::framepos_t position);
	void maybe_present ();

  protected:
	virtual bool on_key_press_event (GdkEventKey* event);

  private:
	boost::shared_ptr<ARDOUR::Playlist> playlist;
	framepos_t position;
	bool in_row_change;
	uint32_t regions_at_position;

        PBD::ScopedConnection playlist_modified_connection;

	struct LayeringOrderColumns : public Gtk::TreeModel::ColumnRecord {
		LayeringOrderColumns () {
			add (name);
			add (region);
		}
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<boost::shared_ptr<ARDOUR::Region> > region;
	};
	LayeringOrderColumns layering_order_columns;
	Glib::RefPtr<Gtk::ListStore> layering_order_model;
	Gtk::TreeView layering_order_display;
	AudioClock clock;
	Gtk::Label track_label;
	Gtk::Label track_name_label;
	Gtk::Label clock_label;
	Gtk::ScrolledWindow scroller;   // Available layers
	PublicEditor& editor;

	void row_activated (const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column);
	void refill ();
	void playlist_modified ();
};

#endif /* __gtk2_ardour_region_layering_order_editor_h__ */
