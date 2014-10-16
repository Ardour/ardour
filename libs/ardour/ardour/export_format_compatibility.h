/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#ifndef __ardour_export_format_compatibility_h__
#define __ardour_export_format_compatibility_h__

#include <string>
#include "ardour/export_format_base.h"

namespace ARDOUR
{

/// Allows adding to all sets. A format should be able to test if it is compatible with this
class LIBARDOUR_API ExportFormatCompatibility : public ExportFormatBase, public ExportFormatBase::SelectableCompatible {
  private:

  public:
	ExportFormatCompatibility (std::string name)
	{
		set_name (name);
		sample_formats.insert (SF_None);
		sample_rates.insert (SR_None);
		format_ids.insert (F_None);
		qualities.insert (Q_None);
	}

	~ExportFormatCompatibility () {};

	ExportFormatCompatibility (ExportFormatBase const & other)
		: ExportFormatBase (other) {}

	void add_endianness (Endianness endianness) { endiannesses.insert (endianness); }
	void add_sample_format (SampleFormat format) { sample_formats.insert (format); }
	void add_sample_rate (SampleRate rate) { sample_rates.insert (rate); }
	void add_format_id (FormatId id) { format_ids.insert (id); }
	void add_quality (Quality quality) { qualities.insert (quality); }
};

} // namespace ARDOUR

#endif /* __ardour_export_format_compatibility_h__ */
