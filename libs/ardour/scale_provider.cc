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

#include "ardour/scale.h"
#include "ardour/scale_provider.h"

#include "pbd/i18n.h"

namespace ARDOUR {
	namespace Properties {
		PBD::PropertyDescriptor<bool> musical_mode; /* type is irrelevant */
	}
}

using namespace ARDOUR;

void
ScaleProvider::make_property_quarks ()
{
	Properties::musical_mode.property_id = g_quark_from_static_string (X_("musical-mode"));
}

ScaleProvider::ScaleProvider (ScaleProvider* parent)
	: _parent (parent)
	, _key (nullptr)
{
}

ScaleProvider::~ScaleProvider ()
{
	delete _key;
}

void
ScaleProvider::set_key (MusicalKey const & k)
{
	delete _key;
	_key = new MusicalKey (k);
}

MusicalKey const *
ScaleProvider::key () const
{
	if (_key) {
		return _key;
	}

	if (_parent) {
		return _parent->key();
	}

	return nullptr;
}

XMLNode&
ScaleProvider::get_state () const
{
	XMLNode* node = new XMLNode (X_("ScaleProvider"));
	return *node;
}

int
ScaleProvider::set_state (const XMLNode&, int version)
{
	return 0;
}
