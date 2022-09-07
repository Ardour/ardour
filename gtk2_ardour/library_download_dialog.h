#ifndef __gtk2_ardour_library_download_dialog_h__
#define __gtk2_ardour_library_download_dialog_h__

#include <string>

#include <gtkmm/entry.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treeview.h>

#include "ardour_dialog.h"

namespace ARDOUR {
	class LibraryDescription;
}

class LibraryDownloadDialog : public ArdourDialog
{
  public:
	LibraryDownloadDialog ();

	void add_library (ARDOUR::LibraryDescription const &);

  private:
	class LibraryColumns : public Gtk::TreeModelColumnRecord {
	  public:
		LibraryColumns() {
			add (name);
			add (author);
			add (license);
			add (size);
			add (installed);
			add (description);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> author;
		Gtk::TreeModelColumn<std::string> license;
		Gtk::TreeModelColumn<std::string> size;
		Gtk::TreeModelColumn<std::string> description;
		Gtk::TreeModelColumn<bool> installed;
	};

	Gtk::TreeView _display;
	Glib::RefPtr<Gtk::ListStore> _model;
	LibraryColumns _columns;

	template <class T>
	Gtk::TreeViewColumn* append_col (Gtk::TreeModelColumn<T> const& col, int width = 0)
	{
		Gtk::TreeViewColumn* c = manage (new Gtk::TreeViewColumn ("", col));
		if (width) {
			c->set_fixed_width (width);
			c->set_sizing (Gtk::TREE_VIEW_COLUMN_FIXED);
		}
		_display.append_column (*c);
		return c;
	}

	bool tv_query_tooltip (int x, int y, bool kbd, const Glib::RefPtr<Gtk::Tooltip>& tooltip);


	void setup_col (Gtk::TreeViewColumn*, int, Gtk::AlignmentEnum, const char*, const char*);
	void setup_toggle (Gtk::TreeViewColumn*, sigc::slot<void, std::string>);

	void install_activated (std::string str);
};



#endif /* __gtk2_ardour_library_download_dialog_h__ */
