/*
    Copyright (C) 2011 Paul Davis
    Copyright (C) 2012 David Robillard <http://drobilla.net>
    Copyright (C) 2017 Robin Gareus <robin@gareus.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/* include order matter due to apple defines */
#include <gtkmm/window.h>

#include "gtkmm2ext/cairo_canvas.h"
#include "gtkmm2ext/nsglview.h"
#include "gtkmm2ext/rgb_macros.h"

#include <gdk/gdkquartz.h>

#include <OpenGL/gl.h>
#import  <Cocoa/Cocoa.h>

#ifndef ARDOUR_CANVAS_NSVIEW_TAG
#define ARDOUR_CANVAS_NSVIEW_TAG 0xa2d0c2c4
#endif

__attribute__ ((visibility ("hidden")))
@interface ArdourCanvasOpenGLView : NSOpenGLView
{
@private
	unsigned int _texture_id;
	int _width;
	int _height;
	Cairo::RefPtr<Cairo::ImageSurface> surf;
	Gtkmm2ext::CairoCanvas *cairo_canvas;
}

@property (readwrite) NSInteger tag;

- (id) initWithFrame:(NSRect)frame;
- (void) dealloc;
- (void) setCairoCanvas:(Gtkmm2ext::CairoCanvas*)c;
- (void) reshape;
- (void) drawRect:(NSRect)rect;
- (BOOL) canBecomeKeyWindow:(id)sender;
- (BOOL) acceptsFirstResponder:(id)sender;

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
	_width = 0;
	_height = 0;

	if (self) {
		self.tag = ARDOUR_CANVAS_NSVIEW_TAG;
		[[self openGLContext] makeCurrentContext];
		glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
		glDisable (GL_DEPTH_TEST);
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable (GL_TEXTURE_RECTANGLE_ARB);
		[NSOpenGLContext clearCurrentContext];

		[self reshape];
	}

	return self;
}

- (void) dealloc {
	[[self openGLContext] makeCurrentContext];
	glDeleteTextures (1, &_texture_id);
	[NSOpenGLContext clearCurrentContext];

	[super dealloc];
}

- (void) setCairoCanvas:(Gtkmm2ext::CairoCanvas*)c
{
	cairo_canvas = c;
}

- (BOOL) canBecomeKeyWindow:(id)sender{
	return NO;
}

- (BOOL) acceptsFirstResponder:(id)sender{
	return NO;
}

- (void) reshape
{
	[[self openGLContext] update];

	NSRect bounds = [self bounds];
	int    width  = bounds.size.width;
	int    height = bounds.size.height;

	if (_width == width && _height == height) {
		return;
	}

	[[self openGLContext] makeCurrentContext];

	glViewport (0, 0, width, height);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	glOrtho (-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

	glClear (GL_COLOR_BUFFER_BIT);

	glDeleteTextures (1, &_texture_id);
	glGenTextures (1, &_texture_id);
	glBindTexture (GL_TEXTURE_RECTANGLE_ARB, _texture_id);
	glTexImage2D (GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8,
			width, height, 0,
			GL_BGRA, GL_UNSIGNED_BYTE, NULL);
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	[NSOpenGLContext clearCurrentContext];

	_width  = width;
	_height = height;
}

- (void) drawRect:(NSRect)rect
{
	[[self openGLContext] makeCurrentContext];

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClear(GL_COLOR_BUFFER_BIT);

	/* call back into CairoCanvas */
	cairo_rectangle_t cairo_rect;

	cairo_rect.x = rect.origin.x;
	cairo_rect.y = rect.origin.y;
	cairo_rect.width = rect.size.width;
	cairo_rect.height = rect.size.height;

	if (!surf || surf->get_width () != _width || surf->get_height() != _height) {
		surf = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, _width, _height);

		cairo_rect.x = 0;
		cairo_rect.y = 0;
		cairo_rect.width = _width;
		cairo_rect.height = _height;
	}

	Cairo::RefPtr<Cairo::Context> ctx = Cairo::Context::create (surf);

	// TODO: check retina screen, scaling factor.
	// cairo_surface_get_device_scale () or explicit scale

	ctx->rectangle (cairo_rect.x, cairo_rect.y, cairo_rect.width, cairo_rect.height);
	ctx->clip_preserve ();
	{
		/* draw background color */
		uint32_t col = cairo_canvas->background_color ();
		int r, g, b, a;
		UINT_TO_RGBA (col, &r, &g, &b, &a);
		ctx->set_source_rgba (r/255.0, g/255.0, b/255.0, a/255.0);
	}
	ctx->fill ();

	cairo_canvas->render (ctx, &cairo_rect);

	surf->flush ();
	uint8_t* imgdata = surf->get_data ();

	/* NOTE for big-endian (PPC), we'd need to flip byte-order
	 * RGBA <> RGBA for the texture.
	 * GtkCanvas does not use this nsview for PPC builds, yet
	 */

	/* continue OpenGL */
	glPushMatrix ();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, _texture_id);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8,
			_width, _height, /*border*/ 0,
			GL_BGRA, GL_UNSIGNED_BYTE, imgdata);

	glBegin(GL_QUADS);
	glTexCoord2f(           0.0f, (GLfloat) _height);
	glVertex2f(-1.0f, -1.0f);

	glTexCoord2f((GLfloat) _width, (GLfloat) _height);
	glVertex2f( 1.0f, -1.0f);

	glTexCoord2f((GLfloat) _width, 0.0f);
	glVertex2f( 1.0f,  1.0f);

	glTexCoord2f(            0.0f, 0.0f);
	glVertex2f(-1.0f,  1.0f);
	glEnd();

	glDisable(GL_TEXTURE_2D);
	glPopMatrix();

	///

	glFlush();
	glSwapAPPLE();
	[NSOpenGLContext clearCurrentContext];
}
@end


void*
Gtkmm2ext::nsglview_create (Gtkmm2ext::CairoCanvas* canvas)
{
	ArdourCanvasOpenGLView* gl_view = [ArdourCanvasOpenGLView new];
	if (!gl_view) {
		return 0;
	}
	[gl_view setCairoCanvas:canvas];
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
