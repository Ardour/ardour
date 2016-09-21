/*
  Copyright (C) 2016 Paul Davis

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

#include <string>

#include <gtkmm/menu.h>

#include "pbd/string_convert.h"

#include "ardour/session.h"
#include "ardour/stripable.h"
#include "ardour/types.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"

#include "ardour_button.h"
#include "control_slave_ui.h"
#include "gui_thread.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using std::string;

ControlSlaveUI::ControlSlaveUI (Session* s)
	: SessionHandlePtr (s)
	, initial_button (ArdourButton::default_elements)
	, context_menu (0)
{
	set_no_show_all (true);

	Gtkmm2ext::UI::instance()->set_tip (*this, _("VCA Assign"));

	initial_button.set_no_show_all (true);
	initial_button.set_name (X_("vca assign"));
	initial_button.set_text (_("-VCAs-"));
	initial_button.show ();
	initial_button.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	initial_button.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &ControlSlaveUI::vca_button_release), 0), false);

	pack_start (initial_button, true, true);
}

ControlSlaveUI::~ControlSlaveUI ()
{
	delete context_menu;
}

void
ControlSlaveUI::set_stripable (boost::shared_ptr<Stripable> s)
{
	connections.drop_connections ();

	stripable = s;

	if (stripable) {
		boost::shared_ptr<GainControl> ac = stripable->gain_control();
		assert (ac);

		ac->MasterStatusChange.connect (connections,
		                                invalidator (*this),
		                                boost::bind (&ControlSlaveUI::update_vca_display, this),
		                                gui_context());

		stripable->DropReferences.connect (connections, invalidator (*this), boost::bind (&ControlSlaveUI::set_stripable, this, boost::shared_ptr<Stripable>()), gui_context());
	}

	update_vca_display ();
}

void
ControlSlaveUI::update_vca_display ()
{
	if (!_session || _session->deletion_in_progress()) {
		return;
	}

	VCAList vcas (_session->vca_manager().vcas());
	bool any = false;

	Gtkmm2ext::container_clear (*this);
	master_connections.drop_connections ();

	if (stripable) {
		for (VCAList::iterator v = vcas.begin(); v != vcas.end(); ++v) {
			if (stripable->gain_control()->slaved_to ((*v)->gain_control())) {
				add_vca_button (*v);
				any = true;
			}
		}
	}

	if (!any) {
		pack_start (initial_button, true, true);
	}

	show ();
}

void
ControlSlaveUI::vca_menu_toggle (Gtk::CheckMenuItem* menuitem, uint32_t n)
{
	boost::shared_ptr<VCA> vca = _session->vca_manager().vca_by_number (n);

	if (!vca) {
		return;
	}

	boost::shared_ptr<Slavable> sl = boost::dynamic_pointer_cast<Slavable> (stripable);

	if (!sl) {
		return;
	}

	if (!menuitem->get_active()) {
		sl->unassign (vca);
	} else {
		sl->assign (vca, false);
	}
}

void
ControlSlaveUI::unassign_all ()
{
	boost::shared_ptr<Slavable> sl = boost::dynamic_pointer_cast<Slavable> (stripable);

	if (!sl) {
		return;
	}

	sl->unassign (boost::shared_ptr<VCA>());
}

bool
ControlSlaveUI::specific_vca_button_release (GdkEventButton* ev, uint32_t n)
{
	return vca_button_release  (ev, n);
}

bool
ControlSlaveUI::vca_button_release (GdkEventButton* ev, uint32_t n)
{
	using namespace Gtk::Menu_Helpers;

	if (!_session) {
		return false;
	}

	/* primary click only */

	if (ev->button != 1) {
		return false;
	}

	if (!stripable) {
		/* no route - nothing to do */
		return false;
	}

	VCAList vcas (_session->vca_manager().vcas());

	if (vcas.empty()) {
		/* the button should not have been visible under these conditions */
		return true;
	}

	delete context_menu;
	context_menu = new Menu;
	MenuList& items = context_menu->items();
	bool slaved = false;

	for (VCAList::iterator v = vcas.begin(); v != vcas.end(); ++v) {

		boost::shared_ptr<GainControl> gcs = stripable->gain_control();
		boost::shared_ptr<GainControl> gcm = (*v)->gain_control();

		if (gcs == gcm) {
			/* asked to slave to self. not ok */
			continue;
		}

		if (gcm->slaved_to (gcs)) {
			/* master is already slaved to slave */
			continue;
		}

		items.push_back (CheckMenuElem ((*v)->name()));
		Gtk::CheckMenuItem* item = dynamic_cast<Gtk::CheckMenuItem*> (&items.back());

		if (gcs->slaved_to (gcm)) {
			item->set_active (true);
			slaved = true;
		}

		item->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &ControlSlaveUI::vca_menu_toggle), item, (*v)->number()));
	}

	if (slaved) {
		items.push_back (MenuElem (_("Unassign All"), sigc::mem_fun (*this, &ControlSlaveUI::unassign_all)));
	}

	if (!items.empty()) {
		context_menu->popup (1, ev->time);
	}

	return true;
}

void
ControlSlaveUI::add_vca_button (boost::shared_ptr<VCA> vca)
{
	ArdourButton* vca_button = manage (new ArdourButton (ArdourButton::default_elements));

	vca_button->set_no_show_all (true);
	vca_button->set_name (X_("vca assign"));
	vca_button->add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	vca_button->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &ControlSlaveUI::specific_vca_button_release), vca->number()), false);
	vca_button->set_text (PBD::to_string (vca->number()));
	vca_button->set_fixed_colors (vca->presentation_info().color(), vca->presentation_info().color ());

	vca->presentation_info().PropertyChanged.connect (master_connections, invalidator (*this), boost::bind (&ControlSlaveUI::master_property_changed, this, _1), gui_context());

	pack_start (*vca_button);
	vca_button->show ();
}

void
ControlSlaveUI::master_property_changed (PBD::PropertyChange const& /* what_changed */)
{
	update_vca_display ();
}
