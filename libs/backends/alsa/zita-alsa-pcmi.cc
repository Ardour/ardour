/*
 *  Copyright (C) 2006-2012 Fons Adriaensen <fons@linuxaudio.org>
 *  Copyright (C) 2014-2021 Robin Gareus <robin@gareus.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#if defined(__NetBSD__)
#include <sys/endian.h>
#else
#include <endian.h>
#endif
#include "zita-alsa-pcmi.h"
#include <sys/time.h>

/* Public members *************************************************************/

Alsa_pcmi::Alsa_pcmi (
		const char*  play_name,
		const char*  capt_name,
		const char*  ctrl_name,
		unsigned int fsamp,
		unsigned int fsize,
		unsigned int play_nfrag,
		unsigned int capt_nfrag,
		unsigned int debug)
	: _fsamp (fsamp)
	, _fsize (fsize)
	, _play_nfrag (play_nfrag)
	, _real_nfrag (play_nfrag)
	, _capt_nfrag (capt_nfrag)
	, _debug (debug)
	, _state (-1)
	, _play_handle (0)
	, _capt_handle (0)
	, _ctrl_handle (0)
	, _play_hwpar (0)
	, _play_swpar (0)
	, _capt_hwpar (0)
	, _capt_swpar (0)
	, _play_nchan (0)
	, _capt_nchan (0)
	, _play_xrun (0)
	, _capt_xrun (0)
	, _synced (false)
	, _play_npfd (0)
	, _capt_npfd (0)
{
	const char* p;

	p = getenv ("ARDOUR_ALSA_DEBUG");
	if (p && *p) {
		_debug = atoi (p);
	}
	initialise (play_name, capt_name, ctrl_name);
}

Alsa_pcmi::~Alsa_pcmi (void)
{
	if (_play_handle) {
		snd_pcm_close (_play_handle);
	}
	if (_capt_handle) {
		snd_pcm_close (_capt_handle);
	}
	if (_ctrl_handle) {
		snd_ctl_close (_ctrl_handle);
	}

	snd_pcm_sw_params_free (_capt_swpar);
	snd_pcm_hw_params_free (_capt_hwpar);
	snd_pcm_sw_params_free (_play_swpar);
	snd_pcm_hw_params_free (_play_hwpar);
}

int
Alsa_pcmi::pcm_start (void)
{
	int err;

	if (_play_handle) {
		unsigned int n = snd_pcm_avail_update (_play_handle);
		if (n < _fsize * _play_nfrag) {
			if (_debug & DEBUG_STAT)
				fprintf (stderr, "Alsa_pcmi: full buffer not available at start.\n");
			return -1;
		}
		for (unsigned int i = 0; i < _play_nfrag; i++) {
			play_init (_fsize);
			for (unsigned int j = 0; j < _play_nchan; j++) {
				clear_chan (j, _fsize);
			}
			play_done (_fsize);
		}
		if ((err = snd_pcm_start (_play_handle)) < 0) {
			if (_debug & DEBUG_STAT)
				fprintf (stderr, "Alsa_pcmi: pcm_start(play): %s.\n", snd_strerror (err));
			return -1;
		}
	}
	if (_capt_handle && !_synced && ((err = snd_pcm_start (_capt_handle)) < 0)) {
		if (_debug & DEBUG_STAT)
			fprintf (stderr, "Alsa_pcmi: pcm_start(capt): %s.\n", snd_strerror (err));
		return -1;
	}

	return 0;
}

int
Alsa_pcmi::pcm_stop (void)
{
	int err;

	if (_play_handle && ((err = snd_pcm_drop (_play_handle)) < 0)) {
		if (_debug & DEBUG_STAT) {
			fprintf (stderr, "Alsa_pcmi: pcm_drop(play): %s.\n", snd_strerror (err));
		}
		return -1;
	}
	if (_capt_handle && !_synced && ((err = snd_pcm_drop (_capt_handle)) < 0)) {
		if (_debug & DEBUG_STAT) {
			fprintf (stderr, "Alsa_pcmi: pcm_drop(capt): %s.\n", snd_strerror (err));
		}
		return -1;
	}

	return 0;
}

snd_pcm_sframes_t
Alsa_pcmi::pcm_wait (void)
{
	bool              need_capt;
	bool              need_play;
	snd_pcm_sframes_t capt_av;
	snd_pcm_sframes_t play_av;
	unsigned short    rev;
	int               i, r, n1, n2;

	_state    = 0;
	need_capt = _capt_handle ? true : false;
	need_play = _play_handle ? true : false;

	while (need_play || need_capt) {
		n1 = 0;
		if (need_play) {
			snd_pcm_poll_descriptors (_play_handle, _poll_fd, _play_npfd);
			n1 += _play_npfd;
		}
		n2 = n1;
		if (need_capt) {
			snd_pcm_poll_descriptors (_capt_handle, _poll_fd + n1, _capt_npfd);
			n2 += _capt_npfd;
		}
		for (i = 0; i < n2; i++)
			_poll_fd[i].events |= POLLERR;

		timespec timeout;
		timeout.tv_sec  = 1;
		timeout.tv_nsec = 0;
#if defined(__NetBSD__)
		r = pollts (_poll_fd, n2, &timeout, NULL);
#else
		r = ppoll (_poll_fd, n2, &timeout, NULL);
#endif

		if (r < 0) {
			if (errno == EINTR) {
				return 0;
			}
			if (_debug & DEBUG_WAIT) {
				fprintf (stderr, "Alsa_pcmi: poll(): %s\n.", strerror (errno));
			}
			_state = -1;
			return 0;
		}
		if (r == 0) {
			if (_debug & DEBUG_WAIT) {
				fprintf (stderr, "Alsa_pcmi: poll timed out.\n");
			}
			_state = -1;
			return 0;
		}

		if (need_play) {
			snd_pcm_poll_descriptors_revents (_play_handle, _poll_fd, n1, &rev);
			if (rev & POLLERR) {
				if (_debug & DEBUG_WAIT) {
					fprintf (stderr, "Alsa_pcmi: error on playback pollfd.\n");
				}
				_state = 1;
				recover ();
				return 0;
			}
			if (rev & POLLOUT) {
				need_play = false;
			}
		}
		if (need_capt) {
			snd_pcm_poll_descriptors_revents (_capt_handle, _poll_fd + n1, n2 - n1, &rev);
			if (rev & POLLERR) {
				if (_debug & DEBUG_WAIT) {
					fprintf (stderr, "Alsa_pcmi: error on capture pollfd.\n");
				}
				_state = 1;
				recover ();
				return 0;
			}
			if (rev & POLLIN) {
				need_capt = false;
			}
		}
	}

	play_av = 999999999;
	if (_play_handle && (play_av = snd_pcm_avail_update (_play_handle)) < 0) {
		_state = -1;
		if (!recover ()) {
			_state = 1;
		}
		return 0;
	}
	capt_av = 999999999;
	if (_capt_handle && (capt_av = snd_pcm_avail_update (_capt_handle)) < 0) {
		_state = -1;
		if (!recover ()) {
			_state = 1;
		}
		return 0;
	}

	return (capt_av < play_av) ? capt_av : play_av;
}

int
Alsa_pcmi::pcm_idle (int len)
{
	if (_capt_handle) {
		snd_pcm_uframes_t n = len;
		while (n) {
			snd_pcm_uframes_t k = capt_init (n);
			capt_done (k);
			n -= k;
		}
	}
	if (_play_handle) {
		snd_pcm_uframes_t n = len;
		while (n) {
			snd_pcm_uframes_t k = play_init (n);
			for (unsigned int i = 0; i < _play_nchan; i++)
				clear_chan (i, k);
			play_done (k);
			n -= k;
		}
	}
	return 0;
}

int
Alsa_pcmi::play_init (snd_pcm_uframes_t len)
{
	int err;
	const snd_pcm_channel_area_t* a;

	if (!_play_handle) {
		return 0;
	}

	if ((err = snd_pcm_mmap_begin (_play_handle, &a, &_play_offs, &len)) < 0) {
		if (_debug & DEBUG_DATA)
			fprintf (stderr, "Alsa_pcmi: snd_pcm_mmap_begin(play): %s.\n", snd_strerror (err));
		return -1;
	}
	_play_step = (a->step) >> 3;
	for (unsigned int i = 0; i < _play_nchan; i++, a++) {
		_play_ptr[i] = (char*)a->addr + ((a->first + a->step * _play_offs) >> 3);
	}

	return len;
}

int
Alsa_pcmi::capt_init (snd_pcm_uframes_t len)
{
	int err;
	const snd_pcm_channel_area_t* a;

	if (!_capt_handle) {
		return 0;
	}

	if ((err = snd_pcm_mmap_begin (_capt_handle, &a, &_capt_offs, &len)) < 0) {
		if (_debug & DEBUG_DATA)
			fprintf (stderr, "Alsa_pcmi: snd_pcm_mmap_begin(capt): %s.\n", snd_strerror (err));
		return -1;
	}
	_capt_step = (a->step) >> 3;
	for (unsigned int i = 0; i < _capt_nchan; i++, a++) {
		_capt_ptr[i] = (char*)a->addr + ((a->first + a->step * _capt_offs) >> 3);
	}

	return len;
}

void
Alsa_pcmi::clear_chan (int chan, int len)
{
	_play_ptr[chan] = (this->*Alsa_pcmi::_clear_func) (_play_ptr[chan], len);
}

void
Alsa_pcmi::play_chan (int chan, const float* src, int len, int step)
{
	_play_ptr[chan] = (this->*Alsa_pcmi::_play_func) (src, _play_ptr[chan], len, step);
}

void
Alsa_pcmi::capt_chan (int chan, float* dst, int len, int step)
{
	_capt_ptr[chan] = (this->*Alsa_pcmi::_capt_func) (_capt_ptr[chan], dst, len, step);
}

int
Alsa_pcmi::play_done (int len)
{
	if (!_play_handle) {
		return 0;
	}
	return snd_pcm_mmap_commit (_play_handle, _play_offs, len);
}

int
Alsa_pcmi::capt_done (int len)
{
	if (!_capt_handle) {
		return 0;
	}
	return snd_pcm_mmap_commit (_capt_handle, _capt_offs, len);
}

static const char*
access_type_name (snd_pcm_access_t a)
{
	switch (a) {
		case SND_PCM_ACCESS_MMAP_INTERLEAVED:
			return "MMAP interleaved";
		case SND_PCM_ACCESS_MMAP_NONINTERLEAVED:
			return "MMAP non-interleaved";
		case SND_PCM_ACCESS_MMAP_COMPLEX:
			return "MMAP complex";
		case SND_PCM_ACCESS_RW_INTERLEAVED:
			assert (0);
			return "RW interleaved";
		case SND_PCM_ACCESS_RW_NONINTERLEAVED:
			assert (0);
			return "RW non-interleaved";
		default:
			assert (0);
			return "unknown";
	}
}

void
Alsa_pcmi::printinfo (void)
{
	fprintf (stdout, "playback");
	if (_play_handle) {
		fprintf (stdout, "\n  nchan  : %d\n", _play_nchan);
		fprintf (stdout, "  fsamp  : %d\n", _fsamp);
		fprintf (stdout, "  fsize  : %ld\n", _fsize);
		fprintf (stdout, "  nfrag  : %d\n", _real_nfrag);
		fprintf (stdout, "  format : %s\n", snd_pcm_format_name (_play_format));
		fprintf (stdout, "  access : %s\n", access_type_name (_play_access));
	} else {
		fprintf (stdout, " : not enabled\n");
	}

	fprintf (stdout, "capture");
	if (_capt_handle) {
		fprintf (stdout, "\n  nchan  : %d\n", _capt_nchan);
		fprintf (stdout, "  fsamp  : %d\n", _fsamp);
		fprintf (stdout, "  fsize  : %ld\n", _fsize);
		fprintf (stdout, "  nfrag  : %d\n", _capt_nfrag);
		fprintf (stdout, "  format : %s\n", snd_pcm_format_name (_capt_format));
		fprintf (stdout, "  access : %s\n", access_type_name (_capt_access));
		if (_play_handle) {
			fprintf (stdout, "%s\n", _synced ? "synced" : "not synced");
		}
	} else {
		fprintf (stdout, "  : not enabled\n");
	}
}

/* Private members ************************************************************/

void
Alsa_pcmi::initialise (const char* play_name, const char* capt_name, const char* ctrl_name)
{
	unsigned int         fsamp;
	snd_pcm_uframes_t    fsize;
	unsigned int         nfrag;
	int                  err;
	int                  dir;
	snd_ctl_card_info_t* card;

	if (play_name) {
		if (snd_pcm_open (&_play_handle, play_name, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
			_play_handle = 0;
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alsa_pcmi: Cannot open PCM device %s for playback.\n", play_name);
			}
		}
	}

	if (capt_name) {
		if (snd_pcm_open (&_capt_handle, capt_name, SND_PCM_STREAM_CAPTURE, 0) < 0) {
			_capt_handle = 0;
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alsa_pcmi: Cannot open PCM device %s for capture.\n", capt_name);
			}
		}
	}

	if (!_play_handle && !_capt_handle) {
		return;
	}

	if (ctrl_name) {
		snd_ctl_card_info_alloca (&card);

		if ((err = snd_ctl_open (&_ctrl_handle, ctrl_name, 0)) < 0) {
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alse_driver: ctl_open(): %s\n", snd_strerror (err));
			}
			return;
		}
		if ((err = snd_ctl_card_info (_ctrl_handle, card)) < 0) {
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alsa_pcmi: ctl_card_info(): %s\n", snd_strerror (err));
			}
			return;
		}
	}

	/* devices opened, now perform hardware config */
	_state = -2;

	if (_capt_handle) {
		if (snd_pcm_hw_params_malloc (&_capt_hwpar) < 0) {
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alsa_pcmi: can't allocate capture hw params\n");
			}
			return;
		}
		if (snd_pcm_sw_params_malloc (&_capt_swpar) < 0) {
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alsa_pcmi: can't allocate capture sw params\n");
			}
			return;
		}
		if (set_hwpar (_capt_handle, _capt_hwpar, "capture", _capt_nfrag, &_capt_nchan) < 0) {
			return;
		}
		if (set_swpar (_capt_handle, _capt_swpar, "capture") < 0) {
			return;
		}
	}

	if (_play_handle) {
		if (snd_pcm_hw_params_malloc (&_play_hwpar) < 0) {
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alsa_pcmi: can't allocate playback hw params\n");
			}
			return;
		}
		if (snd_pcm_sw_params_malloc (&_play_swpar) < 0) {
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alsa_pcmi: can't allocate playback sw params\n");
			}
			return;
		}
		if (set_hwpar (_play_handle, _play_hwpar, "playback", _play_nfrag, &_play_nchan) < 0) {
			return;
		}
		if (set_swpar (_play_handle, _play_swpar, "playback") < 0) {
			return;
		}
	}

	/* devices are configured, now confirm settings and setup format conversion */

	if (_play_handle) {
		if (snd_pcm_hw_params_get_rate (_play_hwpar, &fsamp, &dir) || (fsamp != _fsamp) || dir) {
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alsa_pcmi: can't get requested sample rate for playback.\n");
			}
			_state = -3;
			return;
		}
		if (snd_pcm_hw_params_get_period_size (_play_hwpar, &fsize, &dir) || (fsize != _fsize) || dir) {
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alsa_pcmi: can't get requested period size for playback.\n");
			}
			_state = -4;
			return;
		}
		if (snd_pcm_hw_params_get_periods (_play_hwpar, &_real_nfrag, &dir) || (_real_nfrag != _play_nfrag) || dir) {
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alsa_pcmi warning: requested %u periods for playback, using %u.\n", _play_nfrag, _real_nfrag);
			}
		}

		snd_pcm_hw_params_get_format (_play_hwpar, &_play_format);
		snd_pcm_hw_params_get_access (_play_hwpar, &_play_access);

#if __BYTE_ORDER == __LITTLE_ENDIAN
		switch (_play_format) {
			case SND_PCM_FORMAT_FLOAT_LE:
				_clear_func = &Alsa_pcmi::clear_32;
				_play_func  = &Alsa_pcmi::play_float;
				break;

			case SND_PCM_FORMAT_S32_LE:
				_clear_func = &Alsa_pcmi::clear_32;
				_play_func  = &Alsa_pcmi::play_32;
				break;

			case SND_PCM_FORMAT_S32_BE:
				_clear_func = &Alsa_pcmi::clear_32;
				_play_func  = &Alsa_pcmi::play_32swap;
				break;

			case SND_PCM_FORMAT_S24_3LE:
				_clear_func = &Alsa_pcmi::clear_24;
				_play_func  = &Alsa_pcmi::play_24;
				break;

			case SND_PCM_FORMAT_S24_3BE:
				_clear_func = &Alsa_pcmi::clear_24;
				_play_func  = &Alsa_pcmi::play_24swap;
				break;

			case SND_PCM_FORMAT_S16_LE:
				_clear_func = &Alsa_pcmi::clear_16;
				_play_func  = &Alsa_pcmi::play_16;
				break;

			case SND_PCM_FORMAT_S16_BE:
				_clear_func = &Alsa_pcmi::clear_16;
				_play_func  = &Alsa_pcmi::play_16swap;
				break;

			default:
				if (_debug & DEBUG_INIT) {
					fprintf (stderr, "Alsa_pcmi: can't handle playback sample format.\n");
				}
				_state = -6;
				return;
		}
#elif __BYTE_ORDER == __BIG_ENDIAN
		switch (_play_format) {
			case SND_PCM_FORMAT_S32_LE:
				_clear_func = &Alsa_pcmi::clear_32;
				_play_func  = &Alsa_pcmi::play_32swap;
				break;

			case SND_PCM_FORMAT_S32_BE:
				_clear_func = &Alsa_pcmi::clear_32;
				_play_func  = &Alsa_pcmi::play_32;
				break;

			case SND_PCM_FORMAT_S24_3LE:
				_clear_func = &Alsa_pcmi::clear_24;
				_play_func  = &Alsa_pcmi::play_24swap;
				break;

			case SND_PCM_FORMAT_S24_3BE:
				_clear_func = &Alsa_pcmi::clear_24;
				_play_func  = &Alsa_pcmi::play_24;
				break;

			case SND_PCM_FORMAT_S16_LE:
				_clear_func = &Alsa_pcmi::clear_16;
				_play_func  = &Alsa_pcmi::play_16swap;
				break;

			case SND_PCM_FORMAT_S16_BE:
				_clear_func = &Alsa_pcmi::clear_16;
				_play_func  = &Alsa_pcmi::play_16;
				break;

			default:
				if (_debug & DEBUG_INIT) {
					fprintf (stderr, "Alsa_pcmi: can't handle playback sample format.\n");
				}
				_state = -6;
				return;
		}
#else
# error "System byte order is undefined or not supported"
#endif

		_play_npfd = snd_pcm_poll_descriptors_count (_play_handle);
	}

	if (_capt_handle) {
		if (snd_pcm_hw_params_get_rate (_capt_hwpar, &fsamp, &dir) || (fsamp != _fsamp) || dir) {
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alsa_pcmi: can't get requested sample rate for capture.\n");
			}
			_state = -3;
			return;
		}
		if (snd_pcm_hw_params_get_period_size (_capt_hwpar, &fsize, &dir) || (fsize != _fsize) || dir) {
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alsa_pcmi: can't get requested period size for capture.\n");
			}
			_state = -4;
			return;
		}
		if (snd_pcm_hw_params_get_periods (_capt_hwpar, &nfrag, &dir) || (nfrag != _capt_nfrag) || dir) {
			if (_debug & DEBUG_INIT) {
				fprintf (stderr, "Alsa_pcmi warning: requested %u periods for playback, using %u.\n", _capt_nfrag, nfrag);
			}
		}

		if (_play_handle) {
			_synced = !snd_pcm_link (_play_handle, _capt_handle);
		}

		snd_pcm_hw_params_get_format (_capt_hwpar, &_capt_format);
		snd_pcm_hw_params_get_access (_capt_hwpar, &_capt_access);

#if __BYTE_ORDER == __LITTLE_ENDIAN
		switch (_capt_format) {
			case SND_PCM_FORMAT_FLOAT_LE:
				_capt_func = &Alsa_pcmi::capt_float;
				break;

			case SND_PCM_FORMAT_S32_LE:
				_capt_func = &Alsa_pcmi::capt_32;
				break;

			case SND_PCM_FORMAT_S32_BE:
				_capt_func = &Alsa_pcmi::capt_32swap;
				break;

			case SND_PCM_FORMAT_S24_3LE:
				_capt_func = &Alsa_pcmi::capt_24;
				break;

			case SND_PCM_FORMAT_S24_3BE:
				_capt_func = &Alsa_pcmi::capt_24swap;
				break;

			case SND_PCM_FORMAT_S16_LE:
				_capt_func = &Alsa_pcmi::capt_16;
				break;

			case SND_PCM_FORMAT_S16_BE:
				_capt_func = &Alsa_pcmi::capt_16swap;
				break;

			default:
				if (_debug & DEBUG_INIT) {
					fprintf (stderr, "Alsa_pcmi: can't handle capture sample format.\n");
				}
				_state = -6;
				return;
		}
#elif __BYTE_ORDER == __BIG_ENDIAN
		switch (_capt_format) {
			case SND_PCM_FORMAT_S32_LE:
				_capt_func = &Alsa_pcmi::capt_32swap;
				break;

			case SND_PCM_FORMAT_S32_BE:
				_capt_func = &Alsa_pcmi::capt_32;
				break;

			case SND_PCM_FORMAT_S24_3LE:
				_capt_func = &Alsa_pcmi::capt_24swap;
				break;

			case SND_PCM_FORMAT_S24_3BE:
				_capt_func = &Alsa_pcmi::capt_24;
				break;

			case SND_PCM_FORMAT_S16_LE:
				_capt_func = &Alsa_pcmi::capt_16swap;
				break;

			case SND_PCM_FORMAT_S16_BE:
				_capt_func = &Alsa_pcmi::capt_16;
				break;

			default:
				if (_debug & DEBUG_INIT) {
					fprintf (stderr, "Alsa_pcmi: can't handle capture sample format.\n");
				}
				_state = -6;
				return;
		}
#else
# error "System byte order is undefined or not supported"
#endif

		_capt_npfd = snd_pcm_poll_descriptors_count (_capt_handle);
	}

	if (_play_npfd + _capt_npfd > MAXPFD) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: interface requires more than %d pollfd\n", MAXPFD);
		}
		return;
	}

	_state = 0;
}

int
Alsa_pcmi::set_hwpar (snd_pcm_t* handle, snd_pcm_hw_params_t* hwpar, const char* sname, unsigned int nfrag, unsigned int* nchan)
{
	bool err;

	if (snd_pcm_hw_params_any (handle, hwpar) < 0) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: no %s hw configurations available.\n", sname);
		}
		return -1;
	}
	if (snd_pcm_hw_params_set_periods_integer (handle, hwpar) < 0) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: can't set %s period size to integral value.\n", sname);
		}
		return -1;
	}

	bool il = _debug & TRY_INTLVD;

	if (   (snd_pcm_hw_params_set_access (handle, hwpar, il ? SND_PCM_ACCESS_MMAP_INTERLEAVED : SND_PCM_ACCESS_MMAP_NONINTERLEAVED) < 0)
	    && (snd_pcm_hw_params_set_access (handle, hwpar, il ? SND_PCM_ACCESS_MMAP_NONINTERLEAVED : SND_PCM_ACCESS_MMAP_INTERLEAVED) < 0)
	    && (snd_pcm_hw_params_set_access (handle, hwpar, SND_PCM_ACCESS_MMAP_COMPLEX) < 0))
	{
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: the %s interface doesn't support mmap-based access.\n", sname);
		}
		return -1;
	}

	if (_debug & FORCE_16B) {
		err =    (snd_pcm_hw_params_set_format (handle, hwpar, SND_PCM_FORMAT_S16_LE) < 0)
		      && (snd_pcm_hw_params_set_format (handle, hwpar, SND_PCM_FORMAT_S16_BE) < 0);
	} else {
		err =    (snd_pcm_hw_params_set_format (handle, hwpar, SND_PCM_FORMAT_FLOAT_LE) < 0)
		      && (snd_pcm_hw_params_set_format (handle, hwpar, SND_PCM_FORMAT_S32_LE)   < 0)
		      && (snd_pcm_hw_params_set_format (handle, hwpar, SND_PCM_FORMAT_S32_BE)   < 0)
		      && (snd_pcm_hw_params_set_format (handle, hwpar, SND_PCM_FORMAT_S24_3LE)  < 0)
		      && (snd_pcm_hw_params_set_format (handle, hwpar, SND_PCM_FORMAT_S24_3BE)  < 0)
		      && (snd_pcm_hw_params_set_format (handle, hwpar, SND_PCM_FORMAT_S16_LE)   < 0)
		      && (snd_pcm_hw_params_set_format (handle, hwpar, SND_PCM_FORMAT_S16_BE)   < 0);
	}

	if (err) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: no supported sample format on %s interface.\n.", sname);
		}
		return -1;
	}

	if (snd_pcm_hw_params_set_rate (handle, hwpar, _fsamp, 0) < 0) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: can't set %s sample rate to %u.\n", sname, _fsamp);
		}
		return -3;
	}

	snd_pcm_hw_params_get_channels_max (hwpar, nchan);
	if (*nchan > 1024) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: detected more than 1024 %s channels, reset to 2.\n", sname);
		}
		*nchan = 2;
	}
	if (_debug & FORCE_2CH) {
		*nchan = 2;
	}
	if (*nchan > MAXCHAN) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: number of %s channels reduced to %d.\n", sname, MAXCHAN);
		}
		*nchan = MAXCHAN;
	}

	if (snd_pcm_hw_params_set_channels (handle, hwpar, *nchan) < 0) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: can't set %s channel count to %u.\n", sname, *nchan);
		}
		return -1;
	}
	if (snd_pcm_hw_params_set_period_size_near (handle, hwpar, &_fsize, 0) < 0) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: can't set %s period size to %lu.\n", sname, _fsize);
		}
		return -4;
	}

	unsigned int nf = nfrag;
	snd_pcm_hw_params_set_periods_min (handle, hwpar, &nf, NULL);
	if (nf < nfrag) {
		nf = nfrag;
	}
	if (snd_pcm_hw_params_set_periods_near (handle, hwpar, &nf, NULL) < 0) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: can't set %s periods to %u (requested %u).\n", sname, nf, nfrag);
		}
		return -5;
	}

	if (_debug & DEBUG_INIT) {
		fprintf (stderr, "Alsa_pcmi: use %d periods for %s (requested %u).\n", nf, sname, nfrag);
	}

	if (snd_pcm_hw_params_set_buffer_size (handle, hwpar, _fsize * nf) < 0) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: can't set %s buffer length to %lu.\n", sname, _fsize * nf);
		}
		return -4;
	}
	if (snd_pcm_hw_params (handle, hwpar) < 0) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: can't set %s hardware parameters.\n", sname);
		}
		return -1;
	}

	return 0;
}

int
Alsa_pcmi::set_swpar (snd_pcm_t* handle, snd_pcm_sw_params_t* swpar, const char* sname)
{
	int err;

	snd_pcm_sw_params_current (handle, swpar);

	if ((err = snd_pcm_sw_params_set_tstamp_mode (handle, swpar, SND_PCM_TSTAMP_MMAP)) < 0) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: can't set %s timestamp mode to %u.\n", sname, SND_PCM_TSTAMP_MMAP);
		}
		return -1;
	}
	if ((err = snd_pcm_sw_params_set_avail_min (handle, swpar, _fsize)) < 0) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: can't set %s avail_min to %lu.\n", sname, _fsize);
		}
		return -1;
	}

	if (handle == _play_handle && snd_pcm_sw_params_set_start_threshold (_play_handle, _play_swpar, 0U) < 0) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: can't set %s start-threshold.\n", sname);
		}
		return -1;
	}

	if ((err = snd_pcm_sw_params (handle, swpar)) < 0) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: can't set %s software parameters.\n", sname);
		}
		return -1;
	}

	return 0;
}

int
Alsa_pcmi::recover (void)
{
	int               err;
	snd_pcm_status_t* stat;

	snd_pcm_status_alloca (&stat);

	if (_play_handle) {
		if ((err = snd_pcm_status (_play_handle, stat)) < 0) {
			if (_debug & DEBUG_STAT) {
				fprintf (stderr, "Alsa_pcmi: pcm_status(play): %s\n", snd_strerror (err));
			}
		}
		_play_xrun = xruncheck (stat);
	}
	if (_capt_handle) {
		if ((err = snd_pcm_status (_capt_handle, stat)) < 0) {
			if (_debug & DEBUG_STAT) {
				fprintf (stderr, "Alsa_pcmi: pcm_status(capt): %s\n", snd_strerror (err));
			}
		}
		_capt_xrun = xruncheck (stat);
	}

	if (pcm_stop ()) {
		return -1;
	}
	if (_play_handle && ((err = snd_pcm_prepare (_play_handle)) < 0)) {
		if (_debug & DEBUG_STAT) {
			fprintf (stderr, "Alsa_pcmi: pcm_prepare(play): %s\n", snd_strerror (err));
		}
		return -1;
	}
	if (_capt_handle && !_synced && ((err = snd_pcm_prepare (_capt_handle)) < 0)) {
		if (_debug & DEBUG_INIT) {
			fprintf (stderr, "Alsa_pcmi: pcm_prepare(capt): %s\n", snd_strerror (err));
		}
		return -1;
	}
	if (pcm_start ()) {
		return -1;
	}

	return 0;
}

float
Alsa_pcmi::xruncheck (snd_pcm_status_t* stat)
{
	struct timeval tupd, trig;
	int            ds, du;

	if (snd_pcm_status_get_state (stat) == SND_PCM_STATE_XRUN) {
		snd_pcm_status_get_tstamp (stat, &tupd);
		snd_pcm_status_get_trigger_tstamp (stat, &trig);
		ds = tupd.tv_sec - trig.tv_sec;
		du = tupd.tv_usec - trig.tv_usec;
		if (du < 0) {
			du += 1000000;
			ds -= 1;
		}
		return ds + 1e-6f * du;
	}
	return 0.0f;
}

char*
Alsa_pcmi::clear_16 (char* dst, int nfrm)
{
	while (nfrm--) {
		*((short int*)dst) = 0;
		dst += _play_step;
	}
	return dst;
}

char*
Alsa_pcmi::clear_24 (char* dst, int nfrm)
{
	while (nfrm--) {
		dst[0] = 0;
		dst[1] = 0;
		dst[2] = 0;
		dst += _play_step;
	}
	return dst;
}

char*
Alsa_pcmi::clear_32 (char* dst, int nfrm)
{
	while (nfrm--) {
		*((int*)dst) = 0;
		dst += _play_step;
	}
	return dst;
}

char*
Alsa_pcmi::play_16 (const float* src, char* dst, int nfrm, int step)
{
	while (nfrm--) {
		float s = *src;

		short int d;
		if (s > 1) {
			d = 0x7fff;
		} else if (s < -1) {
			d = 0x8001;
		} else {
			d = (short int)((float)0x7fff * s);
		}
		*((short int*)dst) = d;
		dst += _play_step;
		src += step;
	}
	return dst;
}

char*
Alsa_pcmi::play_16swap (const float* src, char* dst, int nfrm, int step)
{

	while (nfrm--) {
		float s = *src;

		short int d;
		if (s > 1) {
			d = 0x7fff;
		} else if (s < -1) {
			d = 0x8001;
		} else {
			d = (short int)((float)0x7fff * s);
		}
		dst[0] = d >> 8;
		dst[1] = d;
		dst += _play_step;
		src += step;
	}
	return dst;
}

char*
Alsa_pcmi::play_24 (const float* src, char* dst, int nfrm, int step)
{

	while (nfrm--) {
		float s = *src;
		int   d;
		if (s > 1) {
			d = 0x007fffff;
		} else if (s < -1) {
			d = 0x00800001;
		} else {
			d = (int)((float)0x007fffff * s);
		}
		dst[0] = d;
		dst[1] = d >> 8;
		dst[2] = d >> 16;
		dst += _play_step;
		src += step;
	}
	return dst;
}

char*
Alsa_pcmi::play_24swap (const float* src, char* dst, int nfrm, int step)
{
	while (nfrm--) {
		float s = *src;
		int   d;
		if (s > 1) {
			d = 0x007fffff;
		} else if (s < -1) {
			d = 0x00800001;
		} else {
			d = (int)((float)0x007fffff * s);
		}
		dst[0] = d >> 16;
		dst[1] = d >> 8;
		dst[2] = d;
		dst += _play_step;
		src += step;
	}
	return dst;
}

char*
Alsa_pcmi::play_32 (const float* src, char* dst, int nfrm, int step)
{
	while (nfrm--) {
		float s = *src;
		int   d;
		if (s > 1) {
			d = 0x007fffff;
		} else if (s < -1) {
			d = 0x00800001;
		} else {
			d = (int)((float)0x007fffff * s);
		}
		*((int*)dst) = d << 8;
		dst += _play_step;
		src += step;
	}
	return dst;
}

char*
Alsa_pcmi::play_32swap (const float* src, char* dst, int nfrm, int step)
{
	while (nfrm--) {
		float s = *src;
		int   d;
		if (s > 1) {
			d = 0x007fffff;
		} else if (s < -1) {
			d = 0x00800001;
		} else {
			d = (int)((float)0x007fffff * s);
		}
		dst[0] = d >> 16;
		dst[1] = d >> 8;
		dst[2] = d;
		dst[3] = 0;
		dst += _play_step;
		src += step;
	}
	return dst;
}

char*
Alsa_pcmi::play_float (const float* src, char* dst, int nfrm, int step)
{
	while (nfrm--) {
		*((float*)dst) = *src;
		dst += _play_step;
		src += step;
	}
	return dst;
}

const char*
Alsa_pcmi::capt_16 (const char* src, float* dst, int nfrm, int step)
{
	while (nfrm--) {
		const short int s = *((short int const*)src);
		const float     d = (float)s / (float)0x7fff;
		*dst = d;
		dst += step;
		src += _capt_step;
	}
	return src;
}

const char*
Alsa_pcmi::capt_16swap (const char* src, float* dst, int nfrm, int step)
{
	while (nfrm--) {
		short int s = (src[0] & 0xFF) << 8;
		s          += (src[1] & 0xFF);
		float d  = (float)s / (float)0x7fff;
		*dst = d;
		dst += step;
		src += _capt_step;
	}
	return src;
}

const char*
Alsa_pcmi::capt_24 (const char* src, float* dst, int nfrm, int step)
{
	while (nfrm--) {
		int s = (src[0] & 0xFF);
		s    += (src[1] & 0xFF) << 8;
		s    += (src[2] & 0xFF) << 16;
		if (s & 0x00800000) {
			s -= 0x01000000;
		}
		float d = (float)s / (float)0x007fffff;
		*dst = d;
		dst += step;
		src += _capt_step;
	}
	return src;
}

const char*
Alsa_pcmi::capt_24swap (const char* src, float* dst, int nfrm, int step)
{
	while (nfrm--) {
		int s = (src[0] & 0xFF) << 16;
		s    += (src[1] & 0xFF) << 8;
		s    += (src[2] & 0xFF);
		if (s & 0x00800000) {
			s -= 0x01000000;
		}
		float d = (float)s / (float)0x007fffff;
		*dst = d;
		dst += step;
		src += _capt_step;
	}
	return src;
}

const char*
Alsa_pcmi::capt_32 (const char* src, float* dst, int nfrm, int step)
{
	while (nfrm--) {
		const int   s = *((int const*)src);
		const float d = (float)s / (float)0x7fffff00;
		*dst = d;
		dst += step;
		src += _capt_step;
	}
	return src;
}

const char*
Alsa_pcmi::capt_32swap (const char* src, float* dst, int nfrm, int step)
{
	while (nfrm--) {
		int s = (src[0] & 0xFF) << 24;
		s    += (src[1] & 0xFF) << 16;
		s    += (src[2] & 0xFF) << 8;
		float d = (float)s / (float)0x7fffff00;
		*dst = d;
		dst += step;
		src += _capt_step;
	}
	return src;
}

const char*
Alsa_pcmi::capt_float (const char* src, float* dst, int nfrm, int step)
{
	while (nfrm--) {
		*dst = *((float const*)src);
		dst += step;
		src += _capt_step;
	}
	return src;
}
