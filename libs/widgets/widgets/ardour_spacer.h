/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _WIDGETS_ARDOUR_SPACER_H_
#define _WIDGETS_ARDOUR_SPACER_H_

#include "gtkmm2ext/cairo_widget.h"

#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API ArdourVSpacer : public CairoWidget
{
public:
	ArdourVSpacer (float r = 0.75f);

protected:
	void render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t* r) {
		float h = r->height * ratio;
		float t = .5f * (r->height - h);
		ctx->rectangle (0, t, 1, h);
		ctx->set_source_rgb (0, 0, 0);
		ctx->fill ();
	}

	void on_size_request (Gtk::Requisition* req) {
		req->width = 1;
		req->height = 0;
		CairoWidget::on_size_request (req);
	}

	float ratio;
};

} /* end namespace */

#endif
