#include <gtkmm/stock.h>
#include "ardour/route_group.h"
#include "route_group_dialog.h"
#include "i18n.h"

using namespace Gtk;
using namespace ARDOUR;

RouteGroupDialog::RouteGroupDialog (RouteGroup* g, StockID const & s)
	: Dialog (_("Route group")),
	  _group (g),
	  _active (_("Active")),
	  _gain (_("Gain")),
	  _mute (_("Muting")),
	  _solo (_("Soloing")),
	  _rec_enable (_("Record enable")),
	  _select (_("Selection")),
	  _edit (_("Editing"))
{
	_name.set_text (_group->name ());
	_active.set_active (_group->is_active ());

	_gain.set_active (_group->property (RouteGroup::Gain));
	_mute.set_active (_group->property (RouteGroup::Mute));
	_solo.set_active (_group->property (RouteGroup::Solo));
	_rec_enable.set_active (_group->property (RouteGroup::RecEnable));
	_select.set_active (_group->property (RouteGroup::Select));
	_edit.set_active (_group->property (RouteGroup::Edit));
	
	HBox* h = manage (new HBox);
	h->pack_start (*manage (new Label (_("Name:"))));
	h->pack_start (_name);

	get_vbox()->pack_start (*h);
	get_vbox()->pack_start (_active);
	get_vbox()->pack_start (_gain);
	get_vbox()->pack_start (_mute);
	get_vbox()->pack_start (_solo);
	get_vbox()->pack_start (_rec_enable);
	get_vbox()->pack_start (_select);
	get_vbox()->pack_start (_edit);

	get_vbox()->set_border_width (8);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (s, RESPONSE_OK);

	show_all ();
}

int
RouteGroupDialog::do_run ()
{
	int const r = run ();

	if (r == Gtk::RESPONSE_OK) {
		_group->set_name (_name.get_text ());
		_group->set_active (_active.get_active (), this);

		_group->set_property (RouteGroup::Gain, _gain.get_active ());
		_group->set_property (RouteGroup::Mute, _mute.get_active ());
		_group->set_property (RouteGroup::Solo, _solo.get_active ());
		_group->set_property (RouteGroup::RecEnable, _rec_enable.get_active ());
		_group->set_property (RouteGroup::Select, _select.get_active ());
		_group->set_property (RouteGroup::Edit, _edit.get_active ());
	}

	return r;
}
