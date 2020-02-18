/*
 * Copyright (C) 2014-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __libardour_scene_change_h__
#define __libardour_scene_change_h__

#include "pbd/stateful.h"

#include "ardour/types.h"

namespace ARDOUR
{

class SceneChange : public PBD::Stateful
{
  public:
	SceneChange ();
	virtual ~SceneChange () {};

	static boost::shared_ptr<SceneChange> factory (const XMLNode&, int version);
	static std::string xml_node_name;

        uint32_t color() const;
        void set_color (uint32_t);
        bool color_out_of_bounds() const { return _color == out_of_bound_color; }
        static const uint32_t out_of_bound_color;

        bool active () const { return _active; }
        void set_active (bool);

        PBD::Signal0<void> ColorChanged;
        PBD::Signal0<void> ActiveChanged;

    protected:
        /* derived classes are responsible for serializing & deserializing this value */
        uint32_t _color;
        bool     _active;
};

} /* namespace */


#endif /* __libardour_scene_change_h__ */
