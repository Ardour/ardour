// ----------------------------------------------------------------------------
//
//  Copyright (C) 2006-2018 Fons Adriaensen <fons@linuxaudio.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ----------------------------------------------------------------------------

#ifndef ARDOUR_ZITA_CONVOLVER_H
#define ARDOUR_ZITA_CONVOLVER_H


#include <fftw3.h>
#include <pthread.h>
#include <stdint.h>

#include "zita-convolver/zconvolver_visibility.h"

namespace ArdourZita {

#ifdef ZCSEMA_IS_IMPLEMENTED
#undef ZCSEMA_IS_IMPLEMENTED
#endif

/* note: mingw and msvc actually use PTW32's implementation of semaphores */
#if defined(__linux__) || defined(__GNU__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || defined(PTW32_VERSION) || defined (__WINPTHREADS_VERSION)

#include <semaphore.h>

class LIBZCONVOLVER_API ZCsema
{
public:
	ZCsema (void)
	{
		init (0, 0);
	}

	~ZCsema (void)
	{
		sem_destroy (&_sema);
	}

	int init (int s, int v)
	{
		return sem_init (&_sema, s, v);
	}

	int post (void)
	{
		return sem_post (&_sema);
	}

	int wait (void)
	{
		return sem_wait (&_sema);
	}

	int trywait (void)
	{
		return sem_trywait (&_sema);
	}

private:
	ZCsema (const ZCsema&);            // disabled
	ZCsema& operator= (const ZCsema&); // disabled

	sem_t _sema;
};

#define ZCSEMA_IS_IMPLEMENTED

#elif defined (__APPLE__)

// NOTE:  ***** I DO NOT REPEAT NOT PROVIDE SUPPORT FOR OSX *****
//
// The following code partially emulates the POSIX sem_t for which
// OSX has only a crippled implementation. It may or may not compile,
// and if it compiles it may or may not work correctly. Blame APPLE
// for not following POSIX standards.

class LIBZCONVOLVER_API ZCsema
{
public:
	ZCsema (void) : _count (0)
	{
		init (0, 0);
	}

	~ZCsema (void)
	{
		pthread_mutex_destroy (&_mutex);
		pthread_cond_destroy (&_cond);
	}

	ZCsema (const ZCsema&);            // disabled
	ZCsema& operator= (const ZCsema&); // disabled

	int init (int s, int v)
	{
		_count = v;
		return pthread_mutex_init (&_mutex, 0) || pthread_cond_init (&_cond, 0);
	}

	int post (void)
	{
		pthread_mutex_lock (&_mutex);
		_count++;
		if (_count == 1) {
			pthread_cond_signal (&_cond);
		}
		pthread_mutex_unlock (&_mutex);
		return 0;
	}

	int wait (void)
	{
		pthread_mutex_lock (&_mutex);
		while (_count < 1) {
			pthread_cond_wait (&_cond, &_mutex);
		}
		_count--;
		pthread_mutex_unlock (&_mutex);
		return 0;
	}

	int trywait (void)
	{
		if (pthread_mutex_trylock (&_mutex)) {
			return -1;
		}
		if (_count < 1) {
			pthread_mutex_unlock (&_mutex);
			return -1;
		}
		_count--;
		pthread_mutex_unlock (&_mutex);
		return 0;
	}

private:
	int             _count;
	pthread_mutex_t _mutex;
	pthread_cond_t  _cond;
};

#define ZCSEMA_IS_IMPLEMENTED
#endif

#ifndef ZCSEMA_IS_IMPLEMENTED
#error "The ZCsema class is not implemented."
#endif

// ----------------------------------------------------------------------------

class LIBZCONVOLVER_API Inpnode
{
private:
	friend class Convlevel;

	Inpnode (uint16_t inp);
	~Inpnode (void);
	void alloc_ffta (uint16_t npar, int32_t size);
	void free_ffta (void);

	Inpnode*        _next;
	fftwf_complex** _ffta;
	uint16_t        _npar;
	uint16_t        _inp;
};

class LIBZCONVOLVER_API Macnode
{
private:
	friend class Convlevel;

	Macnode (Inpnode* inpn);
	~Macnode (void);
	void alloc_fftb (uint16_t npar);
	void free_fftb (void);

	Macnode*        _next;
	Inpnode*        _inpn;
	Macnode*        _link;
	fftwf_complex** _fftb;
	uint16_t        _npar;
};

class LIBZCONVOLVER_API Outnode
{
private:
	friend class Convlevel;

	Outnode (uint16_t out, int32_t size);
	~Outnode (void);

	Outnode* _next;
	Macnode* _list;
	float*   _buff[3];
	uint16_t _out;
};

class LIBZCONVOLVER_API Converror
{
public:
	enum {
		BAD_STATE = -1,
		BAD_PARAM = -2,
		MEM_ALLOC = -3
	};

	Converror (int error) : _error (error) {}

private:
	int _error;
};

class LIBZCONVOLVER_API Convlevel
{
private:
	friend class Convproc;

	enum {
		OPT_FFTW_MEASURE = 1,
		OPT_VECTOR_MODE  = 2,
		OPT_LATE_CONTIN  = 4
	};

	enum {
		ST_IDLE,
		ST_TERM,
		ST_PROC
	};

	Convlevel (void);
	~Convlevel (void);

	void configure (int      prio,
	                uint32_t offs,
	                uint32_t npar,
	                uint32_t parsize,
	                uint32_t options);

	void impdata_write (uint32_t inp,
	                    uint32_t out,
	                    int32_t  step,
	                    float*   data,
	                    int32_t  ind0,
	                    int32_t  ind1,
	                    bool     create);

	void impdata_clear (uint32_t inp,
	                    uint32_t out);

	void reset (uint32_t inpsize,
	            uint32_t outsize,
	            float**  inpbuff,
	            float**  outbuff);

	void start (int absprio, int policy);

	void process ();

	int readout ();
	int readtail (uint32_t n_samples);

	void stop (void);

	void cleanup (void);

	void fftswap (fftwf_complex* p);

	void print (FILE* F);

	static void* static_main (void* arg);

	void main (void);

	Macnode* findmacnode (uint32_t inp, uint32_t out, bool create);

	volatile uint32_t _stat;      // current processing state
	int               _prio;      // relative priority
	uint32_t          _offs;      // offset from start of impulse response
	uint32_t          _npar;      // number of partitions
	uint32_t          _parsize;   // partition and outbut buffer size
	uint32_t          _outsize;   // step size for output buffer
	uint32_t          _outoffs;   // offset into output buffer
	uint32_t          _inpsize;   // size of shared input buffer
	uint32_t          _inpoffs;   // offset into input buffer
	uint32_t          _options;   // various options
	uint32_t          _ptind;     // rotating partition index
	uint32_t          _opind;     // rotating output buffer index
	int               _bits;      // bit identifiying this level
	int               _wait;      // number of unfinished cycles
	pthread_t         _pthr;      // posix thread executing this level
	ZCsema            _trig;      // sema used to trigger a cycle
	ZCsema            _done;      // sema used to wait for a cycle
	Inpnode*          _inp_list;  // linked list of active inputs
	Outnode*          _out_list;  // linked list of active outputs
	fftwf_plan        _plan_r2c;  // FFTW plan, forward FFT
	fftwf_plan        _plan_c2r;  // FFTW plan, inverse FFT
	float*            _time_data; // workspace
	float*            _prep_data; // workspace
	fftwf_complex*    _freq_data; // workspace
	float**           _inpbuff;   // array of shared input buffers
	float**           _outbuff;   // array of shared output buffers
};

// ----------------------------------------------------------------------------

class LIBZCONVOLVER_API Convproc
{
public:
	Convproc (void);
	~Convproc (void);

	enum {
		ST_IDLE,
		ST_STOP,
		ST_WAIT,
		ST_PROC
	};

	enum {
		FL_LATE = 0x0000FFFF,
		FL_LOAD = 0x01000000
	};

	enum {
		OPT_FFTW_MEASURE = Convlevel::OPT_FFTW_MEASURE,
		OPT_VECTOR_MODE  = Convlevel::OPT_VECTOR_MODE,
		OPT_LATE_CONTIN  = Convlevel::OPT_LATE_CONTIN
	};

	enum {
		MAXINP   = 64,
		MAXOUT   = 64,
		MAXLEV   = 8,
		MINPART  = 64,
		MAXPART  = 8192,
		MAXDIVIS = 16,
		MINQUANT = 16,
		MAXQUANT = 8192
	};

	uint32_t state (void) const
	{
		return _state;
	}

	float* inpdata (uint32_t inp) const
	{
		return _inpbuff[inp] + _inpoffs;
	}

	float* outdata (uint32_t out) const
	{
		return _outbuff[out] + _outoffs;
	}

	int configure (uint32_t ninp,
	               uint32_t nout,
	               uint32_t maxsize,
	               uint32_t quantum,
	               uint32_t minpart,
	               uint32_t maxpart,
	               float    density);

	int impdata_create (uint32_t inp,
	                    uint32_t out,
	                    int32_t  step,
	                    float*   data,
	                    int32_t  ind0,
	                    int32_t  ind1);

	int impdata_clear (uint32_t inp,
	                   uint32_t out);

	void set_options (uint32_t options);

	int reset (void);

	int start_process (int abspri, int policy);

	int process ();
	int tailonly (uint32_t n_samples);

	int stop_process (void);

	bool check_started (uint32_t);
	bool check_stop (void);

	int cleanup (void);

	void print (FILE* F = stdout);

private:
	uint32_t   _state;           // current state
	float*     _inpbuff[MAXINP]; // input buffers
	float*     _outbuff[MAXOUT]; // output buffers
	uint32_t   _inpoffs;         // current offset in input buffers
	uint32_t   _outoffs;         // current offset in output buffers
	uint32_t   _options;         // option bits
	uint32_t   _ninp;            // number of inputs
	uint32_t   _nout;            // number of outputs
	uint32_t   _quantum;         // processing block size
	uint32_t   _minpart;         // smallest partition size
	uint32_t   _maxpart;         // largest allowed partition size
	uint32_t   _nlevels;         // number of partition sizes
	uint32_t   _inpsize;         // size of input buffers
	uint32_t   _latecnt;         // count of cycles ending too late
	Convlevel* _convlev[MAXLEV]; // array of processors
	void*      _dummy[64];

	static float _mac_cost;
	static float _fft_cost;
};

// ----------------------------------------------------------------------------

} /* end namespace */

#endif
