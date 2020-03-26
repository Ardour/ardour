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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "TruePeak.h"

namespace TruePeakMeter {

static double sinc (double x)
{
	x = fabs (x);
	if (x < 1e-6) return 1.0;
	x *= M_PI;
	return sin (x) / x;
}

static double wind (double x)
{
	x = fabs (x);
	if (x >= 1.0) return 0.0f;
	x *= M_PI;
	return 0.384 + 0.500 * cos (x) + 0.116 * cos (2 * x);
}

Resampler_table  *Resampler_table::_list = 0;
Resampler_mutex   Resampler_table::_mutex;

Resampler_table::Resampler_table (double fr, unsigned int hl, unsigned int np)
	: _next (0)
	, _refc (0)
	, _fr (fr)
	, _hl (hl)
	, _np (np)
{
	unsigned int  i, j;
	double        t;
	float        *p;

	_ctab = new float [hl * (np + 1)];
	p = _ctab;
	for (j = 0; j <= np; j++)
	{
		t = (double) j / (double) np;
		for (i = 0; i < hl; i++)
		{
			p [hl - i - 1] = (float)(fr * sinc (t * fr) * wind (t / hl));
			t += 1;
		}
		p += hl;
	}
}

Resampler_table::~Resampler_table (void)
{
	delete[] _ctab;
}

Resampler_table *
Resampler_table::create (double fr, unsigned int hl, unsigned int np)
{
	Resampler_table *P;

	_mutex.lock ();
	P = _list;
	while (P)
	{
		if ((fr >= P->_fr * 0.999) && (fr <= P->_fr * 1.001) && (hl == P->_hl) && (np == P->_np))
		{
			P->_refc++;
			_mutex.unlock ();
			return P;
		}
		P = P->_next;
	}
	P = new Resampler_table (fr, hl, np);
	P->_refc = 1;
	P->_next = _list;
	_list = P;
	_mutex.unlock ();
	return P;
}

void
Resampler_table::destroy (Resampler_table *T)
{
	Resampler_table *P, *Q;

	_mutex.lock ();
	if (T)
	{
		T->_refc--;
		if (T->_refc == 0)
		{
			P = _list;
			Q = 0;
			while (P)
			{
				if (P == T)
				{
					if (Q) Q->_next = T->_next;
					else      _list = T->_next;
					break;
				}
				Q = P;
				P = P->_next;
			}
			delete T;
		}
	}
	_mutex.unlock ();
}

static unsigned int
gcd (unsigned int a, unsigned int b)
{
	if (a == 0) return b;
	if (b == 0) return a;
	while (1)
	{
		if (a > b)
		{
			a = a % b;
			if (a == 0) return b;
			if (a == 1) return 1;
		}
		else
		{
			b = b % a;
			if (b == 0) return a;
			if (b == 1) return 1;
		}
	}
	return 1;
}

Resampler::Resampler (void)
	: _table (0)
	, _nchan (0)
	, _buff  (0)
{
	reset ();
}

Resampler::~Resampler (void)
{
	clear ();
}

int
Resampler::setup (unsigned int fs_inp,
                  unsigned int fs_out,
                  unsigned int nchan,
                  unsigned int hlen)
{
	if ((hlen < 8) || (hlen > 96)) return 1;
	return setup (fs_inp, fs_out, nchan, hlen, 1.0 - 2.6 / hlen);
}

int
Resampler::setup (unsigned int fs_inp,
                  unsigned int fs_out,
                  unsigned int nchan,
                  unsigned int hlen,
                  double       frel)
{
	unsigned int       g, h, k, n, s;
	double             r;
	float              *B = 0;
	Resampler_table    *T = 0;

	k = s = 0;
	if (fs_inp && fs_out && nchan)
	{
		r = (double) fs_out / (double) fs_inp;
		g = gcd (fs_out, fs_inp);
		n = fs_out / g;
		s = fs_inp / g;
		if ((16 * r >= 1) && (n <= 1000))
		{
			h = hlen;
			k = 250;
			if (r < 1)
			{
				frel *= r;
				h = (unsigned int)(ceil (h / r));
				k = (unsigned int)(ceil (k / r));
			}
			T = Resampler_table::create (frel, h, n);
			B = new float [nchan * (2 * h - 1 + k)];
		}
	}
	clear ();
	if (T)
	{
		_table = T;
		_buff  = B;
		_nchan = nchan;
		_inmax = k;
		_pstep = s;
		return reset ();
	} else {
		delete[] B;
		return 1;
	}
}

void
Resampler::clear (void)
{
	Resampler_table::destroy (_table);
	delete[] _buff;
	_buff  = 0;
	_table = 0;
	_nchan = 0;
	_inmax = 0;
	_pstep = 0;
	reset ();
}

double
Resampler::inpdist (void) const
{
	if (!_table) return 0;
	return (int)(_table->_hl + 1 - _nread) - (double)_phase / _table->_np;
}

int
Resampler::inpsize (void) const
{
	if (!_table) return 0;
	return 2 * _table->_hl;
}

int
Resampler::reset (void)
{
	if (!_table) return 1;

	inp_count = 0;
	out_count = 0;
	inp_data = 0;
	out_data = 0;
	_index = 0;
	_nread = 0;
	_nzero = 0;
	_phase = 0;
	if (_table)
	{
		_nread = 2 * _table->_hl;
		return 0;
	}
	return 1;
}

int
Resampler::process (void)
{
	unsigned int   hl, ph, np, dp, in, nr, nz, i, n, c;
	float          *p1, *p2;

	if (!_table) return 1;

	hl = _table->_hl;
	np = _table->_np;
	dp = _pstep;
	in = _index;
	nr = _nread;
	ph = _phase;
	nz = _nzero;
	n = (2 * hl - nr) * _nchan;
	p1 = _buff + in * _nchan;
	p2 = p1 + n;

	while (out_count)
	{
		if (nr)
		{
			if (inp_count == 0) break;
			if (inp_data)
			{
				for (c = 0; c < _nchan; c++) p2 [c] = inp_data [c];
				inp_data += _nchan;
				nz = 0;
			}
			else
			{
				for (c = 0; c < _nchan; c++) p2 [c] = 0;
				if (nz < 2 * hl) nz++;
			}
			nr--;
			p2 += _nchan;
			inp_count--;
		}
		else
		{
			if (out_data)
			{
				if (nz < 2 * hl)
				{
					float *c1 = _table->_ctab + hl * ph;
					float *c2 = _table->_ctab + hl * (np - ph);
					for (c = 0; c < _nchan; c++)
					{
						float *q1 = p1 + c;
						float *q2 = p2 + c;
						float s = 1e-20f;
						for (i = 0; i < hl; i++)
						{
							q2 -= _nchan;
							s += *q1 * c1 [i] + *q2 * c2 [i];
							q1 += _nchan;
						}
						*out_data++ = s - 1e-20f;
					}
				}
				else
				{
					for (c = 0; c < _nchan; c++) *out_data++ = 0;
				}
			}
			out_count--;

			ph += dp;
			if (ph >= np)
			{
				nr = ph / np;
				ph -= nr * np;
				in += nr;
				p1 += nr * _nchan;;
				if (in >= _inmax)
				{
					n = (2 * hl - nr) * _nchan;
					memcpy (_buff, p1, n * sizeof (float));
					in = 0;
					p1 = _buff;
					p2 = p1 + n;
				}
			}
		}
	}
	_index = in;
	_nread = nr;
	_phase = ph;
	_nzero = nz;

	return 0;
}

TruePeakdsp::TruePeakdsp (void)
	: _m (0)
	, _p (0)
	, _res (true)
	, _res_peak (true)
	, _buf (NULL)
{
}

TruePeakdsp::~TruePeakdsp (void)
{
	free(_buf);
}

void
TruePeakdsp::process (float const *d, int n)
{
	_src.inp_count = n;
	_src.inp_data = d;
	_src.out_count = n * 4;
	_src.out_data = _buf;
	_src.process ();

	float x = 0;
	float v;
	assert (_buf);
	float *b = _buf;
	while (n--) {
		v = fabsf(*b++);
		if (v > x) x = v;
		v = fabsf(*b++);
		if (v > x) x = v;
		v = fabsf(*b++);
		if (v > x) x = v;
		v = fabsf(*b++);
		if (v > x) x = v;
	}

	if (_res) {
		_m = x;
		_res = false;
	} else if (x > _m) {
		_m = x;
	}

	if (_res_peak) {
		_p = x;
		_res_peak = false;
	} else if (x > _p) {
		_p = x;
	}
}

float
TruePeakdsp::read (void)
{
	_res = true;
	return _m;
}

void
TruePeakdsp::read (float &m, float &p)
{
	_res = true;
	_res_peak = true;
	m = _m;
	p = _p;
}

void
TruePeakdsp::reset ()
{
	_res = true;
	_m = 0;
	_p = 0;
}

bool
TruePeakdsp::init (float fsamp)
{
	_src.setup(fsamp, fsamp * 4.0, 1, 24, 1.0);
	_buf = (float*) malloc(32768 * sizeof(float));
	if (!_buf) {
		return false;
	}

	/* q/d initialize */
	float zero[8192];
	for (int i = 0; i < 8192; ++i) {
		zero[i]= 0.0;
	}
	_src.inp_count = 8192;
	_src.inp_data = zero;
	_src.out_count = 32768;
	_src.out_data = _buf;
	_src.process ();
	return true;
}

}

///////////////////////////////////////////////////////////////////////////////

using std::string;
using std::vector;
using std::cerr;
using std::endl;
using namespace TruePeakMeter;

VampTruePeak::VampTruePeak(float inputSampleRate)
    : Plugin(inputSampleRate)
    , m_blockSize(0)
    , m_rate (inputSampleRate)
{
}

VampTruePeak::~VampTruePeak()
{
}

string
VampTruePeak::getIdentifier() const
{
	return "dBTP";
}

string
VampTruePeak::getName() const
{
	return "dBTP Meter";
}

string
VampTruePeak::getDescription() const
{
	return "True Peak Meter (4x Oversampling)";
}

string
VampTruePeak::getMaker() const
{
	return "Robin Gareus, Fons Adrianesen";
}

int
VampTruePeak::getPluginVersion() const
{
	return 2;
}

string
VampTruePeak::getCopyright() const
{
	return "GPL version 3 or later";
}

bool
VampTruePeak::initialise(size_t channels, size_t stepSize, size_t blockSize)
{
	if (channels < getMinChannelCount() ||
			channels > getMaxChannelCount()) {
		return false;
	}

	if (blockSize == 0 || blockSize > 8192) {
		return false;
	}

	if (!_meter.init (m_inputSampleRate)) {
		return false;
	}

	m_blockSize = blockSize;

	return true;
}

void
VampTruePeak::reset()
{
	_meter.reset ();
}

VampTruePeak::OutputList
VampTruePeak::getOutputDescriptors() const
{
	OutputList list;

	OutputDescriptor zc;
	zc.identifier = "level";
	zc.name = "TruePeak";
	zc.description = "TruePeak (4x Oversampling)";
	zc.unit = "dbTP";
	zc.hasFixedBinCount = true;
	zc.binCount = 0;
	zc.hasKnownExtents = false;
	zc.isQuantized = false;
	zc.sampleType = OutputDescriptor::OneSamplePerStep;
	list.push_back(zc);

	zc.identifier = "peaks";
	zc.name = "TruePeakPeaks";
	zc.description = "Location of Peaks above -1dBTP";
	zc.unit = "sec";
	zc.hasFixedBinCount = true;
	zc.binCount = 0;
	zc.hasKnownExtents = false;
	zc.isQuantized = false;
	zc.sampleType = OutputDescriptor::OneSamplePerStep;
	list.push_back(zc);

	return list;
}

VampTruePeak::FeatureSet
VampTruePeak::process(const float *const *inputBuffers,
                      Vamp::RealTime timestamp)
{
	if (m_blockSize == 0) {
		cerr << "ERROR: VampTruePeak::process: "
			<< "VampTruePeak has not been initialised"
			<< endl;
		return FeatureSet();
	}

	size_t remain = m_blockSize;
	size_t processed = 0;
	while (remain > 0) {
		size_t to_proc = std::min ((size_t)48, remain);
		_meter.process (&inputBuffers[0][processed], to_proc);
		processed += to_proc;
		remain -= to_proc;

		if (_meter.read () >= .89125 /* -1dBTP */) {
			long f = Vamp::RealTime::realTime2Frame (timestamp, m_rate);
			_above_m1.values.push_back ((float) (f + processed));
		}
	}

	return FeatureSet();
}

VampTruePeak::FeatureSet
VampTruePeak::getRemainingFeatures()
{
	FeatureSet returnFeatures;

	float m, p;
	_meter.read(m, p);

	Feature dbtp;
	dbtp.hasTimestamp = false;
	dbtp.values.push_back(p);
	returnFeatures[0].push_back(dbtp);

	_above_m1.hasTimestamp = false;
	returnFeatures[1].push_back(_above_m1);

	return returnFeatures;
}
