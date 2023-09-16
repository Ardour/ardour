/*
  Copyright (C) 2023 Paul Davis

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software Foundation,
  Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __libtemporal_bbt_argument_h__
#define __libtemporal_bbt_argument_h__

#include "temporal/bbt_time.h"
#include "temporal/timeline.h"

namespace Temporal {

struct LIBTEMPORAL_API BBT_Argument : public BBT_Time
{
  public:
	BBT_Argument () : BBT_Time (), _reference (0) {}
	BBT_Argument (int32_t B, int32_t b, int32_t t) : BBT_Time (B, b, t),  _reference (0) {}

	BBT_Argument (superclock_t r) : BBT_Time (), _reference (r) {}
	BBT_Argument (superclock_t r, int32_t B, int32_t b, int32_t t) : BBT_Time (B, b, t), _reference (r) {}

	explicit BBT_Argument (BBT_Time const & bbt) : BBT_Time (bbt),  _reference (0) {}
	BBT_Argument (superclock_t r, BBT_Time const & bbt) : BBT_Time (bbt),  _reference (r) {}

	superclock_t reference() const { return _reference; }

  private:
	superclock_t _reference;

};

} // end namespace

namespace std {

LIBTEMPORAL_API std::ostream& operator<< (std::ostream& o, Temporal::BBT_Argument const & bbt);

}

#endif /* __libtemporal_bbt_argument_h__ */
