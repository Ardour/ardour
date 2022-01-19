/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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
#ifndef _gtk_ardour_source_list_base_h_
#define _gtk_ardour_source_list_base_h_

#include "region_list_base.h"

class SourceListBase : public RegionListBase
{
public:
	SourceListBase ();
	void set_session (ARDOUR::Session*);

protected:
	void name_edit (const std::string&, const std::string&);
	void tag_edit (const std::string&, const std::string&);
	bool list_region (boost::shared_ptr<ARDOUR::Region>) const;

private:
	void remove_source (boost::shared_ptr<ARDOUR::Source>);
	void remove_weak_source (boost::weak_ptr<ARDOUR::Source>);
};

#endif /* _gtk_ardour_source_list_base_h_ */
