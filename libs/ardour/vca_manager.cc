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

#include "pbd/error.h"
#include "pbd/replace_all.h"
#include "pbd/string_convert.h"

#include "ardour/boost_debug.h"
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
	Mutex::Lock lm (lock);
	for (VCAList::const_iterator i = _vcas.begin(); i != _vcas.end(); ++i) {
		(*i)->DropReferences ();
	}
	_vcas.clear ();
}

VCAList
VCAManager::vcas () const
{
	Mutex::Lock lm (lock);
	return _vcas;
}

int
VCAManager::create_vca (uint32_t howmany, std::string const & name_template)
{
	VCAList vcal;

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

			_vcas.push_back (vca);
			vcal.push_back (vca);
		}
	}

	VCAAdded (vcal); /* EMIT SIGNAL */

	_session.set_dirty ();

	return 0;
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
