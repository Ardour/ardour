/*
 * Copyright (C) 2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2021 Robin Gareus <robin@gareus.org>
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

#ifndef _WIDGETS_ARDOUR_KNOB_H_
#define _WIDGETS_ARDOUR_KNOB_H_

#include "widgets/ardour_ctrl_base.h"
#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API ArdourKnob : public ArdourCtrlBase
{
public:
	enum Element {
		Arc     = 0x1,
		Bevel   = 0x2,
		unused2 = 0x4,
		unused3 = 0x8,
		unused4 = 0x10,
		unused5 = 0x20,
	};

	ArdourKnob (Element e = default_elements, Flags flags = NoFlags);

	Element elements () const
	{
		return _elements;
	}
	void set_elements (Element);
	void add_elements (Element);

	static Element default_elements;

	void gen_faceplate (Pango::FontDescription const&, std::string const&, std::string const&, std::string const&);

protected:
	virtual void render (Cairo::RefPtr<Cairo::Context> const&, cairo_rectangle_t*);
	void         on_size_request (Gtk::Requisition* req);

private:
	Element _elements;
};

} // namespace ArdourWidgets

#endif /* __gtk2_ardour_ardour_knob_h__ */
