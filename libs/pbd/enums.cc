/*
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/controllable.h"
#include "pbd/enumwriter.h"

using namespace PBD;
using namespace std;

void
setup_libpbd_enums ()
{
	EnumWriter& enum_writer (EnumWriter::instance());
	vector<int> i;
	vector<string> s;

	Controllable::Flag controllable_flags;
	Controllable::GroupControlDisposition controllable_group_disposition;
#define REGISTER(e) enum_writer.register_distinct (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_BITS(e) enum_writer.register_bits (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_ENUM(e) i.push_back (e); s.push_back (#e)
#define REGISTER_CLASS_ENUM(t,e) i.push_back (t::e); s.push_back (#e)

	REGISTER_CLASS_ENUM (Controllable, Toggle);
	REGISTER_CLASS_ENUM (Controllable, GainLike);
	REGISTER_CLASS_ENUM (Controllable, RealTime);
	REGISTER_CLASS_ENUM (Controllable, NotAutomatable);
	REGISTER_CLASS_ENUM (Controllable, InlineControl);
	REGISTER_CLASS_ENUM (Controllable, HiddenControl);
	REGISTER_BITS (controllable_flags);

	REGISTER_CLASS_ENUM (Controllable, InverseGroup);
	REGISTER_CLASS_ENUM (Controllable, UseGroup);
	REGISTER_CLASS_ENUM (Controllable, NoGroup);
	REGISTER_CLASS_ENUM (Controllable, ForGroup);
	REGISTER (controllable_group_disposition);

}

