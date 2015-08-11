/*
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 Paul Davis
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <alsa/asoundlib.h>
#include "pbd/convert.h"
#include "ardouralsautil/deviceinfo.h"

using namespace std;

int
ARDOUR::get_alsa_device_parameters (const char* device_name, const bool play, ALSADeviceInfo *nfo)
{
	snd_pcm_t *pcm;
	snd_pcm_hw_params_t *hw_params;
	std::string errmsg;
	int err;

	nfo->valid = false;

	err = snd_pcm_open (&pcm, device_name,
			play ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE,
			SND_PCM_NONBLOCK);

	if (err < 0) {
		fprintf (stderr, "ALSA: Cannot open device '%s': %s\n", device_name, snd_strerror (err));
		return 1;
	}

	snd_pcm_hw_params_alloca (&hw_params);
	err = snd_pcm_hw_params_any (pcm, hw_params);
	if (err < 0) {
		errmsg = "Cannot get hardware parameters";
		goto error_out;
	}

	err = snd_pcm_hw_params_get_channels_max (hw_params, &nfo->max_channels);
	if (err < 0) {
		errmsg = "Cannot get maximum channels count";
		goto error_out;
	}

	err = snd_pcm_hw_params_get_rate_min (hw_params, &nfo->min_rate, NULL);
	if (err < 0) {
		errmsg = "Cannot get minimum rate";
		goto error_out;
	}
	err = snd_pcm_hw_params_get_rate_max (hw_params, &nfo->max_rate, NULL);
	if (err < 0) {
		errmsg = "Cannot get maximum rate";
		goto error_out;
	}

	err = snd_pcm_hw_params_get_buffer_size_min (hw_params, &nfo->min_size);
	if (err < 0) {
		errmsg = "Cannot get minimum buffer size";
		goto error_out;
	}
	err = snd_pcm_hw_params_get_buffer_size_max (hw_params, &nfo->max_size);
	if (err < 0) {
		errmsg = "Cannot get maximum buffer size";
		goto error_out;
	}
	snd_pcm_close (pcm);
	nfo->valid = true;
	return 0;

error_out:
	fprintf (stderr, "ALSA: %s: %s\n", errmsg.c_str(), snd_strerror (err));
	snd_pcm_close (pcm);
	return 1;

}
