/*
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

	unsigned long min_psiz, max_psiz;
	unsigned long min_bufz, max_bufz;

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

	err = snd_pcm_hw_params_get_period_size_min (hw_params, &min_psiz, 0);
	if (err < 0) {
		errmsg = "Cannot get minimum period size";
		goto error_out;
	}
	err = snd_pcm_hw_params_get_period_size_max (hw_params, &max_psiz, 0);
	if (err < 0) {
		errmsg = "Cannot get maximum period size";
		goto error_out;
	}

	err = snd_pcm_hw_params_get_buffer_size_min (hw_params, &min_bufz);
	if (err < 0) {
		errmsg = "Cannot get minimum buffer size";
		goto error_out;
	}
	err = snd_pcm_hw_params_get_buffer_size_max (hw_params, &max_bufz);
	if (err < 0) {
		errmsg = "Cannot get maximum buffer size";
		goto error_out;
	}

	err = snd_pcm_hw_params_get_periods_min (hw_params, &nfo->min_nper, 0);
	if (err < 0) {
		errmsg = "Cannot get minimum period count";
		goto error_out;
	}
	err = snd_pcm_hw_params_get_periods_max (hw_params, &nfo->max_nper, 0);
	if (err < 0) {
		errmsg = "Cannot get maximum period count";
		goto error_out;
	}

	snd_pcm_close (pcm);

	nfo->min_size = std::max (min_psiz, min_bufz / nfo->max_nper);
	nfo->max_size = std::min (max_psiz, max_bufz / nfo->min_nper);

	/* see also libs/backends/alsa/zita-alsa-pcmi.cc
	 * If any debug parameter is set, print device info.
	 */
	if (getenv ("ARDOUR_ALSA_DEBUG")) {
		fprintf (stdout, "ALSA: *%s* device-info\n", play ? "playback" : "capture");
		fprintf (stdout, "  dev_name : %s\n", device_name);
		fprintf (stdout, "  channels : %u\n", nfo->max_channels);
		fprintf (stdout, "  min_rate : %u\n", nfo->min_rate);
		fprintf (stdout, "  max_rate : %u\n", nfo->max_rate);
		fprintf (stdout, "  min_psiz : %lu\n", min_psiz);
		fprintf (stdout, "  max_psiz : %lu\n", max_psiz);
		fprintf (stdout, "  min_bufz : %lu\n", min_bufz);
		fprintf (stdout, "  max_bufz : %lu\n", max_bufz);
		fprintf (stdout, "  min_nper : %d\n", nfo->min_nper);
		fprintf (stdout, "  max_nper : %d\n", nfo->max_nper);
		fprintf (stdout, "  possible : %lu .. %lu\n", nfo->min_size, nfo->max_size);
	}

	nfo->valid = true;
	return 0;

error_out:
	fprintf (stderr, "ALSA: %s: %s\n", errmsg.c_str(), snd_strerror (err));
	snd_pcm_close (pcm);
	return 1;

}
