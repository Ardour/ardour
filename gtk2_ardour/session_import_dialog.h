/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#ifndef __session_import_dialog_h__
#define __session_import_dialog_h__

#include <string>
#include <list>
#include <utility>

#include <boost/shared_ptr.hpp>
#include <gtkmm.h>

#include "pbd/xml++.h"

#include "ardour_dialog.h"
namespace ARDOUR {
	class ElementImportHandler;
	class ElementImporter;
	class Session;
}

class SessionImportDialog : public ArdourDialog
{
  private:
	typedef boost::shared_ptr<ARDOUR::ElementImportHandler> HandlerPtr;
	typedef std::list<HandlerPtr> HandlerList;

	typedef boost::shared_ptr<ARDOUR::ElementImporter> ElementPtr;
	typedef std::list<ElementPtr> ElementList;

  public:
	SessionImportDialog (ARDOUR::Session* target);

	virtual Gtk::FileChooserAction browse_action() const { return Gtk::FILE_CHOOSER_ACTION_OPEN; }

  private:

	void load_session (const std::string& filename);
	void fill_list ();
	void browse ();
	void do_merge ();
	void end_dialog ();
	void update (std::string path);
	void show_info(const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column);

	std::pair<bool, std::string> open_rename_dialog (std::string text, std::string name);
	bool open_prompt_dialog (std::string text);

	// Data
	HandlerList        handlers;
	XMLTree            tree;

	// GUI
	Gtk::Frame                    file_frame;
	Gtk::HBox                     file_hbox;
	Gtk::Entry                    file_entry;
	Gtk::Button                   file_browse_button;

	struct SessionBrowserColumns : public Gtk::TreeModel::ColumnRecord
	{
	public:
	  Gtk::TreeModelColumn<std::string>    name;
	  Gtk::TreeModelColumn<bool>           queued;
	  Gtk::TreeModelColumn<ElementPtr>     element;
	  Gtk::TreeModelColumn<std::string>    info;

	  SessionBrowserColumns() { add (name); add (queued); add (element); add (info); }
	};

	SessionBrowserColumns         sb_cols;
	Glib::RefPtr<Gtk::TreeStore>  session_tree;
	Gtk::TreeView                 session_browser;
	Gtk::ScrolledWindow           session_scroll;

	Gtk::Button*                  ok_button;
	Gtk::Button*                  cancel_button;

	PBD::ScopedConnectionList connections;
};

#endif
