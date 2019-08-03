/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "pbd/error.h"
#include "pbd/replace_all.h"
#include "pbd/string_convert.h"

#include "ardour/boost_debug.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/slavable.h"
#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Glib::Threads;
using namespace PBD;
using std::string;

string VCAManager::xml_node_name (X_("VCAManager"));

VCAManager::VCAManager (Session& s)
	: SessionHandleRef (s)
	, _vcas_loaded (false)
{
}

VCAManager::~VCAManager ()
{
	clear ();
}

void
VCAManager::clear ()
{
	bool send = false;
	{
		Mutex::Lock lm (lock);
		for (VCAList::const_iterator i = _vcas.begin(); i != _vcas.end(); ++i) {
			if ((*i)->is_selected ()) {
				_session.selection().remove_stripable_by_id ((*i)->id());
				send = true;
			}
			(*i)->DropReferences ();
		}
		_vcas.clear ();
	}

	if (send && !_session.deletion_in_progress ()) {
		PropertyChange pc;
		pc.add (Properties::selected);
		PresentationInfo::Change (pc);
	}
}

VCAList
VCAManager::vcas () const
{
	Mutex::Lock lm (lock);
	return _vcas;
}

VCAList
VCAManager::create_vca (uint32_t howmany, std::string const & name_template)
{
	VCAList vcal;

	uint32_t n_stripables = _session.nstripables ();

	{
		Mutex::Lock lm (lock);

		for (uint32_t n = 0; n < howmany; ++n) {

			int num = VCA::next_vca_number ();
			string name = name_template;

			if (name.find ("%n")) {
				string sn = PBD::to_string (num);
				replace_all (name, "%n", sn);
			}

			boost::shared_ptr<VCA> vca = boost::shared_ptr<VCA> (new VCA (_session, num, name));
			BOOST_MARK_VCA (vca);

			vca->init ();
			vca->set_presentation_order (n + n_stripables);

			_vcas.push_back (vca);
			vcal.push_back (vca);
		}
	}

	VCAAdded (vcal); /* EMIT SIGNAL */

	if (!vcal.empty ()) {
		VCACreated (); /* EMIT SIGNAL */
	}

	_session.set_dirty ();

	return vcal;
}

void
VCAManager::remove_vca (boost::shared_ptr<VCA> vca)
{
	{
		Mutex::Lock lm (lock);
		_vcas.remove (vca);
	}

	/* this should cause deassignment and deletion */

	vca->DropReferences ();

	if (vca->is_selected () && !_session.deletion_in_progress ()) {
		_session.selection().remove_stripable_by_id (vca->id());
		PropertyChange pc;
		pc.add (Properties::selected);
		PresentationInfo::Change (pc);
	}
	_session.set_dirty ();
}

boost::shared_ptr<VCA>
VCAManager::vca_by_number (int32_t n) const
{
	Mutex::Lock lm (lock);

	for (VCAList::const_iterator i = _vcas.begin(); i != _vcas.end(); ++i) {
		if ((*i)->number() == n) {
			return *i;
		}
	}

	return boost::shared_ptr<VCA>();
}

boost::shared_ptr<VCA>
VCAManager::vca_by_name (std::string const& name) const
{
	Mutex::Lock lm (lock);

	for (VCAList::const_iterator i = _vcas.begin(); i != _vcas.end(); ++i) {
		if ((*i)->name() == name || (*i)->full_name() == name) {
			return *i;
		}
	}

	return boost::shared_ptr<VCA>();
}

XMLNode&
VCAManager::get_state ()
{
	XMLNode* node = new XMLNode (xml_node_name);

	{
		Mutex::Lock lm (lock);

		for (VCAList::const_iterator i = _vcas.begin(); i != _vcas.end(); ++i) {
			node->add_child_nocopy ((*i)->get_state());
		}
	}

	return *node;
}

int
VCAManager::set_state (XMLNode const& node, int version)
{
	if (node.name() != xml_node_name) {
		return -1;
	}

	XMLNodeList const & children = node.children();
	VCAList vcal;

	_vcas_loaded = false;

	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() == VCA::xml_node_name) {
			boost::shared_ptr<VCA> vca = boost::shared_ptr<VCA> (new VCA (_session, 0, X_("tobereset")));
			BOOST_MARK_VCA (vca);

			if (vca->init() || vca->set_state (**i, version)) {
				error << _("Cannot set state of a VCA") << endmsg;
				return -1;
			}

			/* can't hold the lock for the entire loop,
			 * because the new VCA maybe slaved and needs
			 * to call back into us to set up its own
			 * slave/master relationship
			 */

			{
				Mutex::Lock lm (lock);
				_vcas.push_back (vca);
				vcal.push_back (vca);
			}
		}
	}

	_vcas_loaded = true;

	VCAAdded (vcal); /* EMIT SIGNAL */

	return 0;
}

void
VCAManager::clear_all_solo_state ()
{
	Mutex::Lock lm (lock);

	for (VCAList::const_iterator i = _vcas.begin(); i != _vcas.end(); ++i) {
		(*i)->clear_all_solo_state ();
	}
}
