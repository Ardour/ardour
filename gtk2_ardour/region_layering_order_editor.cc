#include <gtkmm/table.h>
#include <gtkmm/stock.h>
#include <gtkmm/alignment.h>
#include "ardour/region.h"

#include "gui_thread.h"
#include "keyboard.h"
#include "public_editor.h"
#include "region_layering_order_editor.h"
#include "utils.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

RegionLayeringOrderEditor::RegionLayeringOrderEditor (PublicEditor& pe)
	: ArdourWindow (pe, _("RegionLayeringOrderEditor"))
	, playlist ()
	, position ()
	, in_row_change (false)
        , regions_at_position (0)
	, layering_order_columns ()
	, layering_order_model (Gtk::ListStore::create (layering_order_columns))
	, layering_order_display ()
        , clock ("layer dialog", true, "", false, false, false)
	, scroller ()
	, editor (pe)
{
	set_name ("RegionLayeringOrderEditorWindow");

	layering_order_display.set_model (layering_order_model);

	layering_order_display.append_column (_("Region Name"), layering_order_columns.name);
	layering_order_display.set_headers_visible (true);
	layering_order_display.set_reorderable (false);
	layering_order_display.set_rules_hint (true);

	scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	scroller.add (layering_order_display);

	clock.set_mode (AudioClock::BBT);

        Gtk::Table* scroller_table = manage (new Gtk::Table);
        scroller_table->set_size_request (300, 250);
        scroller_table->attach (scroller, 0, 1, 0, 1);
        scroller_table->set_col_spacings (5);
        scroller_table->set_row_spacings (5);

        track_label.set_name ("RegionLayeringOrderEditorLabel");
        track_label.set_text (_("Track:"));
	track_label.set_alignment (0, 0.5);
        clock_label.set_name ("RegionLayeringOrderEditorLabel");
        clock_label.set_text (_("Position:"));
	clock_label.set_alignment (0, 0.5);
        track_name_label.set_name ("RegionLayeringOrderEditorNameLabel");
	track_name_label.set_alignment (0, 0.5);
        clock.set_mode (AudioClock::BBT);

        Gtk::Table* info_table = manage (new Gtk::Table (2, 2));
        info_table->set_col_spacings (5);
        info_table->set_row_spacings (5);
        info_table->attach (track_label, 0, 1, 0, 1, FILL, FILL);
        info_table->attach (track_name_label, 1, 2, 0, 1, FILL, FILL);
        info_table->attach (clock_label, 0, 1, 1, 2, FILL, FILL);
        info_table->attach (clock, 1, 2, 1, 2, FILL, FILL);

	Gtk::VBox* vbox = Gtk::manage (new Gtk::VBox ());
	vbox->set_spacing (12);
	vbox->pack_start (*info_table, false, false);
	vbox->pack_start (*scroller_table, true, true);
	add (*vbox);

        info_table->set_name ("RegionLayeringOrderTable");
        scroller_table->set_name ("RegionLayeringOrderTable");

	layering_order_display.set_name ("RegionLayeringOrderDisplay");

	layering_order_display.signal_row_activated ().connect (mem_fun (*this, &RegionLayeringOrderEditor::row_activated));

	layering_order_display.grab_focus ();

	set_title (_("Choose Top Region"));
	show_all();
}

RegionLayeringOrderEditor::~RegionLayeringOrderEditor ()
{
}

void
RegionLayeringOrderEditor::row_activated (const TreeModel::Path& path, TreeViewColumn*)
{
	if (in_row_change) {
		return;
	}

	TreeModel::iterator iter = layering_order_model->get_iter (path);

	if (iter) {
		TreeModel::Row row = *iter;
		boost::shared_ptr<Region> region = row[layering_order_columns.region];

		region->raise_to_top ();
	}
}

typedef boost::shared_ptr<Region> RegionPtr;

struct RegionCompareByLayer {
    bool operator() (RegionPtr a, RegionPtr b) const {
	    return a->layer() > b->layer();
    }
};

void
RegionLayeringOrderEditor::refill ()
{
	regions_at_position = 0;

	if (!playlist) {
		return;
	}

	typedef Playlist::RegionList RegionList;

	in_row_change = true;

	layering_order_model->clear ();

	boost::shared_ptr<RegionList> region_list(playlist->regions_at (position));

	regions_at_position = region_list->size();

	if (regions_at_position < 2) {
		playlist_modified_connection.disconnect ();
		hide ();
		in_row_change = false;
		return;
	}

	RegionCompareByLayer cmp;
	region_list->sort (cmp);

	for (RegionList::const_iterator i = region_list->begin(); i != region_list->end(); ++i) {
		TreeModel::Row newrow = *(layering_order_model->append());
		newrow[layering_order_columns.name] = (*i)->name();
		newrow[layering_order_columns.region] = *i;

               if (i == region_list->begin()) {
                       layering_order_display.get_selection()->select(newrow);
               }
	}

	in_row_change = false;
}

void
RegionLayeringOrderEditor::set_context (const string& a_name, Session* s, const boost::shared_ptr<Playlist>  & pl, framepos_t pos)
{
        track_name_label.set_text (a_name);

	clock.set_session (s);
	clock.set (pos, true);

	playlist_modified_connection.disconnect ();
	playlist = pl;
	playlist->ContentsChanged.connect (playlist_modified_connection, invalidator (*this), boost::bind
                                           (&RegionLayeringOrderEditor::playlist_modified, this), gui_context());

	position = pos;
	refill ();
}

bool
RegionLayeringOrderEditor::on_key_press_event (GdkEventKey* ev)
{
	bool handled = false;

	/* in general, we want shortcuts working while in this
	   dialog. However, we'd like to treat "return" specially
	   since it is used for row activation. So ..

	   for return: try normal handling first
	   then try the editor (to get accelerators/shortcuts)
	   then try normal handling (for keys other than return)
	*/

	if (ev->keyval == GDK_Return) {
		handled = ArdourWindow::on_key_press_event (ev);
	}

	if (!handled) {
		handled = key_press_focus_accelerator_handler (editor, ev);
	}

	if (!handled) {
		handled = ArdourWindow::on_key_press_event (ev);
	}

	return handled;
}

void
RegionLayeringOrderEditor::maybe_present ()
{
	if (regions_at_position < 2) {
		hide ();
		return;
	}

	present ();
}

void
RegionLayeringOrderEditor::playlist_modified ()
{
	refill ();
}
