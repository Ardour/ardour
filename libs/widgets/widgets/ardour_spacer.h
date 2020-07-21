/*
 * Copyright (C) 2011 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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

#ifndef _WIDGETS_ARDOUR_SPACER_H_
#define _WIDGETS_ARDOUR_SPACER_H_

#include "gtkmm2ext/cairo_widget.h"
#include "gtkmm2ext/colors.h"
#include "widgets/ui_config.h"

#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API ArdourVSpacer : public CairoWidget
{
public:
	ArdourVSpacer (float r = 0.75f);

protected:
	void render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t*) {

		float height = get_height();

		float h = height * ratio;
		float t = .5f * (height - h);
		ctx->rectangle (0, t, 1, h);
		uint32_t c = UIConfigurationBase::instance().color ("neutral:backgroundest");
		Gtkmm2ext::set_source_rgba (ctx, c);
		ctx->fill ();
	}

	void on_size_request (Gtk::Requisition* req) {
		req->width = 1;
		req->height = 0;
		CairoWidget::on_size_request (req);
	}

	float ratio;
};

class LIBWIDGETS_API ArdourHSpacer : public CairoWidget
{
public:
	ArdourHSpacer (float r = 0.75f);

protected:
	void render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t*) {

		float width = get_width();

		float w = width * ratio;
		float t = .5f * (width - w);
		ctx->rectangle (t, 0, w, 1);
		uint32_t c = UIConfigurationBase::instance().color ("neutral:backgroundest");
		Gtkmm2ext::set_source_rgba (ctx, c);
		ctx->fill ();
	}

	void on_size_request (Gtk::Requisition* req) {
		req->width = 0;
		req->height = 1;
		CairoWidget::on_size_request (req);
	}

	float ratio;
};

class LIBWIDGETS_API ArdourDropShadow : public CairoWidget
{
public:
	enum ShadowMode {
		DropShadowLongSideOnly,
		DropShadowBoth,
	};

	ArdourDropShadow (ShadowMode m = DropShadowLongSideOnly, float a = 0.55f);

	void set_mode(ShadowMode m) {mode = m;}

protected:
	void render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t*) {
		float width = get_width();
		float height = get_height();

		Cairo::RefPtr<Cairo::LinearGradient> _gradient;

		if ( (width>height) || mode == DropShadowBoth ) {
			_gradient = Cairo::LinearGradient::create (0, 0, 0, 4);
			_gradient->add_color_stop_rgba (0, 0, 0, 0, alpha);
			_gradient->add_color_stop_rgba (1, 0, 0, 0, 0);
			ctx->set_source (_gradient);
			ctx->rectangle (0, 0, width, 4);
			ctx->fill ();
		}

		if ( (height>width) || mode == DropShadowBoth ) {
			_gradient = Cairo::LinearGradient::create (0, 0, 4, 0);
			_gradient->add_color_stop_rgba (0, 0, 0, 0, alpha);
			_gradient->add_color_stop_rgba (1, 0, 0, 0, 0);
			ctx->set_source (_gradient);
			ctx->rectangle (0, 0, 4, height);
			ctx->fill ();
		}
	}

	float alpha;
	ShadowMode mode;
};

} /* end namespace */

#endif
