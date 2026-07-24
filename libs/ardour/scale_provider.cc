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
#include "ardour/types_convert.h"

#include "pbd/i18n.h"

namespace ARDOUR {
	namespace Properties {
		PBD::PropertyDescriptor<bool> musical_mode; /* type is irrelevant */
		PBD::PropertyDescriptor<KeyEnforcementPolicy> key_enforcement;
	}
}

using namespace ARDOUR;

void
ScaleProvider::make_property_quarks ()
{
	Properties::musical_mode.property_id = g_quark_from_static_string (X_("musical-mode"));
	Properties::key_enforcement.property_id = g_quark_from_static_string (X_("key-enforcement"));
}

ScaleProvider::ScaleProvider (ScaleProvider* parent)
	: _parent (parent)
	, _key (nullptr)
	, _key_enforcement_policy (KeyEnforcementPolicy (0))
{
	if (parent) {
		parent->PropertyChanged.connect_same_thread (parent_connection, [this] (PBD::PropertyChange const & pc) { parent_prop_change (pc); });
	}
}

ScaleProvider::~ScaleProvider ()
{
	delete _key;
}

void
ScaleProvider::parent_prop_change (PBD::PropertyChange const & pc)
{
	send_change (pc);
}

void
ScaleProvider::set_key (MusicalKey const * k)
{
	delete _key;
	_key = k;
	send_change (Properties::musical_mode);
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

ScaleProvider const *
ScaleProvider::scale_provider_origin () const
{
	if (_key) {
		return this;
	}

	if (_parent) {
		return _parent->scale_provider_origin ();
	}

	return nullptr;
}

XMLNode&
ScaleProvider::get_state () const
{
	XMLNode* node = new XMLNode (X_("ScaleProvider"));

	if (_key) {
		node->set_property (X_("Key"), _key->name());
	}

	node->set_property (X_("key-enforcement"), _key_enforcement_policy);

	return *node;
}

int
ScaleProvider::set_state (const XMLNode& node, int /*version*/)
{
	XMLProperty const * keyname;

	if ((keyname = node.property (X_("Key"))) != 0) {
		_key = new MusicalKey (keyname->value());
	}

	node.get_property (X_("Key-enforcement"), _key_enforcement_policy);

	return 0;
}

void
ScaleProvider::set_key_enforcement_policy (KeyEnforcementPolicy kep)
{
	_key_enforcement_policy = kep;
	send_change (Properties::key_enforcement);
}

KeyEnforcementPolicy
ScaleProvider::key_enforcement_policy () const
{
	return _key_enforcement_policy;
}
