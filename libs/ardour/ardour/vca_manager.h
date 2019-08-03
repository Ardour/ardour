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

#ifndef __libardour_vca_manager_h__
#define __libardour_vca_manager_h__

#include <string>
#include <list>

#include <boost/shared_ptr.hpp>

#include <glibmm/threads.h>

#include "pbd/signals.h"
#include "pbd/statefuldestructible.h"

#include "ardour/session_handle.h"
#include "ardour/types.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class VCA;

class LIBARDOUR_API VCAManager : public SessionHandleRef, public PBD::StatefulDestructible
{
public:
	VCAManager (ARDOUR::Session&);
	~VCAManager ();

	VCAList create_vca (uint32_t how_many, std::string const & name = std::string());
	void remove_vca (boost::shared_ptr<VCA>);

	boost::shared_ptr<VCA> vca_by_number(int32_t) const;
	boost::shared_ptr<VCA> vca_by_name (std::string const&) const;

	VCAList vcas() const;
	VCAList::size_type n_vcas() const { return _vcas.size(); }

	PBD::Signal1<void,VCAList&> VCAAdded;
	PBD::Signal0<void> VCACreated; /*<< is not emitted during set_state */

	XMLNode& get_state();
	int set_state (XMLNode const&, int version);

	bool vcas_loaded() const { return _vcas_loaded; }
	void clear_all_solo_state ();

	static std::string xml_node_name;

private:
	mutable Glib::Threads::Mutex lock;
	VCAList _vcas;
	bool _vcas_loaded;

	void clear ();
};

} // namespace

#endif /* __libardour_vca_manager_h__ */
