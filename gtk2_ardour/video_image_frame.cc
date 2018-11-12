/*
    Copyright (C) 2010, 2013 Paul Davis
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

#include <gtkmm2ext/utils.h>
#include <pthread.h>

#include "canvas/container.h"

#include "ardour_http.h"
#include "public_editor.h"
#include "utils_videotl.h"
#include "video_image_frame.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace VideoUtils;

static void freedata_cb (uint8_t *d, void* /*arg*/) {
	/* later this can be used with libharvid
	 * the buffer/videocacheline instead of freeing it
	 */
	free (d);
}

VideoImageFrame::VideoImageFrame (PublicEditor& ed, ArdourCanvas::Container& parent, int w, int h, std::string vsurl, std::string vfn)
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
	sample_position = 0;
	thread_active=false;

	unit_position = editor.sample_to_pixel (sample_position);
	image = new ArdourCanvas::Image (_parent, Cairo::FORMAT_ARGB32, clip_width, clip_height);

	img = image->get_image();
	fill_frame (0, 0, 0);
	draw_line();
	draw_x();
	image->put_image(img);

	image->Event.connect (sigc::bind (sigc::mem_fun (editor, &PublicEditor::canvas_videotl_bar_event), _parent));
}

VideoImageFrame::~VideoImageFrame ()
{
	if (thread_active) pthread_join(thread_id_tt, NULL);
	delete image;
	pthread_mutex_destroy(&request_lock);
	pthread_mutex_destroy(&queue_lock);
}

void
VideoImageFrame::set_position (samplepos_t sample)
{
	double new_unit_position = editor.sample_to_pixel (sample);
	image->move (ArdourCanvas::Duple (new_unit_position - unit_position, 0.0));
	sample_position = sample;
	unit_position = new_unit_position;
}

void
VideoImageFrame::reposition ()
{
	set_position (sample_position);
}

void
VideoImageFrame::exposeimg () {
	ImgChanged(); /* EMIT SIGNAL */
}

void
VideoImageFrame::set_videoframe (samplepos_t videoframenumber, int re)
{
	if (video_frame_number == videoframenumber && rightend == re) return;

	video_frame_number = videoframenumber;
	rightend = re;

	img = image->get_image();
	fill_frame (0, 0, 0);
	draw_x();
	draw_line();
	cut_rightend();
	image->put_image(img);
	exposeimg();

	/* request video-frame from decoder in background thread */
	http_get (video_frame_number);
}

void
VideoImageFrame::draw_line ()
{
	const int rowstride = img->stride;
	const int clip_height = img->height;
	uint8_t *pixels, *p;
	pixels = img->data;

	int y;
	for (y = 0;y < clip_height; y++) {
		p = pixels + y * rowstride;
		p[0] = 255; p[1] = 255; p[2] = 255; p[3] = 255;
	}
}

void
VideoImageFrame::fill_frame (const uint8_t r, const uint8_t g, const uint8_t b)
{
	const int rowstride = img->stride;
	const int clip_height = img->height;
	const int clip_width = img->width;
	uint8_t *pixels, *p;
	pixels = img->data;

	int x,y;
	for (y = 0; y < clip_height; ++y) {
		for (x = 0; x < clip_width; ++x) {
			p = pixels + y * rowstride + x * 4;
			p[0] = b; p[1] = g; p[2] = r; p[3] = 255;
		}
	}
}

void
VideoImageFrame::draw_x ()
{
	int x,y;
	const int rowstride = img->stride;
	const int clip_width = img->width;
	const int clip_height = img->height;
	uint8_t *pixels, *p;
	pixels = img->data;

	for (x = 0;x < clip_width; x++) {
		y = clip_height * x / clip_width;
		p = pixels + y * rowstride + x * 4;
		p[0] = 192; p[1] = 192; p[2] = 192; p[3] = 255;
		p = pixels + y * rowstride + (clip_width-x-1) * 4;
		p[0] = 192; p[1] = 192; p[2] = 192; p[3] = 255;
	}
}

void
VideoImageFrame::cut_rightend ()
{

	if (rightend < 0 ) { return; }

	const int rowstride = img->stride;
	const int clip_height = img->height;
	const int clip_width = img->width;
	uint8_t *pixels, *p;
	pixels = img->data;
	if (rightend > clip_width) { return; }

	int x,y;
	for (y = 0;y < clip_height; ++y) {
		p = pixels + y * rowstride + rightend * 4;
		p[0] = 192; p[1] = 192; p[2] = 192; p[3] = 255;
		for (x=rightend+1; x < clip_width; ++x) {
			p = pixels + y * rowstride + x * 4;
			p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 0;
		}
	}
}

static void *
http_get_thread (void *arg) {
	VideoImageFrame *vif = static_cast<VideoImageFrame *>(arg);
	char url[2048];
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	snprintf(url, sizeof(url), "%s?frame=%li&w=%d&h=%d&file=%s&format=bgra",
	  vif->get_video_server_url().c_str(),
	  (long int) vif->get_req_frame(), vif->get_width(), vif->get_height(),
	  vif->get_video_filename().c_str()
	);
	int status = 0;
	int timeout = 1000; // * 5ms -> 5sec
	char *res = NULL;
	do {
		res = ArdourCurl::http_get (url, &status, false);
		if (status == 503) Glib::usleep(5000); // try-again
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
		img = image->get_image();
		fill_frame (128, 0, 0);
		draw_x();
		cut_rightend();
		draw_line();
		cut_rightend();
		image->put_image(img);
	} else {
		img = image->get_image(false);
		img->data = (uint8_t*) data;
		img->destroy_callback = &freedata_cb;
		draw_line();
		cut_rightend();
		image->put_image(img);
	}

	exposeimg();
	/* don't request frames too quickly, wait after user has zoomed */
	Glib::usleep(40000);

	if (queued_request) {
		http_get_again(want_video_frame_number);
	}
	pthread_mutex_unlock(&request_lock);
}


void
VideoImageFrame::http_get (samplepos_t fn) {
	if (pthread_mutex_trylock(&request_lock)) {
		pthread_mutex_lock(&queue_lock);
		queued_request=true;
		want_video_frame_number=fn;
		pthread_mutex_unlock(&queue_lock);
		return;
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
VideoImageFrame::http_get_again(samplepos_t /*fn*/) {
	pthread_mutex_lock(&queue_lock);
	queued_request=false;
	req_video_frame_number=want_video_frame_number;
	pthread_mutex_unlock(&queue_lock);

	http_get_thread(this);
}

