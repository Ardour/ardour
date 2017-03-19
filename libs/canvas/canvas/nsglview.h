#ifndef __CANVAS_NSGLVIEW_H__
#define __CANVAS_NSGLVIEW_H__

#include <gdk/gdk.h>

namespace ArdourCanvas
{
	class GtkCanvas;

	void* nsglview_create (GtkCanvas*);
	void  nsglview_overlay (void*, GdkWindow*);
	void  nsglview_resize (void*, int x, int y, int w, int h);
	void  nsglview_queue_draw (void*, int x, int y, int w, int h);
}
#endif
