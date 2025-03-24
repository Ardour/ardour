/*
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class Session;

class LIBARDOUR_API AudioReadable {
public:
	virtual ~AudioReadable() {}

	static std::vector<std::shared_ptr<AudioReadable> >
		load (Session&, std::string const&);

	virtual samplecnt_t read (Sample*, samplepos_t pos, samplecnt_t cnt, int channel) const = 0;
	virtual samplecnt_t readable_length_samples() const = 0;
	virtual uint32_t  n_channels () const = 0;
};

}

