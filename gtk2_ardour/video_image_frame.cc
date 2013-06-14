/*
    Copyright (C) 2010 Paul Davis
    Author: Robin Gareus <robin@gareus.org>

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
#include <sigc++/bind.h>
#include "ardour/tempo.h"

#include "ardour_ui.h"
#include "video_image_frame.h"
#include "public_editor.h"
#include "utils.h"
#include "canvas_impl.h"
#include "simpleline.h"
#include "rgb_macros.h"
#include "utils_videotl.h"

#include <gtkmm2ext/utils.h>
#include <pthread.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

VideoImageFrame::VideoImageFrame (PublicEditor& ed, ArdourCanvas::Group& parent, int w, int h, std::string vsurl, std::string vfn)
	: editor (ed)
	, _parent(&parent)
	, clip_width(w)
	, clip_height(h)
	, video_server_url(vsurl)
	, video_filename(vfn)
{
	pthread_mutex_init(&request_lock, NULL);
	pthread_mutex_init(&queue_lock, NULL);
	queued_request=false;
	video_frame_number = -1;
	rightend = -1;
	frame_position = 0;
	thread_active=false;

#if 0 /* DEBUG */
	printf("New VideoImageFrame (%ix%i) %s - %s\n", w, h, vsurl.c_str(), vfn.c_str());
#endif

	unit_position = editor.frame_to_unit (frame_position);
	group = new Group (parent, unit_position, 1.0);
	img_pixbuf = new ArdourCanvas::Pixbuf(*group);

	Glib::RefPtr<Gdk::Pixbuf> img;

	img = Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, true, 8, clip_width, clip_height);
	img->fill(RGBA_TO_UINT(0,0,0,255));
	img_pixbuf->property_pixbuf() = img;

	draw_line();
	video_draw_cross(img_pixbuf->property_pixbuf());

	group->signal_event().connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_videotl_bar_event), _parent));
	//img_pixbuf->signal_event().connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_videotl_bar_event), _parent));
}

VideoImageFrame::~VideoImageFrame ()
{
	if (thread_active) pthread_join(thread_id_tt, NULL);
	delete img_pixbuf;
	delete group;
	pthread_mutex_destroy(&request_lock);
	pthread_mutex_destroy(&queue_lock);
}

void
VideoImageFrame::set_position (framepos_t frame)
{
	double new_unit_position = editor.frame_to_unit (frame);
	group->move (new_unit_position - unit_position, 0.0);
	frame_position = frame;
	unit_position = new_unit_position;
}

void
VideoImageFrame::reposition ()
{
	set_position (frame_position);
}

void
VideoImageFrame::exposeimg ()
{
	img_pixbuf->show();
	/* Note: we can not use this thread to update the window
	 * it needs to be done from the Editor's thread idle_update */
	ImgChanged(); /* EMIT SIGNAL */
}

void
VideoImageFrame::set_videoframe (framepos_t videoframenumber, int re)
{
	if (video_frame_number == videoframenumber && rightend == re) return;

	video_frame_number = videoframenumber;
	rightend = re;
#if 0 /* dummy mode: print framenumber */
	gchar buf[16];
	snprintf (buf, sizeof(buf), "%li", (long int) videoframenumber);
	img_pixbuf->property_pixbuf() = pixbuf_from_ustring(g_strdup (buf), get_font_for_style (N_("MarkerText")), 80, 60, Gdk::Color ("#C0C0C0"));
	return;
#endif
#if 1 /* draw "empty frame" while we request the data */
	Glib::RefPtr<Gdk::Pixbuf> img;
	img = img_pixbuf->property_pixbuf();
	img->fill(RGBA_TO_UINT(0,0,0,255));
	video_draw_cross(img_pixbuf->property_pixbuf());
	draw_line();
	cut_rightend();
	exposeimg();
#endif
	/* request video-frame from decoder in background thread */
	http_get(video_frame_number);
}

void
VideoImageFrame::draw_line ()
{
	Glib::RefPtr<Gdk::Pixbuf> img;
	img = img_pixbuf->property_pixbuf();

	int rowstride = img->get_rowstride();
	int clip_height = img->get_height();
	int n_channels = img->get_n_channels();
	guchar *pixels, *p;
	pixels = img->get_pixels();

	int y;
	for (y=0;y<clip_height;y++) {
		p = pixels + y * rowstride;
		p[0] = 255; p[1] = 255; p[2] = 255;
		if (n_channels>3) p[3] = 255;
	}
}

void
VideoImageFrame::cut_rightend ()
{
	if (rightend < 0 ) { return; }
	Glib::RefPtr<Gdk::Pixbuf> img;
	img = img_pixbuf->property_pixbuf();

	int rowstride = img->get_rowstride();
	int clip_height = img->get_height();
	int clip_width = img->get_width();
	int n_channels = img->get_n_channels();
	guchar *pixels, *p;
	pixels = img->get_pixels();
	if (rightend > clip_width) { return; }

	int x,y;
	for (y=0;y<clip_height;++y) {
		p = pixels + y * rowstride + rightend * n_channels;
		p[0] = 192; p[1] = 127; p[2] = 127;
		if (n_channels>3) p[3] = 255;
		for (x=rightend+1; x<clip_width; ++x) {
			p = pixels + y * rowstride + x * n_channels;
			p[0] = 0; p[1] = 0; p[2] = 0;
			if (n_channels>3) p[3] = 0;
		}
	}
}

void *
http_get_thread (void *arg) {
	VideoImageFrame *vif = static_cast<VideoImageFrame *>(arg);
	char url[2048];
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	snprintf(url, sizeof(url), "%s?frame=%li&w=%d&h=%di&file=%s&format=rgb",
	  vif->get_video_server_url().c_str(),
	  (long int) vif->get_req_frame(), vif->get_width(), vif->get_height(),
	  vif->get_video_filename().c_str()
	);
	int status = 0;
	int timeout = 1000; // * 5ms -> 5sec
	char *res = NULL;
	do {
		res=curl_http_get(url, &status);
		if (status == 503) usleep(5000); // try-again
	} while (status == 503 && --timeout > 0);

	if (status != 200 || !res) {
		printf("no-video frame: video-server returned http-status: %d\n", status);
	}

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	vif->http_download_done(res);
	pthread_exit(0);
	return 0;
}

void
VideoImageFrame::http_download_done (char *data){
	if (queued_request) {
		http_get_again(want_video_frame_number);
		return;
	}

	if (!data) {
		/* Image request failed (HTTP error or timeout) */
		Glib::RefPtr<Gdk::Pixbuf> img;
		img = img_pixbuf->property_pixbuf();
		img->fill(RGBA_TO_UINT(128,0,0,255));
		video_draw_cross(img_pixbuf->property_pixbuf());
		cut_rightend();
		draw_line();
		cut_rightend();
		/* TODO: mark as invalid:
		 * video_frame_number = -1;
		 * TODO: but prevent live-loops when calling update again
		 */
	} else {
		Glib::RefPtr<Gdk::Pixbuf> tmp, img;
#if 0 // RGBA
		tmp = Gdk::Pixbuf::create_from_data ((guint8*) data, Gdk::COLORSPACE_RGB, true, 8, clip_width, clip_height, clip_width*4);
#else // RGB
		tmp = Gdk::Pixbuf::create_from_data ((guint8*) data, Gdk::COLORSPACE_RGB, false, 8, clip_width, clip_height, clip_width*3);
#endif
		img = img_pixbuf->property_pixbuf();
		tmp->copy_area (0, 0, clip_width, clip_height, img, 0, 0);
		free(data);
		draw_line();
		cut_rightend();
	}

	exposeimg();
	/* don't request frames too quickly, wait after user has zoomed */
	usleep(40000);

	if (queued_request) {
		http_get_again(want_video_frame_number);
	}
	pthread_mutex_unlock(&request_lock);
}


void
VideoImageFrame::http_get(framepos_t fn) {
	if (pthread_mutex_trylock(&request_lock)) {
		/* remember last request and schedule after the lock has been released. */
		pthread_mutex_lock(&queue_lock);
		queued_request=true;
		want_video_frame_number=fn;
		pthread_mutex_unlock(&queue_lock);
#if 0
		/* TODO: cancel request and start a new one
		 *  but only if we're waiting for curl request.
		 *  don't interrupt http_download_done()
		 *
		 *  This should work, but requires testing:
		 */
		if (!pthread_cancel(thread_id_tt)) {
			pthread_mutex_unlock(&request_lock);
		} else return;
#else
		return;
#endif
	}
	if (thread_active) pthread_join(thread_id_tt, NULL);
	pthread_mutex_lock(&queue_lock);
	queued_request=false;
	req_video_frame_number=fn;
	pthread_mutex_unlock(&queue_lock);
	int rv = pthread_create(&thread_id_tt, NULL, http_get_thread, this);
	thread_active=true;
	if (rv) {
		thread_active=false;
		printf("thread creation failed. %i\n",rv);
		http_download_done(NULL);
	}
}

void
VideoImageFrame::http_get_again(framepos_t /*fn*/) {
	pthread_mutex_lock(&queue_lock);
	queued_request=false;
	req_video_frame_number=want_video_frame_number;
	pthread_mutex_unlock(&queue_lock);

	http_get_thread(this);
}

