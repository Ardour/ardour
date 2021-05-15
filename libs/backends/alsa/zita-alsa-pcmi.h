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
 */

#ifndef _ZITA_ALSA_PCMI_H_
#define _ZITA_ALSA_PCMI_H_

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API

#include <alsa/asoundlib.h>
#include <stdint.h>

class Alsa_pcmi
{
public:
	Alsa_pcmi (
	    const char*  play_name,
	    const char*  capt_name,
	    const char*  ctrl_name,
	    unsigned int rate,
	    unsigned int frsize,
	    unsigned int play_nfrags,
	    unsigned int capt_nfrags,
	    unsigned int debug = 0);

	~Alsa_pcmi (void);

	enum {
		DEBUG_INIT = 0x001,
		DEBUG_STAT = 0x002,
		DEBUG_WAIT = 0x004,
		DEBUG_DATA = 0x008,
		DEBUG_ALL  = 0x00f, // 15
		FORCE_16B  = 0x100, // 256,
		FORCE_2CH  = 0x200, // 512
		TRY_INTLVD = 0x400, // 1024
	};

	void printinfo (void);

	int pcm_start (void);
	int pcm_stop (void);
	int pcm_idle (int len);

	snd_pcm_sframes_t pcm_wait (void);

	int  play_init (snd_pcm_uframes_t len);
	void clear_chan (int chan, int len);
	void play_chan (int chan, const float* src, int len, int step = 1);
	int  play_done (int len);

	int  capt_init (snd_pcm_uframes_t len);
	void capt_chan (int chan, float* dst, int len, int step = 1);
	int  capt_done (int len);

	int play_avail (void)
	{
		return snd_pcm_avail (_play_handle);
	}

	int capt_avail (void)
	{
		return snd_pcm_avail (_capt_handle);
	}

	int play_delay (void)
	{
		long k;
		snd_pcm_delay (_play_handle, &k);
		return k;
	}

	int capt_delay (void)
	{
		long k;
		snd_pcm_delay (_capt_handle, &k);
		return k;
	}

	float play_xrun (void) const { return _play_xrun; }
	float capt_xrun (void) const { return _capt_xrun; }

	int      state (void) const { return _state; }
	size_t   fsize (void) const { return _fsize; }
	uint32_t fsamp (void) const { return _fsamp; }

	uint32_t play_nfrag (void) const { return _play_nfrag; }
	uint32_t capt_nfrag (void) const { return _capt_nfrag; }

	uint32_t nplay (void) const { return _play_nchan; }
	uint32_t ncapt (void) const { return _capt_nchan; }

	snd_pcm_t* play_handle (void) const { return _play_handle; }
	snd_pcm_t* capt_handle (void) const { return _capt_handle; }

private:
	typedef char* (Alsa_pcmi::*clear_function) (char*, int);
	typedef char* (Alsa_pcmi::*play_function) (const float*, char*, int, int);
	typedef const char* (Alsa_pcmi::*capt_function) (const char*, float*, int, int);

	enum { MAXPFD  = 16,
	       MAXCHAN = 128 };

	void  initialise (const char* play_name, const char* capt_name, const char* ctrl_name);
	int   set_hwpar (snd_pcm_t* handle, snd_pcm_hw_params_t* hwpar, const char* sname, unsigned int nfrag, unsigned int* nchan);
	int   set_swpar (snd_pcm_t* handle, snd_pcm_sw_params_t* swpar, const char* sname);
	int   recover (void);
	float xruncheck (snd_pcm_status_t* stat);

	char* clear_32 (char* dst, int nfrm);
	char* clear_24 (char* dst, int nfrm);
	char* clear_16 (char* dst, int nfrm);

	char* play_float (const float* src, char* dst, int nfrm, int step);
	char* play_32 (const float* src, char* dst, int nfrm, int step);
	char* play_24 (const float* src, char* dst, int nfrm, int step);
	char* play_16 (const float* src, char* dst, int nfrm, int step);
	char* play_32swap (const float* src, char* dst, int nfrm, int step);
	char* play_24swap (const float* src, char* dst, int nfrm, int step);
	char* play_16swap (const float* src, char* dst, int nfrm, int step);

	const char* capt_float (const char* src, float* dst, int nfrm, int step);
	const char* capt_32 (const char* src, float* dst, int nfrm, int step);
	const char* capt_24 (const char* src, float* dst, int nfrm, int step);
	const char* capt_16 (const char* src, float* dst, int nfrm, int step);
	const char* capt_32swap (const char* src, float* dst, int nfrm, int step);
	const char* capt_24swap (const char* src, float* dst, int nfrm, int step);
	const char* capt_16swap (const char* src, float* dst, int nfrm, int step);

	unsigned int         _fsamp;
	snd_pcm_uframes_t    _fsize;
	unsigned int         _play_nfrag;
	unsigned int         _real_nfrag;
	unsigned int         _capt_nfrag;
	unsigned int         _debug;
	int                  _state;
	snd_pcm_t*           _play_handle;
	snd_pcm_t*           _capt_handle;
	snd_ctl_t*           _ctrl_handle;
	snd_pcm_hw_params_t* _play_hwpar;
	snd_pcm_sw_params_t* _play_swpar;
	snd_pcm_hw_params_t* _capt_hwpar;
	snd_pcm_sw_params_t* _capt_swpar;
	snd_pcm_format_t     _play_format;
	snd_pcm_format_t     _capt_format;
	snd_pcm_access_t     _play_access;
	snd_pcm_access_t     _capt_access;
	unsigned int         _play_nchan;
	unsigned int         _capt_nchan;
	float                _play_xrun;
	float                _capt_xrun;
	bool                 _synced;
	int                  _play_npfd;
	int                  _capt_npfd;
	struct pollfd        _poll_fd[MAXPFD];
	snd_pcm_uframes_t    _capt_offs;
	snd_pcm_uframes_t    _play_offs;
	int                  _play_step;
	int                  _capt_step;
	char*                _play_ptr[MAXCHAN];
	const char*          _capt_ptr[MAXCHAN];
	clear_function       _clear_func;
	play_function        _play_func;
	capt_function        _capt_func;
	void*                _dummy[16];
};

#endif
