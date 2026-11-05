/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#pragma once

#include <vector>
#include <list>

#include "pbd/rwlock.h"
#include "pbd/stateful.h"

#include "ardour/stripable.h"

namespace ARDOUR {

class Track;

class LIBARDOUR_API Folder : public Stripable, virtual public PBD::Stateful
{
  public:
	Folder (Session &, std::string const &, PresentationInfo const &);

	int set_state (XMLNode const&, int);
	XMLNode& get_state() const;

	void get_contents (std::vector<std::shared_ptr<Stripable>> &) const;
	void get_content_tracks (std::vector<std::shared_ptr<Track>> &) const;

	virtual void add (std::shared_ptr<Stripable>);
	virtual void add (std::vector<std::shared_ptr<Stripable>>);
	virtual void remove (std::shared_ptr<Stripable>);
	virtual void remove (std::vector<std::shared_ptr<Stripable>>);
	virtual void clear ();

  protected:
	mutable PBD::RWLock contents_lock;
	std::list<std::shared_ptr<Stripable>> _contents;
	PBD::ScopedConnectionList content_connections;

	void content_going_away (std::weak_ptr<Stripable>);
};

}
