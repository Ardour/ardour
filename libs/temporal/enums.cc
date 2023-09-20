/*
 * Copyright (C) 2020 Paul Davis <paul@linuxaudiosystems.com>
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

#include <typeinfo>

#include "pbd/enumwriter.h"

#include "temporal/types.h"
#include "temporal/tempo.h"

using namespace PBD;
using namespace Temporal;
using namespace std;

static bool libtemporal_initialized = false;

void
setup_libtemporal_enums ()
{
	EnumWriter& enum_writer (EnumWriter::instance());
	vector<int> i;
	vector<string> s;

	Temporal::TimeDomain td;
	Temporal::OverlapType _OverlapType;
	Temporal::Tempo::Type _TempoType;

#define REGISTER(e) enum_writer.register_distinct (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_BITS(e) enum_writer.register_bits (typeid(e).name(), i, s); i.clear(); s.clear()
#define REGISTER_ENUM(e) i.push_back (e); s.push_back (#e)
#define REGISTER_CLASS_ENUM(t,e) i.push_back (t::e); s.push_back (#e)

	REGISTER_ENUM (AudioTime);
	REGISTER_ENUM (BeatTime);
	REGISTER (td);

	REGISTER_ENUM (Temporal::OverlapNone);
	REGISTER_ENUM (Temporal::OverlapInternal);
	REGISTER_ENUM (Temporal::OverlapStart);
	REGISTER_ENUM (Temporal::OverlapEnd);
	REGISTER_ENUM (Temporal::OverlapExternal);
	REGISTER(_OverlapType);


	REGISTER_CLASS_ENUM (Tempo, Ramped);
	REGISTER_CLASS_ENUM (Tempo, Constant);
	REGISTER (_TempoType);

	enum_writer.add_to_hack_table ("Tempo::Ramped", "Ramped");
	enum_writer.add_to_hack_table ("Tempo::Constant", "Constant");
}

void Temporal::init ()
{
	if (!libtemporal_initialized) {
		setup_libtemporal_enums ();

		TempoMap::init ();

		libtemporal_initialized = true;
	}
}

void Temporal::reset ()
{
	set_superclock_ticks_per_second (282240000);  /* 2^10 * 3^2 * 5^4 * 7^2 */

	(void) TempoMap::write_copy(); /* we are going to throw away the return value and replace the map entirely */
	TempoMap::WritableSharedPtr new_map (new TempoMap (Tempo (120, 4), Meter (4, 4)));
	TempoMap::update (new_map);;
}

std::ostream&
operator<< (std::ostream& o, Temporal::ratio_t const & r)
{
	return o << r.numerator() << '/' << r.denominator();
}

