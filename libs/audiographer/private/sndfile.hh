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

/*
** sndfile.hh -- A lightweight C++ wrapper for the libsndfile API.
**
** All the methods are inlines and all functionality is contained in this
** file. There is no separate implementation file.
**
** API documentation is in the doc/ directory of the source code tarball
** and at http://www.mega-nerd.com/libsndfile/api.html.
*/

#ifndef SNDFILE_HH
#define SNDFILE_HH

#include "audiographer/visibility.h"

#include <sndfile.h>

#include <string>

// Prevent conflicts
namespace AudioGrapher {

class LIBAUDIOGRAPHER_API SndfileHandle
{	private :
		struct SNDFILE_ref
		{	SNDFILE_ref (void) ;
			~SNDFILE_ref (void) ;

			SNDFILE *sf ;
			SF_INFO sfinfo ;
			int ref ;
			} ;

		SNDFILE_ref *p ;

	public :
			/* Default constructor */
			SndfileHandle (void) : p (NULL) {} ;
			SndfileHandle (const char *path, int mode = SFM_READ,
							int format = 0, int channels = 0, int samplerate = 0) ;
			SndfileHandle (std::string const & path, int mode = SFM_READ,
							int format = 0, int channels = 0, int samplerate = 0) ;
			SndfileHandle (int fd, bool close_desc, int mode = SFM_READ,
							int format = 0, int channels = 0, int samplerate = 0) ;
			~SndfileHandle (void) ;

			SndfileHandle (const SndfileHandle &orig) ;
			SndfileHandle & operator = (const SndfileHandle &rhs) ;

			void close (void) ;

		/* Mainly for debugging/testing. */
		int refCount (void) const { return (p == NULL) ? 0 : p->ref ; }

		operator bool () const { return (p != NULL) ; }

		bool operator == (const SndfileHandle &rhs) const { return (p == rhs.p) ; }

		sf_count_t	frames (void) const		{ return p ? p->sfinfo.frames : 0 ; }
		int			format (void) const		{ return p ? p->sfinfo.format : 0 ; }
		int			channels (void) const	{ return p ? p->sfinfo.channels : 0 ; }
		int			samplerate (void) const { return p ? p->sfinfo.samplerate : 0 ; }

		int error (void) const ;
		const char * strError (void) const ;

		int command (int cmd, void *data, int datasize) ;

		sf_count_t	seek (sf_count_t frames, int whence) ;

		void writeSync (void) ;

		int setString (int str_type, const char* str) ;

		const char* getString (int str_type) const ;

		static int formatCheck (int format, int channels, int samplerate) ;

		sf_count_t read (short *ptr, sf_count_t items) ;
		sf_count_t read (int *ptr, sf_count_t items) ;
		sf_count_t read (float *ptr, sf_count_t items) ;
		sf_count_t read (double *ptr, sf_count_t items) ;

		sf_count_t write (const short *ptr, sf_count_t items) ;
		sf_count_t write (const int *ptr, sf_count_t items) ;
		sf_count_t write (const float *ptr, sf_count_t items) ;
		sf_count_t write (const double *ptr, sf_count_t items) ;

		sf_count_t readf (short *ptr, sf_count_t frames) ;
		sf_count_t readf (int *ptr, sf_count_t frames) ;
		sf_count_t readf (float *ptr, sf_count_t frames) ;
		sf_count_t readf (double *ptr, sf_count_t frames) ;

		sf_count_t writef (const short *ptr, sf_count_t frames) ;
		sf_count_t writef (const int *ptr, sf_count_t frames) ;
		sf_count_t writef (const float *ptr, sf_count_t frames) ;
		sf_count_t writef (const double *ptr, sf_count_t frames) ;

		sf_count_t	readRaw		(void *ptr, sf_count_t bytes) ;
		sf_count_t	writeRaw	(const void *ptr, sf_count_t bytes) ;

} ;

} // namespace AudioGrapher

#endif	/* SNDFILE_HH */

