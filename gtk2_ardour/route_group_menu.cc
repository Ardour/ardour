#include <gtkmm/menu.h>
#include <gtkmm/stock.h>
#include "ardour/session.h"
#include "ardour/route_group.h"
#include "route_group_menu.h"
#include "route_group_dialog.h"
#include "i18n.h"

using namespace Gtk;
using namespace ARDOUR;

RouteGroupMenu::RouteGroupMenu (Session& s)
	: _session (s)
{
	rebuild (0);
}

void
RouteGroupMenu::rebuild (RouteGroup* curr)
{
	using namespace Menu_Helpers;
	
	items().clear ();

	items().push_back (MenuElem (_("New group..."), mem_fun (*this, &RouteGroupMenu::new_group)));
	items().push_back (SeparatorElem ());

	RadioMenuItem::Group group;
	items().push_back (RadioMenuElem (group, _("No group"), bind (mem_fun (*this, &RouteGroupMenu::set_group), (RouteGroup *) 0)));

	if (curr == 0) {
		static_cast<RadioMenuItem*> (&items().back())->set_active ();
	}
		
	_session.foreach_route_group (bind (mem_fun (*this, &RouteGroupMenu::add_item), curr, &group));
}

void
RouteGroupMenu::add_item (RouteGroup* rg, RouteGroup* curr, RadioMenuItem::Group* group)
{
	using namespace Menu_Helpers;

	items().push_back (RadioMenuElem (*group, rg->name(), bind (mem_fun(*this, &RouteGroupMenu::set_group), rg)));
	
	if (rg == curr) {
		static_cast<RadioMenuItem*> (&items().back())->set_active ();
	}	
}

void
RouteGroupMenu::set_group (RouteGroup* g)
{
	GroupSelected (g);
}


void
RouteGroupMenu::new_group ()
{
	RouteGroup* g = new RouteGroup (_session, "", RouteGroup::Active);

	RouteGroupDialog d (g, Gtk::Stock::NEW);
	int const r = d.do_run ();

	if (r == Gtk::RESPONSE_OK) {
		_session.add_route_group (g);
		set_group (g);
	} else {
		delete g;
	}
}
