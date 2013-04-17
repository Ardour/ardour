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

#include "ardour_ui.h"
#include "video_image_frame.h"
#include "public_editor.h"
#include "utils.h"
#include "canvas/group.h"
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

	unit_position = editor.sample_to_pixel (frame_position);
	image = new ArdourCanvas::Image (_parent, Cairo::FORMAT_ARGB32, clip_width, clip_height);

	img = image->get_image();
	fill_frame(0, 0, 0);
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
VideoImageFrame::set_position (framepos_t frame)
{
	double new_unit_position = editor.sample_to_pixel (frame);
	image->move (ArdourCanvas::Duple (new_unit_position - unit_position, 0.0));
	frame_position = frame;
	unit_position = new_unit_position;
}

void
VideoImageFrame::reposition ()
{
	set_position (frame_position);
}

void
VideoImageFrame::exposeimg () {
	ImgChanged(); /* EMIT SIGNAL */
}

void
VideoImageFrame::set_videoframe (framepos_t videoframenumber, int re)
{
	if (video_frame_number == videoframenumber && rightend == re) return;

	video_frame_number = videoframenumber;
	rightend = re;

	img = image->get_image();
	fill_frame(0, 0, 0);
	draw_x();
	draw_line();
	cut_rightend();
	image->put_image(img);
	exposeimg();

	/* request video-frame from decoder in background thread */
	http_get(video_frame_number);
}

void
VideoImageFrame::draw_line ()
{
	const int rowstride = img->stride;
	const int clip_height = img->height;
	uint8_t *pixels, *p;
	pixels = img->data.get();

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
	pixels = img->data.get();

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
	pixels = img->data.get();

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
	pixels = img->data.get();
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

void *
http_get_thread (void *arg) {
	VideoImageFrame *vif = static_cast<VideoImageFrame *>(arg);
	char url[2048];
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	snprintf(url, sizeof(url), "%s?frame=%li&w=%d&h=%di&file=%s&format=bgra",
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
		http_maybe_get_again();
		return;
	}

	if (!data) {
		/* Image request failed (HTTP error or timeout) */
		img = image->get_image();
		fill_frame(128, 0, 0);
		draw_x();
		cut_rightend();
		draw_line();
		cut_rightend();
		image->put_image(img);
	} else {
		img = image->get_image();
		/* TODO - have curl write directly to the shared memory region */
		memcpy((void*) img->data.get(), data, img->stride * img->height);
		free(data);
		draw_line();
		cut_rightend();
		image->put_image(img);
	}

	exposeimg();
	/* don't request frames rapidly, wait after user has zoomed */
	usleep(20000);

	if (queued_request) {
		http_maybe_get_again();
	}
	pthread_mutex_unlock(&request_lock);
}


void
VideoImageFrame::http_get(framepos_t fn) {
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
VideoImageFrame::http_maybe_get_again() {
	pthread_mutex_lock(&queue_lock);
	queued_request=false;
	req_video_frame_number=want_video_frame_number;
	pthread_mutex_unlock(&queue_lock);

	http_get_thread(this);
}


extern "C" {
#include <curl/curl.h>

	struct MemoryStruct {
		char *data;
		size_t size;
	};

	static size_t
	WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data) {
		size_t realsize = size * nmemb;
		struct MemoryStruct *mem = (struct MemoryStruct *)data;

		mem->data = (char *)realloc(mem->data, mem->size + realsize + 1);
		if (mem->data) {
			memcpy(&(mem->data[mem->size]), ptr, realsize);
			mem->size += realsize;
			mem->data[mem->size] = 0;
		}
		return realsize;
	}

	char *curl_http_get (const char *u, int *status) {
		CURL *curl;
		CURLcode res;
		struct MemoryStruct chunk;
		long int httpstatus;
		if (status) *status = 0;
		if (strncmp("http://", u, 7)) return NULL;

		chunk.data=NULL;
		chunk.size=0;

		curl = curl_easy_init();
		if(!curl) return NULL;
		curl_easy_setopt(curl, CURLOPT_URL, u);

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, ARDOUR_USER_AGENT);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, ARDOUR_CURL_TIMEOUT);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
#ifdef CURLERRORDEBUG
		char curlerror[CURL_ERROR_SIZE] = "";
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlerror);
#endif

		res = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpstatus);
		curl_easy_cleanup(curl);
		if (status) *status = httpstatus;
		if (res) {
#ifdef CURLERRORDEBUG
			printf("curl_http_get() failed: %s\n", curlerror);
#endif
			return NULL;
		}
		if (httpstatus != 200) {
			free (chunk.data);
			chunk.data = NULL;
		}
		return (chunk.data);
	}

} /* end extern "C" */
