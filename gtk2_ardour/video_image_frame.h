/*
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2017 Robin Gareus <robin@gareus.org>
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
#ifndef __ardour_video_image_frame_h__
#define __ardour_video_image_frame_h__

#include <string>
#include <glib.h>

#include <sigc++/signal.h>
#include <pthread.h>

#include "ardour/ardour.h"
#include "pbd/signals.h"

#include "canvas/container.h"
#include "canvas/pixbuf.h"
#include "canvas/image.h"

namespace ARDOUR {
	class TempoSection;
	class MeterSection;
}

class PublicEditor;

/** @class VideoImageFrame
 *  @brief a single video-frame to be displayed in the video timeline
 */
class VideoImageFrame : public sigc::trackable
{
	public:
	VideoImageFrame (PublicEditor&, ArdourCanvas::Container&, int, int, std::string, std::string);
	virtual ~VideoImageFrame ();

	void set_position (samplepos_t);
	void set_videoframe (samplepos_t, int rightend = -1);
	samplepos_t get_video_frame_number() {return video_frame_number;}

	int get_height () {return clip_height;}
	int get_width ()  {return clip_width;}
	int get_rightend() { return rightend;}
	samplepos_t get_req_frame () {return req_video_frame_number;}
	std::string get_video_server_url () {return video_server_url;}
	std::string get_video_filename ()   {return video_filename;}

	void http_download_done (char *);
	PBD::Signal0<void> ImgChanged;

	protected:

	PublicEditor& editor;
	ArdourCanvas::Container *_parent;
	ArdourCanvas::Image *image;
	boost::shared_ptr<ArdourCanvas::Image::Data> img;

	int clip_width;
	int clip_height;
	int rightend;

	std::string video_server_url;
	std::string video_filename;

	double        unit_position;
	samplepos_t   sample_position;
	samplepos_t   video_frame_number;

	void reposition ();
	void exposeimg ();

	void fill_frame (const uint8_t r, const uint8_t g, const uint8_t b);
	void draw_line ();
	void draw_x ();
	void cut_rightend ();


	void http_get (samplepos_t fn);
	void http_get_again (samplepos_t fn);

	samplepos_t req_video_frame_number;
	samplepos_t want_video_frame_number;
	bool        queued_request;

	pthread_mutex_t request_lock;
	pthread_mutex_t queue_lock;

	pthread_t      thread_id_tt;
	bool           thread_active;

};

#endif /* __ardour_video_image_frame_h__ */
