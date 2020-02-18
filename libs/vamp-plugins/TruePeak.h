/*
 * Copyright (C) 2006 Chris Cannam
 * Copyright (C) 2006-2012 Fons Adriaensen <fons@linuxaudio.org>
 * COPYRIGHT (C) 2012-2019 Robin Gareus <robin@gareus.org>
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

#ifndef _TruePeak_PLUGIN_H_
#define _TruePeak_PLUGIN_H_

#include <vamp-sdk/Plugin.h>

#include <pthread.h>

namespace TruePeakMeter {

class Resampler_mutex
{
private:

	friend class Resampler_table;

	Resampler_mutex (void) { pthread_mutex_init (&_mutex, 0); }
	~Resampler_mutex (void) { pthread_mutex_destroy (&_mutex); }
	void lock (void) { pthread_mutex_lock (&_mutex); }
	void unlock (void) { pthread_mutex_unlock (&_mutex); }

	pthread_mutex_t  _mutex;
};

class Resampler_table
{
private:

	Resampler_table (double fr, unsigned int hl, unsigned int np);
	~Resampler_table (void);

	friend class Resampler;
	friend class VResampler;

	Resampler_table     *_next;
	unsigned int         _refc;
	float               *_ctab;
	double               _fr;
	unsigned int         _hl;
	unsigned int         _np;

	static Resampler_table *create (double fr, unsigned int hl, unsigned int np);
	static void destroy (Resampler_table *T);

	static Resampler_table  *_list;
	static Resampler_mutex   _mutex;
};

class Resampler
{
public:

	Resampler (void);
	~Resampler (void);

	int  setup (unsigned int fs_inp,
			unsigned int fs_out,
			unsigned int nchan,
			unsigned int hlen);

	int  setup (unsigned int fs_inp,
			unsigned int fs_out,
			unsigned int nchan,
			unsigned int hlen,
			double       frel);

	void   clear (void);
	int    reset (void);
	int    nchan (void) const { return _nchan; }
	int    filtlen (void) const { return inpsize (); } // Deprecated
	int    inpsize (void) const;
	double inpdist (void) const;
	int    process (void);

	unsigned int         inp_count;
	unsigned int         out_count;
	float const         *inp_data;
	float               *out_data;
	void                *inp_list;
	void                *out_list;

private:

	Resampler_table     *_table;
	unsigned int         _nchan;
	unsigned int         _inmax;
	unsigned int         _index;
	unsigned int         _nread;
	unsigned int         _nzero;
	unsigned int         _phase;
	unsigned int         _pstep;
	float               *_buff;
	void                *_dummy [8];
};

class TruePeakdsp
{
public:

	TruePeakdsp (void);
	~TruePeakdsp (void);

	void process (float const *, int n);
	float read (void);
	void  read (float &m, float &p);
	void  reset (void);

	bool init (float fsamp);

private:

	float      _m;
	float      _p;
	bool       _res;
	bool       _res_peak;
	float     *_buf;
	Resampler  _src;
};

}; // namespace TruePeakMeter

class VampTruePeak : public Vamp::Plugin
{
public:
	VampTruePeak(float inputSampleRate);
	virtual ~VampTruePeak();

	size_t getMinChannelCount() const { return 1; }
	size_t getMaxChannelCount() const { return 1; }
	size_t getPreferredBlockSize () const { return 1024; }
	bool initialise(size_t channels, size_t stepSize, size_t blockSize);
	void reset();

	InputDomain getInputDomain() const { return TimeDomain; }

	std::string getIdentifier() const;
	std::string getName() const;
	std::string getDescription() const;
	std::string getMaker() const;
	int getPluginVersion() const;
	std::string getCopyright() const;

	OutputList getOutputDescriptors() const;

	FeatureSet process(const float *const *inputBuffers,
			Vamp::RealTime timestamp);

	FeatureSet getRemainingFeatures();

protected:
	size_t m_blockSize;

private:
	TruePeakMeter::TruePeakdsp _meter;
	Feature _above_m1;
	unsigned int m_rate;
};

#endif
