#ifndef _WIDGETS_UI_BASE_H_
#define _WIDGETS_UI_BASE_H_

#include <cassert>

#include "pbd/stateful.h"
#include "canvas/colors.h"

#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API UIConfigurationBase : public PBD::Stateful
{
protected:
	virtual ~UIConfigurationBase() { _instance = 0; }
	static UIConfigurationBase* _instance;

public:
	static UIConfigurationBase& instance() { return *_instance; }

	sigc::signal<void>  DPIReset;
	sigc::signal<void>  ColorsChanged;

	virtual float get_ui_scale () = 0;
	virtual bool get_widget_prelight () const = 0;
	virtual ArdourCanvas::Color color (const std::string&, bool* failed = 0) const = 0;
};

}
#endif
