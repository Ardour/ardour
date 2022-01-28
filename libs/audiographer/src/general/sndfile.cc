/*
** Copyright (C) 2005-2007 Erik de Castro Lopo <erikd@mega-nerd.com>
**
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in
**       the documentation and/or other materials provided with the
**       distribution.
**     * Neither the author nor the names of any contributors may be used
**       to endorse or promote products derived from this software without
**       specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
** TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
** PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
** CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
** EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
** PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
** OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
** OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
** ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
** The above modified BSD style license (GPL and LGPL compatible) applies to
** this file. It does not apply to libsndfile itself which is released under
** the GNU LGPL or the libsndfile test suite which is released under the GNU
** GPL.
** This means that this header file can be used under this modified BSD style
** license, but the LGPL still holds for the libsndfile library itself.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include <new> // for std::nothrow

#include "private/sndfile.hh"

namespace AudioGrapher {

/*==============================================================================
**	Nothing but implementation below.
*/

SndfileHandle::SNDFILE_ref::SNDFILE_ref (void)
: ref (1)
{}

SndfileHandle::SNDFILE_ref::~SNDFILE_ref (void)
{	if (sf != NULL) { sf_close (sf) ; } }


void
SndfileHandle::close (void)
{
	if (p != NULL && --p->ref == 0)
	{
		delete p ;
		p = NULL;
	}
}


SndfileHandle::SndfileHandle (const char *path, int mode, int fmt, int chans, int srate)
: p (NULL)
{
	p = new (std::nothrow) SNDFILE_ref () ;

	if (p != NULL)
	{	p->ref = 1 ;

		p->sfinfo.frames = 0 ;
		p->sfinfo.channels = chans ;
		p->sfinfo.format = fmt ;
		p->sfinfo.samplerate = srate ;
		p->sfinfo.sections = 0 ;
		p->sfinfo.seekable = 0 ;

		bool writable = false;
		if (mode & SFM_WRITE) {
			writable = true;
		}
		if (writable) {
			::g_unlink (path);
		}
#ifdef PLATFORM_WINDOWS
		int fd = g_open (path, writable ? O_CREAT | O_RDWR : O_RDONLY, writable ? 0644 : 0444);
#else
		int fd = ::open (path, writable ? O_CREAT | O_RDWR : O_RDONLY, writable ? 0644 : 0444);
#endif

		p->sf = sf_open_fd (fd, mode, &p->sfinfo, true) ;
		} ;

	return ;
} /* SndfileHandle const char * constructor */

SndfileHandle::SndfileHandle (std::string const & path, int mode, int fmt, int chans, int srate)
: p (NULL)
{
	p = new (std::nothrow) SNDFILE_ref () ;

	if (p != NULL)
	{	p->ref = 1 ;

		p->sfinfo.frames = 0 ;
		p->sfinfo.channels = chans ;
		p->sfinfo.format = fmt ;
		p->sfinfo.samplerate = srate ;
		p->sfinfo.sections = 0 ;
		p->sfinfo.seekable = 0 ;

		bool writable = false;
		if (mode & SFM_WRITE) {
			writable = true;
		}
		if (writable) {
			::g_unlink (path.c_str());
		}
#ifdef PLATFORM_WINDOWS
		int fd = g_open (path.c_str(), writable ? O_CREAT | O_RDWR : O_RDONLY, writable ? 0644 : 0444);
#else
		int fd = ::open (path.c_str(), writable ? O_CREAT | O_RDWR : O_RDONLY, writable ? 0644 : 0444);
#endif

		p->sf = sf_open_fd (fd, mode, &p->sfinfo, true) ;
		} ;

	return ;
} /* SndfileHandle std::string constructor */

SndfileHandle::SndfileHandle (int fd, bool close_desc, int mode, int fmt, int chans, int srate)
: p (NULL)
{
	if (fd < 0)
		return;

	p = new (std::nothrow) SNDFILE_ref () ;

	if (p != NULL)
	{	p->ref = 1 ;

		p->sfinfo.frames = 0 ;
		p->sfinfo.channels = chans ;
		p->sfinfo.format = fmt ;
		p->sfinfo.samplerate = srate ;
		p->sfinfo.sections = 0 ;
		p->sfinfo.seekable = 0 ;

		p->sf = sf_open_fd (fd, mode, &p->sfinfo, close_desc) ;
		} ;

	return ;
} /* SndfileHandle fd constructor */


SndfileHandle::~SndfileHandle (void)
{	if (p != NULL && --p->ref == 0)
		delete p ;
} /* SndfileHandle destructor */


SndfileHandle::SndfileHandle (const SndfileHandle &orig)
: p (orig.p)
{	if (p != NULL)
		++p->ref ;
} /* SndfileHandle copy constructor */

SndfileHandle &
SndfileHandle::operator = (const SndfileHandle &rhs)
{
	if (&rhs == this)
		return *this ;
	if (p != NULL && --p->ref == 0)
		delete p ;

	p = rhs.p ;
	if (p != NULL)
		++p->ref ;

	return *this ;
} /* SndfileHandle assignment operator */

int
SndfileHandle::error (void) const
{	return sf_error (p->sf) ; }

const char *
SndfileHandle::strError (void) const
{	return sf_strerror (p->sf) ; }

int
SndfileHandle::command (int cmd, void *data, int datasize)
{	return sf_command (p->sf, cmd, data, datasize) ; }

sf_count_t
SndfileHandle::seek (sf_count_t frame_count, int whence)
{	return sf_seek (p->sf, frame_count, whence) ; }

void
SndfileHandle::writeSync (void)
{	sf_write_sync (p->sf) ; }

int
SndfileHandle::setString (int str_type, const char* str)
{	return sf_set_string (p->sf, str_type, str) ; }

const char*
SndfileHandle::getString (int str_type) const
{	return sf_get_string (p->sf, str_type) ; }

int
SndfileHandle::formatCheck(int fmt, int chans, int srate)
{
	SF_INFO sfinfo ;

	sfinfo.frames = 0 ;
	sfinfo.channels = chans ;
	sfinfo.format = fmt ;
	sfinfo.samplerate = srate ;
	sfinfo.sections = 0 ;
	sfinfo.seekable = 0 ;

	return sf_format_check (&sfinfo) ;
}

/*---------------------------------------------------------------------*/

sf_count_t
SndfileHandle::read (short *ptr, sf_count_t items)
{	return sf_read_short (p->sf, ptr, items) ; }

sf_count_t
SndfileHandle::read (int *ptr, sf_count_t items)
{	return sf_read_int (p->sf, ptr, items) ; }

sf_count_t
SndfileHandle::read (float *ptr, sf_count_t items)
{	return sf_read_float (p->sf, ptr, items) ; }

sf_count_t
SndfileHandle::read (double *ptr, sf_count_t items)
{	return sf_read_double (p->sf, ptr, items) ; }

sf_count_t
SndfileHandle::write (const short *ptr, sf_count_t items)
{	return sf_write_short (p->sf, ptr, items) ; }

sf_count_t
SndfileHandle::write (const int *ptr, sf_count_t items)
{	return sf_write_int (p->sf, ptr, items) ; }

sf_count_t
SndfileHandle::write (const float *ptr, sf_count_t items)
{	return sf_write_float (p->sf, ptr, items) ; }

sf_count_t
SndfileHandle::write (const double *ptr, sf_count_t items)
{	return sf_write_double (p->sf, ptr, items) ; }

sf_count_t
SndfileHandle::readf (short *ptr, sf_count_t frame_count)
{	return sf_readf_short (p->sf, ptr, frame_count) ; }

sf_count_t
SndfileHandle::readf (int *ptr, sf_count_t frame_count)
{	return sf_readf_int (p->sf, ptr, frame_count) ; }

sf_count_t
SndfileHandle::readf (float *ptr, sf_count_t frame_count)
{	return sf_readf_float (p->sf, ptr, frame_count) ; }

sf_count_t
SndfileHandle::readf (double *ptr, sf_count_t frame_count)
{	return sf_readf_double (p->sf, ptr, frame_count) ; }

sf_count_t
SndfileHandle::writef (const short *ptr, sf_count_t frame_count)
{	return sf_writef_short (p->sf, ptr, frame_count) ; }

sf_count_t
SndfileHandle::writef (const int *ptr, sf_count_t frame_count)
{	return sf_writef_int (p->sf, ptr, frame_count) ; }

sf_count_t
SndfileHandle::writef (const float *ptr, sf_count_t frame_count)
{	return sf_writef_float (p->sf, ptr, frame_count) ; }

sf_count_t
SndfileHandle::writef (const double *ptr, sf_count_t frame_count)
{	return sf_writef_double (p->sf, ptr, frame_count) ; }

sf_count_t
SndfileHandle::readRaw (void *ptr, sf_count_t bytes)
{	return sf_read_raw (p->sf, ptr, bytes) ; }

sf_count_t
SndfileHandle::writeRaw (const void *ptr, sf_count_t bytes)
{	return sf_write_raw (p->sf, ptr, bytes) ; }


#ifdef ENABLE_SNDFILE_WINDOWS_PROTOTYPES

SndfileHandle::SndfileHandle (LPCWSTR wpath, int mode, int fmt, int chans, int srate)
: p (NULL)
{
	p = new (std::nothrow) SNDFILE_ref () ;

	if (p != NULL)
	{	p->ref = 1 ;

		p->sfinfo.frames = 0 ;
		p->sfinfo.channels = chans ;
		p->sfinfo.format = fmt ;
		p->sfinfo.samplerate = srate ;
		p->sfinfo.sections = 0 ;
		p->sfinfo.seekable = 0 ;

		p->sf = sf_wchar_open (wpath, mode, &p->sfinfo) ;
		} ;

	return ;
} /* SndfileHandle const wchar_t * constructor */

#endif

} // namespace AudioGrapher
