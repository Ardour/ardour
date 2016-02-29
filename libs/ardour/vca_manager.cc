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

#include "pbd/convert.h"
#include "pbd/replace_all.h"

#include "ardour/vca.h"
#include "ardour/vca_manager.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Glib::Threads;
using std::string;

string VCAManager::xml_node_name (X_("VCAManager"));

VCAManager::VCAManager (Session& s)
	: SessionHandleRef (s)
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
				string sn = PBD::to_string (n, std::dec);
				replace_all (name, "%n", sn);
			}

			boost::shared_ptr<VCA> vca = boost::shared_ptr<VCA> (new VCA (_session, name, num));

			_vcas.push_back (vca);
			vcal.push_back (vca);
		}
	}

	VCAAdded (vcal); /* EMIT SIGNAL */

	return 0;
}


void
VCAManager::remove_vca (boost::shared_ptr<VCA> vca)
{
	{
		Mutex::Lock lm (lock);
		_vcas.remove (vca);
	}

	VCAList vcal;
	vcal.push_back (vca);

	VCARemoved (vcal); /* EMIT SIGNAL */
}

boost::shared_ptr<VCA>
VCAManager::vca_by_number (uint32_t n) const
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

	{

		Mutex::Lock lm (lock);

		for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
			if ((*i)->name() == VCA::xml_node_name) {
				std::cerr << "Adding VCA from XML\n";
				boost::shared_ptr<VCA> vca = boost::shared_ptr<VCA> (new VCA (_session, **i, version));
				_vcas.push_back (vca);
				vcal.push_back (vca);
			}
		}
	}

	VCAAdded (vcal); /* EMIT SIGNAL */

	return 0;
}
