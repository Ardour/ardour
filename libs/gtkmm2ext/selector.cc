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

    $Id$
*/

#include <algorithm>
#include <functional>
#include <vector>
#include <string>

#include <glibmm.h>
#include <gdkmm.h>

#include "pbd/pathscanner.h"

#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/selector.h"
#include "gtkmm2ext/utils.h"

using namespace std;
using namespace Gtkmm2ext;

Selector::Selector (void (*func)(Glib::RefPtr<Gtk::ListStore>, void *), void *arg,
		    vector<string> titles)
{
	scroll.add (tview);
	scroll.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	pack_start (scroll, true, true);

	vector<string>::iterator i;
	for (i = titles.begin(); i != titles.end(); ++i) {
		Gtk::TreeModelColumn<Glib::ustring> title;
		column_records.add(title);
	}

	lstore = Gtk::ListStore::create(column_records);
	tview.set_model(lstore);

	update_contents.connect(mem_fun(*this,&Selector::rescan));

	tview.show ();

	refiller = func;
	refill_arg = arg;
	selected_row = -1;
	selected_column = -1;
}

Selector::~Selector ()

{
	/* ensure that any row data set with set_row_data_full() is deleted */
	hide_all ();
	lstore.clear ();
}

void
Selector::on_map()

{
	Gtk::VBox::on_map ();

	selected_row = -1;
	selected_column = -1;
	refill();
}

void
Selector::on_show()
{
	VBox::on_show();

	rescan();
}

void
Selector::reset (void (*func)(Glib::RefPtr<Gtk::ListStore>, void *), void *arg)

{
	refiller = func;
	refill_arg = arg;
	selected_row = -1;
	selected_column = -1;

	refill();
}

void
Selector::refill ()

{
	if (refiller) {
		lstore.clear ();
		refiller (lstore, refill_arg);
	}
}

gint
Selector::_accept (gpointer arg)

{
	((Selector *) arg)->accept ();
	return FALSE;
}

gint
Selector::_chosen (gpointer arg)

{
	((Selector *) arg)->chosen ();
	return FALSE;
}

gint
Selector::_shift_clicked (gpointer arg)
{
	((Selector *) arg)->shift_clicked ();
	return FALSE;
}

gint
Selector::_control_clicked (gpointer arg)
{
	((Selector *) arg)->control_clicked ();
	return FALSE;
}

void
Selector::accept ()
{
	Glib::RefPtr<Gtk::TreeSelection> tree_sel = tview.get_selection();
	Gtk::TreeModel::iterator iter = tree_sel->get_selected();

	if (iter) {

		selection_made (new Result (tview, tree_sel));
	} else {
		cancel ();
	}
}

void
Selector::chosen ()
{
	Glib::RefPtr<Gtk::TreeSelection> tree_sel = tview.get_selection();
	Gtk::TreeModel::iterator iter = tree_sel->get_selected();
	
	if (iter) {
		choice_made (new Result (tview, tree_sel));
	} else {
		cancel ();
	}
}

void
Selector::shift_clicked ()
{
	Glib::RefPtr<Gtk::TreeSelection> tree_sel = tview.get_selection();
	Gtk::TreeModel::iterator iter = tree_sel->get_selected();

	if (iter) {
		shift_made (new Result (tview, tree_sel));
	} else {
		cancel ();
	}
}

void
Selector::control_clicked ()
{
	Glib::RefPtr<Gtk::TreeSelection> tree_sel = tview.get_selection();
	Gtk::TreeModel::iterator iter = tree_sel->get_selected();

	if (iter) {
		control_made (new Result (tview, tree_sel));
	} else {
		cancel ();
	}
}

void
Selector::cancel ()
{
        Glib::RefPtr<Gtk::TreeSelection> tree_sel = tview.get_selection();
	tree_sel->unselect_all();

	selection_made (new Result (tview, tree_sel));
}

void
Selector::rescan ()

{
	selected_row = -1;
	selected_column = -1;
	refill ();
	show_all ();
}

struct string_cmp {
    bool operator()(const string* a, const string* b) {
	    return *a < *b;
    }
};

bool
TreeView_Selector::on_button_press_event(GdkEventButton* ev)
{
	bool return_value = TreeView::on_button_press_event(ev);

	if (ev && (ev->type == GDK_BUTTON_RELEASE || ev->type == GDK_2BUTTON_PRESS)) {
		if (ev->state & Keyboard::PrimaryModifier) {
			g_idle_add (Selector::_control_clicked, this);
		} else if (ev->state & Keyboard::TertiaryModifier) {
			g_idle_add (Selector::_shift_clicked, this);
		} else if (ev->type == GDK_2BUTTON_PRESS) {
			g_idle_add (Selector::_accept, this);
		} else {
			g_idle_add (Selector::_chosen, this);
		}
	}

	return return_value;
}
