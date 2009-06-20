#include <gtkmm/stock.h>
#include "ardour/route_group.h"
#include "route_group_dialog.h"
#include "i18n.h"

using namespace Gtk;
using namespace ARDOUR;

RouteGroupDialog::RouteGroupDialog (RouteGroup* g)
	: Dialog (_("Route group")),
	  _group (g),
	  _active (_("Active"))
{
	_name.set_text (_group->name ());
	_active.set_active (_group->is_active ());
	
	HBox* h = manage (new HBox);
	h->pack_start (*manage (new Label (_("Name:"))));
	h->pack_start (_name);

	get_vbox()->pack_start (*h);
	get_vbox()->pack_start (_active);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	/* XXX: change this depending on context */
	add_button (Stock::OK, RESPONSE_OK);

	show_all ();
}

int
RouteGroupDialog::do_run ()
{
	int const r = run ();

	if (r == Gtk::RESPONSE_OK) {
		_group->set_name (_name.get_text ());
		_group->set_active (_active.get_active (), this);
	}

	return r;
}
