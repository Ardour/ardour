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

#ifndef __canvas_stateful_image_h__
#define __canvas_stateful_image_h__

#include <string>
#include <vector>
#include <map>

#include <cairomm/cairomm.h>

#include "canvas/item.h"

class XMLNode;

namespace Pango {
	class FontDescription;
}

namespace ArdourCanvas {

class StatefulImage : public Item
{
  private:
    typedef Cairo::RefPtr<Cairo::ImageSurface> ImageHandle;

    class State {
      public:
	ImageHandle image;
    };

    typedef std::vector<State> States;

  public:

    StatefulImage (Canvas*, const XMLNode&);
    StatefulImage (Item*, const XMLNode&);
    ~StatefulImage ();

    bool set_state (States::size_type);
    void set_text (const std::string&);

    void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
    void compute_bounding_box () const;

    static void set_image_search_path (const std::string&);

  private:
    States                  _states;
    States::size_type       _state;
    std::string             _text;
    Pango::FontDescription* _font;
    uint32_t                _text_color;
    double                  _text_x;
    double                  _text_y;

    int load_states (const XMLNode&);

    typedef std::map<std::string,Cairo::RefPtr<Cairo::ImageSurface> > ImageCache;
    static ImageCache _image_cache;
    static PBD::Searchpath _image_search_path;

    static ImageHandle find_image (const std::string&);
};

}

#endif /* __canvas_stateful_image_h__ */
