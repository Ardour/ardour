/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef _WIDGETS_UI_BASE_H_
#define _WIDGETS_UI_BASE_H_

#include <cassert>

#include <pangomm/fontdescription.h>

#include "pbd/configuration.h"
#include "gtkmm2ext/colors.h"

#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API UIConfigurationBase : public PBD::Configuration
{
protected:
	virtual ~UIConfigurationBase() { _instance = 0; }
	static UIConfigurationBase* _instance;

public:
	static UIConfigurationBase& instance() { return *_instance; }

	sigc::signal<void>  DPIReset;
	sigc::signal<void>  ColorsChanged;

	virtual float get_ui_scale () = 0;
	virtual bool get_all_floating_windows_are_dialogs () const = 0;
	virtual bool get_widget_prelight () const = 0;
	virtual Gtkmm2ext::Color color (const std::string&, bool* failed = 0) const = 0;

	virtual Pango::FontDescription get_NormalFont () const = 0;
	virtual Pango::FontDescription get_SmallFont () const = 0;
	virtual Pango::FontDescription get_NormalMonospaceFont () const = 0;
	virtual Pango::FontDescription get_SmallMonospaceFont () const = 0;
	virtual Pango::FontDescription get_ArdourSmallFont () const = 0;
};

}
#endif
