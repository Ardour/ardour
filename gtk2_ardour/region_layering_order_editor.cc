#include <gtkmm/table.h>
#include <gtkmm/stock.h>
#include <ardour/region.h>

#include "i18n.h"
#include "keyboard.h"
#include "public_editor.h"
#include "region_layering_order_editor.h"
#include "utils.h"

using namespace Gtk;
using namespace ARDOUR;

RegionLayeringOrderEditor::RegionLayeringOrderEditor (PublicEditor& pe)
: ArdourDialog (pe, _("RegionLayeringOrderEditor"), false, false)
	, playlist ()
	, position ()
	, in_row_change (false)
        , regions_at_position (0)
	, layering_order_columns ()
	, layering_order_model (Gtk::ListStore::create (layering_order_columns))
	, layering_order_display ()
        , clock ("layer dialog", true, "TransportClock", false, false, false)
	, scroller ()
	, the_editor(pe)
{
	set_name ("RegionLayeringOrderEditorWindow");

	layering_order_display.set_model (layering_order_model);

	layering_order_display.append_column (_("Region Name"), layering_order_columns.name);
	layering_order_display.set_headers_visible (true);
	layering_order_display.set_headers_clickable (true);
	layering_order_display.set_reorderable (false);
	layering_order_display.set_rules_hint (true);

	scroller.set_border_width (10);
	scroller.set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
	scroller.add (layering_order_display);

	Gtk::Table* table = manage (new Gtk::Table (7, 11));
	table->set_size_request (300, 250);
	table->attach (scroller, 0, 7, 0, 5);

	clock.set_mode (AudioClock::BBT);

	HBox* hbox = manage (new HBox);
	hbox->pack_start (clock, true, false);

	get_vbox()->set_spacing (6);
	get_vbox()->pack_start (label, false, false);
	get_vbox()->pack_start (*hbox, false, false);
	get_vbox()->pack_start (*table);

	table->set_name ("RegionLayeringOrderTable");
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
RegionLayeringOrderEditor::row_activated (const TreeModel::Path& path, TreeViewColumn* column)
{
	cerr << "Row activated\n";

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
	}

	in_row_change = false;
}

void
RegionLayeringOrderEditor::set_context (const string& a_name, Session* s, const boost::shared_ptr<Playlist>  & pl, nframes64_t pos)
{
	label.set_text (a_name);

	clock.set_session (s);
	clock.set (pos, true, 0, 0);

	playlist_modified_connection.disconnect ();
	playlist = pl;
	playlist_modified_connection = playlist->Modified.connect (mem_fun (*this, &RegionLayeringOrderEditor::playlist_modified));

	position = pos;
	refill ();
}

bool
RegionLayeringOrderEditor::on_key_press_event (GdkEventKey* ev)
{
	if (ev->keyval == GDK_Return) {
		cerr << "grab magic key\n";
		Keyboard::magic_widget_grab_focus ();		
	}

	bool result = key_press_focus_accelerator_handler (the_editor, ev);
	cerr << "event handled: " << result << endl;

	if (ev->keyval == GDK_Return) {
		cerr << "drop magic focus\n";
		Keyboard::magic_widget_drop_focus ();		
	}

	if (!result) {
		result = ArdourDialog::on_key_press_event (ev);
	}
	return result;
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
