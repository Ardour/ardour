#include <stdint.h>
#include <cairomm/context.h>
#include "canvas/utils.h"

void
ArdourCanvas::set_source_rgba (Cairo::RefPtr<Cairo::Context> context, Color color)
{
	context->set_source_rgba (
		((color >> 24) & 0xff) / 255.0,
		((color >> 16) & 0xff) / 255.0,
		((color >>  8) & 0xff) / 255.0,
		((color >>  0) & 0xff) / 255.0
		);
}

