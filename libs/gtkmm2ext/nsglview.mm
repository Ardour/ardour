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

/* include order matter due to apple defines */
#include <gtkmm/window.h>

#include "gtkmm2ext/cairo_canvas.h"
#include "gtkmm2ext/nsglview.h"
#include "gtkmm2ext/rgb_macros.h"

#include <gdk/gdkquartz.h>

//#define NSVIEW_PROFILE
//#define DEBUG_NSVIEW_EXPOSURE

#include <OpenGL/gl.h>
#import  <Cocoa/Cocoa.h>

/* the gtk-quartz library which ardour links against
 * is patched to pass events directly through to
 * NSView child-views (AU Plugin GUIs).
 *
 * In this particular case however we do want the
 * events to reach the GTK widget instead of the
 * NSView subview.
 *
 * If a NSVIew tag equals to the given magic-number,
 * Gdk events propagate.
 */
#ifndef ARDOUR_CANVAS_NSVIEW_TAG
#error NO TAG
#define ARDOUR_CANVAS_NSVIEW_TAG 0x0
#endif

__attribute__ ((visibility ("hidden")))
@interface ArdourCanvasOpenGLView : NSOpenGLView
{
@private
	Cairo::RefPtr<Cairo::ImageSurface> _surf;
	Gtkmm2ext::CairoCanvas*            _cairo_canvas;

	NSInteger    _tag;
	unsigned int _texture_id;
	int          _width;
	int          _height;
	float        _scale;
	bool         _use_backing_scale;
	GdkRectangle _expose_area;
}

@property (readwrite) NSInteger tag;

- (instancetype)initWithScale:(BOOL)use_backing_scale;
- (id) initWithFrame:(NSRect)frame;
- (void) dealloc;
- (void) setCairoCanvas:(Gtkmm2ext::CairoCanvas*)canvas;
- (void) reshape;
- (void) setNeedsDisplayInRect:(NSRect)rect;
- (void) setNeedsDisplay:(BOOL)yn;
- (void) drawRect:(NSRect)rect;
- (BOOL) canBecomeKeyWindow:(id)sender;
- (BOOL) acceptsFirstResponder:(id)sender;
- (void) reallocateTexture;

@end

@implementation ArdourCanvasOpenGLView

@synthesize tag = _tag;

- (id) initWithFrame:(NSRect)frame
{
	NSOpenGLPixelFormatAttribute pixelAttribs[16] = {
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAAccelerated,
		NSOpenGLPFAColorSize, 32,
		NSOpenGLPFADepthSize, 32,
		NSOpenGLPFAMultisample,
		NSOpenGLPFASampleBuffers, 1,
		NSOpenGLPFASamples, 4,
		0
	};

	NSOpenGLPixelFormat* pixelFormat =
		[[NSOpenGLPixelFormat alloc] initWithAttributes:pixelAttribs];

	if (pixelFormat) {
		self = [super initWithFrame:frame pixelFormat:pixelFormat];
		[pixelFormat release];
	} else {
		self = [super initWithFrame:frame];
	}

	_texture_id = 0;
	_width      = 0;
	_height     = 0;
	_scale      = 1.;

	_expose_area.x = _expose_area.y = _expose_area.width = _expose_area.height = 0;

	if (self) {
		self.tag = ARDOUR_CANVAS_NSVIEW_TAG;
		[[self openGLContext] makeCurrentContext];
		glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
		glDisable (GL_DEPTH_TEST);
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable (GL_TEXTURE_RECTANGLE_ARB);
		[NSOpenGLContext clearCurrentContext];

		if (_use_backing_scale) {
			[self setWantsBestResolutionOpenGLSurface:YES];
			[self setWantsLayer:YES];
		} else {
			[self setWantsBestResolutionOpenGLSurface:NO];
		}

		[self reshape];
	}

	return self;
}

- (instancetype)initWithScale:(BOOL)use_backing_scale
{
  _use_backing_scale = use_backing_scale;
	[super init];
	return self;
}


- (void) dealloc {
	[[self openGLContext] makeCurrentContext];
	glDeleteTextures (1, &_texture_id);
	[NSOpenGLContext clearCurrentContext];

	[super dealloc];
}

- (void) setCairoCanvas:(Gtkmm2ext::CairoCanvas*)canvas
{
	_cairo_canvas = canvas;
}

- (BOOL) canBecomeKeyWindow:(id)sender{
	return NO;
}

- (BOOL) acceptsFirstResponder:(id)sender{
	return NO;
}

- (void) reallocateTexture
{
	/* must be called withing GLContext */
	int const w = _width * _scale;
	int const h = _height * _scale;

	glViewport (0, 0, w, h);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	glOrtho (-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

	glClear (GL_COLOR_BUFFER_BIT);

	glDeleteTextures (1, &_texture_id);
	glGenTextures (1, &_texture_id);
	glBindTexture (GL_TEXTURE_RECTANGLE_ARB, _texture_id);
	glTexImage2D (GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8,
			w, h, 0,
			GL_BGRA, GL_UNSIGNED_BYTE, NULL);
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();


	_expose_area.x = _expose_area.y = 0;
	_expose_area.width  = _width;
	_expose_area.height = _height;
}

- (void) reshape
{
	[super reshape];
	[[self openGLContext] update];

	NSRect bounds = [self bounds];
	int    width  = bounds.size.width;
	int    height = bounds.size.height;
	float  scale = 1.0;

	if (_use_backing_scale && [self window]) {
		scale = [[self window] backingScaleFactor];
	}

	if (_width == width && _height == height && _scale == scale) {
		return;
	}

	_width  = width;
	_height = height;
	_scale  = scale;

	[[self openGLContext] makeCurrentContext];
	[self reallocateTexture];
	[NSOpenGLContext clearCurrentContext];
}

- (void) setNeedsDisplay:(BOOL)yn
{
	if (yn) {
		_expose_area.x      = 0;
		_expose_area.y      = 0;
		_expose_area.width  = _width;
		_expose_area.height = _height;
#ifdef DEBUG_NSVIEW_EXPOSURE
		printf ("setNeedsDisplay:       %4d x %4d\n", _width, _height);
#endif
	}
	[super setNeedsDisplay:yn];
}

- (void) setNeedsDisplayInRect:(NSRect)rect
{
#ifdef DEBUG_NSVIEW_EXPOSURE
	printf ("setNeedsDisplayInRect: %4.0f x %4.0f   @ %.0f + %.0f\n",
			rect.size.width, rect.size.height, rect.origin.x, rect.origin.y);
#endif

	if (_expose_area.width == 0 && _expose_area.height == 0) {
		_expose_area.x      = rect.origin.x;
		_expose_area.y      = rect.origin.y;
		_expose_area.width  = rect.size.width;
		_expose_area.height = rect.size.height;
	} else {
		GdkRectangle r1 = {(int)rect.origin.x, (int)rect.origin.y, (int)rect.size.width, (int)rect.size.height};
		gdk_rectangle_union (&r1, &_expose_area, &_expose_area);
	}

	GdkRectangle rw = {0, 0, _width, _height};
	gdk_rectangle_intersect (&rw, &_expose_area, &_expose_area);

	[super setNeedsDisplayInRect:rect];
}

- (void) drawRect:(NSRect)rect
{
#ifdef NSVIEW_PROFILE
	const int64_t start = g_get_monotonic_time ();
#endif

	float  scale = 1.0;

	if (_use_backing_scale && [self window]) {
		scale = [[self window] backingScaleFactor];
	}

	if ( _scale != scale) {
		_scale = scale;
		[[self openGLContext] update];
		[[self openGLContext] makeCurrentContext];
		[self reallocateTexture];
	} else {
		[[self openGLContext] makeCurrentContext];
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClear(GL_COLOR_BUFFER_BIT);

	/* call back into CairoCanvas */
	cairo_rectangle_t cairo_rect;

	cairo_rect.x      = _expose_area.x;
	cairo_rect.y      = _expose_area.y;
	cairo_rect.width  = _expose_area.width;
	cairo_rect.height = _expose_area.height;
	_expose_area.x = _expose_area.y = _expose_area.width = _expose_area.height = 0;

	int const w = _width * _scale;
	int const h = _height * _scale;

	if (!_surf || _surf->get_width () != w || _surf->get_height() != h) {
		_surf = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, w, h);

		cairo_rect.x = 0;
		cairo_rect.y = 0;
		cairo_rect.width  = _width;
		cairo_rect.height = _height;
	}

#ifdef DEBUG_NSVIEW_EXPOSURE
	printf ("NSGL::drawRect: ...... %4.0f x %4.0f   @ %.0f + %.0f\n",
			cairo_rect.width, cairo_rect.height, cairo_rect.x, cairo_rect.y);
#endif


	Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create (_surf);
	ctx->scale (_scale, _scale);

	ctx->rectangle (cairo_rect.x, cairo_rect.y, cairo_rect.width, cairo_rect.height);
	ctx->clip ();
	{
		/* draw background color */
		uint32_t col = _cairo_canvas->background_color ();
		int r, g, b, a;
		UINT_TO_RGBA (col, &r, &g, &b, &a);
		ctx->set_source_rgba (r/255.0, g/255.0, b/255.0, a/255.0);
		ctx->paint ();
	}

	_cairo_canvas->render (ctx, &cairo_rect);

	_surf->flush ();
	uint8_t* imgdata = _surf->get_data ();

	/* NOTE for big-endian (PPC), we'd need to flip byte-order
	 * RGBA <> RGBA for the texture.
	 * GtkCanvas does not use this nsview for PPC builds, yet
	 */

	/* continue OpenGL */
	glPushMatrix ();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texture_id);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8,
			w, h, /*border*/ 0,
			GL_BGRA, GL_UNSIGNED_BYTE, imgdata);

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, (GLfloat) h);
	glVertex2f(-1.0f, -1.0f);

	glTexCoord2f((GLfloat) w, (GLfloat) h);
	glVertex2f(1.0f, -1.0f);

	glTexCoord2f((GLfloat) w, 0.0f);
	glVertex2f(1.0f,  1.0f);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(-1.0f,  1.0f);
	glEnd();

	glDisable(GL_TEXTURE_2D);
	glPopMatrix();

	glFlush();
	glSwapAPPLE();
	[NSOpenGLContext clearCurrentContext];
	[super setNeedsDisplay:NO];

#ifdef NSVIEW_PROFILE
	const int64_t end = g_get_monotonic_time ();
	const int64_t elapsed = end - start;
	printf ("NSGL::drawRect (%d x %d) * %.1f in %f ms\n", _width, _height, _scale, elapsed / 1000.f);
#endif
}
@end

void*
Gtkmm2ext::nsglview_create (Gtkmm2ext::CairoCanvas* canvas)
{
	return nsglview_create (canvas, true);
}

void*
Gtkmm2ext::nsglview_create (Gtkmm2ext::CairoCanvas* canvas, bool use_backing_scale)
{
	if (g_getenv ("ARDOUR_NSGL") && (0 == atoi (g_getenv ("ARDOUR_NSGL")))) {
		return 0;
	}
/* the API is currently only used on intel mac
 * for big-endian  RGBA <> RGBA byte order of the texture
 * will have to be swapped.
 */
#ifdef __ppc__
	return 0;
#endif
	ArdourCanvasOpenGLView* gl_view = [[ArdourCanvasOpenGLView alloc] initWithScale:use_backing_scale];
	if (!gl_view) {
		return 0;
	}
	[gl_view setCairoCanvas:canvas];
	[gl_view setHidden:YES];
	return gl_view;
}

void
Gtkmm2ext::nsglview_overlay (void* glv, GdkWindow* window)
{
	ArdourCanvasOpenGLView* gl_view = (ArdourCanvasOpenGLView*) glv;
	NSView* view = gdk_quartz_window_get_nsview (window);
	[view addSubview:gl_view];
}

void
Gtkmm2ext::nsglview_resize (void* glv, int x, int y, int w, int h)
{
	ArdourCanvasOpenGLView* gl_view = (ArdourCanvasOpenGLView*) glv;
	[gl_view setFrame:NSMakeRect(x, y, w, h)];
}

void
Gtkmm2ext::nsglview_queue_draw (void* glv, int x, int y, int w, int h)
{
	ArdourCanvasOpenGLView* gl_view = (ArdourCanvasOpenGLView*) glv;
	[gl_view setNeedsDisplayInRect:NSMakeRect(x, y, w, h)];
}

void
Gtkmm2ext::nsglview_set_visible (void* glv, bool vis)
{
	ArdourCanvasOpenGLView* gl_view = (ArdourCanvasOpenGLView*) glv;
	if (vis) {
		[gl_view setHidden:NO];
	} else {
		[gl_view setHidden:YES];
	}
}
