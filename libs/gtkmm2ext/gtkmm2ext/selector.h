/*
    Copyright (C) 1999 Paul Barton-Davis 
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

#ifndef __gtkselector_h__
#define __gtkselector_h__

#include <string>
#include <vector>

#include <gtkmm.h>

#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API TreeView_Selector : public Gtk::TreeView
{
public:
	TreeView_Selector() {}
	virtual ~TreeView_Selector() {}

protected:
	virtual bool on_button_press_event(GdkEventButton *ev);
};

typedef void (SelectorRefillFunction)(Glib::RefPtr<Gtk::ListStore>, void *);

class LIBGTKMM2EXT_API Selector : public Gtk::VBox
{
	friend class Gtkmm2ext::TreeView_Selector;

public:
	Selector (SelectorRefillFunction, void *arg, 
		  std::vector<std::string> titles);

	virtual ~Selector ();
	Glib::RefPtr<Gtk::ListStore> liststore () { return lstore; }
	void reset (void (*refiller)(Glib::RefPtr<Gtk::ListStore>, void *), void *arg);
	void set_size (unsigned int w, unsigned int h) {
		scroll.set_size_request (w, h);
		tview.columns_autosize ();
	}

	struct Result {
	    Gtk::TreeView& view;
	    Glib::RefPtr<Gtk::TreeSelection> selection;

	    Result (Gtk::TreeView& v, Glib::RefPtr<Gtk::TreeSelection> sel)
		    : view (v), selection (sel) {}
	};

	/* selection is activated via a double click, choice via
	   a single click.
	*/
	sigc::signal<void,Result*> selection_made;
	sigc::signal<void,Result*> choice_made;
	sigc::signal<void,Result*> shift_made;
	sigc::signal<void,Result*> control_made;

	sigc::signal<void> update_contents;

	void accept();
	void cancel();
	void rescan();


  protected:
	virtual void on_map ();
	virtual void on_show ();

  private:
	Gtk::ScrolledWindow scroll;
	Gtk::TreeModel::ColumnRecord column_records;
	Glib::RefPtr<Gtk::ListStore> lstore;
	Gtkmm2ext::TreeView_Selector tview;
	void (*refiller)(Glib::RefPtr<Gtk::ListStore>, void *);
	void *refill_arg;
	gint selected_row;
	gint selected_column;
	gint chosen_row;
	gint chosen_column;

	void refill ();
	void chosen ();
	void shift_clicked ();
	void control_clicked ();

	static gint _accept (gpointer);
	static gint _chosen (gpointer);
	static gint _shift_clicked (gpointer);
	static gint _control_clicked (gpointer);

};

} /* namespace */

#endif // __gtkselector_h__
